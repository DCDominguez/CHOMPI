#include <new>
#include "hardware.h"
#include "command_queue.h"
#include "engine.h"
#include "fatfs_storage.h"
#include "midi_framer.h"
#include "panel_controller.h"
#include "preset_menu.h"
#include "restart.h"
#include "runtime.h"
#include "sequence_store.h"
#include "sampler_runtime.h"
#include "usb_packets.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "util/CpuLoadMeter.h"

extern "C" { extern USBD_HandleTypeDef hUsbDeviceHS; }

using namespace daisy;
using namespace chompi;

namespace {
Hardware hw;
template<typename Transport> struct MidiPort {
    Transport transport;
    forge::MidiFramer framer;
    forge::SpscQueue<forge::MidiFrame, 16> frames;
    std::atomic<uint32_t> dropped{0}, clock_dropped{0};
#ifdef FORGE_TEST_HOOKS
    std::atomic<uint32_t> received{0};
#endif
    static void Receive(uint8_t* data, size_t size, void* context) {
        auto& self = *static_cast<MidiPort*>(context);
        forge::MidiFrame frame;
        for(size_t i = 0; i < size; ++i)
            if(self.framer.Feed(data[i], frame)) {
#ifdef FORGE_TEST_HOOKS
                self.received.fetch_add(1, std::memory_order_relaxed);
#endif
                if(!self.frames.Push(frame)) {
                    if(frame.kind == forge::MidiFrame::Kind::Clock) self.clock_dropped.fetch_add(1, std::memory_order_relaxed);   // a late tick, not a stuck note
                    else self.dropped.fetch_add(1, std::memory_order_relaxed);
                }
            }
    }
    void Listen() {
        if(!transport.RxActive()) {
            framer.Reset(); transport.FlushRx();
            transport.StartRx(Receive, this);
        }
    }
};
MidiPort<MidiUartTransport> uart_midi;
MidiPort<MidiUsbTransport> usb_midi;
CpuLoadMeter cpu;
UsbHandle usb_sender;
// libDaisy exposes the configured device and CDC state used by its MIDI mode.
// Keep TX memory alive until USB completion, and never rewrite it while busy.
// Largest reply (v5 status, 102 bytes with F0/F7) packs into 34 USB-MIDI events.
constexpr size_t kMaxEnvelope = forge::kMaxReply + 2;
uint8_t usb_tx_packets[((kMaxEnvelope + 2) / 3) * 4];
struct Outgoing { uint8_t source = 0; uint8_t bytes[kMaxEnvelope]{}; size_t size = 0; };
forge::SpscQueue<Outgoing, 32> outgoing; // producer and consumer both main loop
Outgoing pending;
bool has_pending = false;
uint32_t pending_since = 0;
// Objects whose default members are not all zero would carry a .data image in SRAM_EXEC
// (code space); they are built at start-up in zeroed storage instead (RESOURCE_LEDGER).
template<typename T> T& Construct() { alignas(T) static unsigned char storage[sizeof(T)]; return *new(storage) T(); }
forge::Engine& engine = Construct<forge::Engine>();
// Emergency count: main loop is the only writer; audio reads it each block.
std::atomic<uint32_t> emergency_epoch{0};
bool discard_ingress = false; // main-loop owned
forge::SpscQueue<forge::Request, 64> requests;
forge::SpscQueue<forge::Response, 64> responses;
// The bootloader does not zero SDRAM; Engine::Init clears these before audio.
constexpr size_t kDelayCapacity = 48002;
float DSY_SDRAM_BSS delay_left[kDelayCapacity];
float DSY_SDRAM_BSS delay_right[kDelayCapacity];
constexpr size_t kReverbCapacity = 8704; // >= Reverb::Required(48000) = 8606
// 34 KB in DTCM (zero-wait, uncached), as TAPE/TEMPO/WAVE place their reverbs.
// The FDN lines exceed the 16 KB D-cache, so SDRAM would mean cache misses.
// Not zeroed at boot; Reverb's unread counter hides stale cells until rewritten.
float __attribute__((section(".dtcmram_bss"))) reverb_memory[kReverbCapacity];
// Why CHOMPI last started (core/restart.h). Backup SRAM keeps these across a reset
// (not a power loss); NOLOAD, so they are validated by a magic number.
struct BootCount { uint32_t magic, boots; };
__attribute__((section(".backup_sram.forge"))) forge::restart::FaultRecord fault_record;   // after boot_info (0x38800000)
__attribute__((section(".backup_sram.forge"))) BootCount boot_count;
__attribute__((section(".backup_sram.forge"))) forge::restart::StartupGuard startup_guard;   // safe mode (0.15.2)
bool safe_mode = false;                        // this start skips the card's settings (core/restart.h)
uint32_t mount_ms = 0;                         // how long the start-up card mount took (RESTARTS.TXT)
uint8_t reset_flags = 0;                       // this start's RCC_RSR, compacted
forge::restart::FaultRecord last_fault{};      // the crash that caused this start, if any
// Forge's fault handler replaces libDaisy's (a breakpoint that freezes CHOMPI without a
// debugger) through a copy of the vector table: record where it crashed, then restart.
// The copy lives in DTCM: uncached, so the core's vector fetches see what was written
// without depending on D-cache maintenance (AXI SRAM is write-back cached).
alignas(1024) __attribute__((section(".dtcmram_bss"))) uint32_t vector_table[166];
extern "C" __attribute__((used)) void ForgeFaultRecord(const uint32_t* frame) {
    fault_record.pc = frame[6]; fault_record.lr = frame[5];
    fault_record.cfsr = SCB->CFSR; fault_record.hfsr = SCB->HFSR;
    fault_record.count = fault_record.Valid() ? fault_record.count + 1 : 1;
    fault_record.magic = forge::restart::FaultRecord::kMagic;
    forge::restart::CountCrash(startup_guard);
    __DSB();
    NVIC_SystemReset();
}
extern "C" __attribute__((naked)) void ForgeFaultHandler() {
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n b ForgeFaultRecord\n");
}
void InstallFaultHandler() {
    PWR->CR1 |= PWR_CR1_DBP;                               // backup SRAM writable
    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    const uint32_t* current = reinterpret_cast<const uint32_t*>(SCB->VTOR);
    for(unsigned i = 0; i < 166; ++i) vector_table[i] = current[i];
    vector_table[3] = reinterpret_cast<uint32_t>(&ForgeFaultHandler);   // HardFault (other faults escalate to it)
    __disable_irq();
    SCB->VTOR = reinterpret_cast<uint32_t>(vector_table);
    __DSB(); __ISB();
    __enable_irq();
}
// TAPE's warble line (8 KB): ordinary .bss, zeroed at start-up, so no image space.
float warble_memory[2 * forge::tape::Warble::kLength];
// FX section (0.20): the chorus line and cloud's diffuser, 33 KB of D2 SRAM (otherwise idle; the
// engine clears what it uses before reading it). Engine::FxFloats() = 4096 + 4236 floats.
constexpr size_t kFxFloats = 8448;
float __attribute__((section(".d2_bss"))) fx_memory[kFxFloats];
uint32_t dropped_commands = 0, rejected_messages = 0; // main-loop owned

// Device presets on the SD card (main loop only) and the TAPE-style panel menu
// (audio owner). Menu actions cross to the main loop with a parameter snapshot.
SdmmcHandler sdmmc;
FatFSInterface fsi;
FatFsStorage card;            // the card's settings, presets, loops and samples: not ready in safe mode
FatFsStorage install_card;    // the same volume for USB install and the restart log: usable in safe mode
forge::PresetStore store(card);
// One line per start in FORGE/RESTARTS.TXT on the card (kept under 16 KB).
FORGE_COLD void LogRestart() {
    if(!install_card.Ready()) return;
    char line[160];
    unsigned n = forge::restart::Describe(boot_count.boots, reset_flags, &last_fault, line, sizeof(line));
    if(n < sizeof(line)) {                         // start-up card timing (the owner's 60 s stall, 2026-10-10)
        char text[48] = "card mount ";
        unsigned k = 11; uint32_t v = mount_ms; char digits[11]; int d = 0;
        do { digits[d++] = static_cast<char>('0' + v % 10); v /= 10; } while(v && d < 10);
        while(d && k < sizeof(text) - 4) text[k++] = digits[--d];
        text[k++] = ' '; text[k++] = 'm'; text[k++] = 's'; text[k] = 0;
        n += forge::restart::DescribeEvent(boot_count.boots, text, line + n, sizeof(line) - n);
    }
    if(safe_mode && n < sizeof(line))
        n += forge::restart::DescribeEvent(boot_count.boots, "SAFE MODE after repeated start-up crashes: card settings, presets and samples skipped",
                                           line + n, sizeof(line) - n);
    f_mkdir("FORGE");
    static FIL file;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
    FILINFO info;
    const bool big = f_stat("FORGE/RESTARTS.TXT", &info) == FR_OK && info.fsize > 16384;
    if(f_open(&file, "FORGE/RESTARTS.TXT", big ? (FA_WRITE | FA_CREATE_ALWAYS) : (FA_WRITE | FA_OPEN_APPEND)) != FR_OK) return;
    UINT written = 0;
    f_write(&file, line, n, &written);
    f_close(&file);
}
forge::power::LevelCheck level_check;           // 0.19.2: the battery level (core/power.h)
// The charger readings behind the stock battery protection and the install check (core/power.h).
forge::power::Readings ChargerReadings() {
    forge::power::Readings r;
    r.battery_low = hw.batt_low_bounce; r.usb_good = hw.vin_gd_bounce;
    r.legacy = hw.legacy_cable_bounce; r.input_limit = hw.iindpm_stat_bounce;
    return r;
}
bool InstallPowerOk() {
    return forge::power::InstallPowerOk(static_cast<forge::power::Battery>(hw.GetBatteryLevel()), ChargerReadings());
}
// Just before the stock protection switches CHOMPI off or stops it, leave a line in
// FORGE/RESTARTS.TXT (once per kind per start): those shut-offs leave no reset flag.
FORGE_COLD void LogLockout(forge::power::Lockout lockout) {
    static uint8_t logged = 0;
    const uint8_t bit = static_cast<uint8_t>(1u << static_cast<unsigned>(lockout));
    if((logged & bit) || !install_card.Ready()) return;
    logged |= bit;
    char line[160];
    const unsigned n = forge::restart::DescribeEvent(boot_count.boots, forge::power::LockoutText(lockout), line, sizeof(line));
    static FIL file;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
    if(f_open(&file, "FORGE/RESTARTS.TXT", FA_WRITE | FA_OPEN_APPEND) != FR_OK) return;
    UINT written = 0;
    f_write(&file, line, n, &written);
    f_close(&file);
}
struct PanelAction { forge::MenuAction action; forge::Parameters patch; };
forge::SpscQueue<PanelAction, 8> panel_actions;   // audio -> main
std::atomic<uint32_t> menu_state{0};               // PresetMenu::Packed(), for LEDs
uint8_t last_bank = 0, last_slot = forge::panel::kNoSlot; // main-loop owned
// 0.17: the last recalled / saved preset, kept in FORGE/LAST.TXT ("bank slot", 0-based) and
// recalled at start-up (not in safe mode: the settings view is closed then).
uint8_t remembered_bank = 0xff, remembered_slot = 0xff;
uint32_t last_changed_at = 0;
FORGE_COLD void ReadLastPreset() {
    if(!card.Ready()) return;
    static FIL file;   // static: DMA-reachable (see LogRestart)
    if(f_open(&file, "FORGE/LAST.TXT", FA_READ) != FR_OK) return;
    char text[8] = {}; UINT got = 0;
    f_read(&file, text, sizeof(text) - 1, &got);
    f_close(&file);
    if(got >= 3 && text[0] >= '0' && text[0] < '0' + forge::kPresetBanks && text[1] == ' ') {
        const int slot = text[3] >= '0' && text[3] <= '9' ? (text[2] - '0') * 10 + (text[3] - '0') : text[2] - '0';
        if(slot >= 0 && slot < 15) { remembered_bank = static_cast<uint8_t>(text[0] - '0'); remembered_slot = static_cast<uint8_t>(slot); }
    }
}
// Written 2 s after the last change, so a burst of program changes writes once.
FORGE_COLD void WatchLastPreset() {
    static uint8_t seen_bank = 0xff, seen_slot = 0xff;
    if(last_slot == forge::panel::kNoSlot) return;
    if(last_bank != seen_bank || last_slot != seen_slot) { seen_bank = last_bank; seen_slot = last_slot; last_changed_at = System::GetNow(); }
    if((last_bank == remembered_bank && last_slot == remembered_slot) || System::GetNow() - last_changed_at < 2000 || !card.Ready()) return;
    f_mkdir("FORGE");
    static FIL file;
    if(f_open(&file, "FORGE/LAST.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) { remembered_bank = last_bank; remembered_slot = last_slot; return; }
    char text[6] = {static_cast<char>('0' + last_bank), ' ', 0, 0, '\n', 0}; unsigned n = 2;
    if(last_slot >= 10) text[n++] = static_cast<char>('0' + last_slot / 10);
    text[n++] = static_cast<char>('0' + last_slot % 10); text[n++] = '\n';
    UINT written = 0; f_write(&file, text, n, &written); f_close(&file);
    remembered_bank = last_bank; remembered_slot = last_slot;
}
uint32_t flash_until = 0; bool flash_ok = true;   // panel LED feedback after SD actions
std::atomic<uint32_t> audio_flash{0};              // audio -> main: 1 = ok, 2 = failed (record refused)

// Sampler (docs/forge/SAMPLING.md). SDRAM is not zeroed at boot; every read is
// bounded by a slot's published `loaded` count, so stale memory never plays.
constexpr uint32_t kPoolSamples = 32u * 1024 * 1024 / 2;      // 32 MiB: chromatic sample or kit bank (~174 s stereo)
constexpr uint32_t kRecordFrames = 16u * 1024 * 1024 / 4;     // 16 MB: ~87 s stereo at 48 kHz
int16_t DSY_SDRAM_BSS sample_pool[kPoolSamples];
int16_t DSY_SDRAM_BSS record_memory[2 * kRecordFrames];
// Looper (docs/forge/LOOPING.md): 4,000,000 stereo frames = ~83 s; reads stay below what was recorded.
constexpr uint32_t kLoopFrames = 4000000u;
int16_t DSY_SDRAM_BSS loop_memory[2 * kLoopFrames];
forge::Looper looper;                                         // audio owner
std::atomic<uint32_t> looper_state{0};                        // audio -> main: PackLooper, for the LEDs
std::atomic<uint32_t> chord_word{0}, chord_notes{0};          // audio -> main: Engine::ChordView (0.20 display)
uint8_t __attribute__((aligned(32))) sample_scratch[16384];   // D1 SRAM: reachable by SD DMA
forge::SampleTable& sample_table = Construct<forge::SampleTable>();
forge::SampleHandoff sample_handoff;
forge::SampleLoader& sample_loader = Construct<forge::SampleLoader>();                            // main loop
forge::Recorder recorder;                                     // audio owner (Unlock: main)
FatFsSampleFiles sample_files(card);
// USB file transfer and firmware install (core/file_transfer.h; main loop, gate shared with audio).
FatFsUploadFiles upload_files(install_card);   // recovery works in safe mode
forge::FileTransfer file_transfer;
forge::InstallGate install_gate;
std::atomic<uint32_t> sample_wanted{0};                      // audio -> main: PackSelection of the live patch
std::atomic<bool> recording_now{false};                       // audio -> main, for the CHOMPI LED
std::atomic<float> input_peak{0.f};                           // audio -> main: input meter (record position)
std::atomic<float> loop_speed{1.f}, loop_head{1.f};           // audio -> main: SW5's lights (target, head)
forge::SpscQueue<forge::SampleJob, 4> sample_jobs;            // audio -> main: panel save/erase/copy
uint8_t led_shadow[26][3];                                    // main loop: what the LEDs show now (probe page 1)
uint8_t view_leds[forge::kViewLeds][3];                       // main loop: every panel light, for the display (opcode 0D)

#ifdef FORGE_TEST_HOOKS
forge::InspectorMailbox& inspector_mailbox = Construct<forge::InspectorMailbox>();
forge::InspectorAudio& inspector_latest = Construct<forge::InspectorAudio>();
forge::InspectorSnapshot& inspector_snapshot = Construct<forge::InspectorSnapshot>();
forge::InspectorLog& inspector_log = Construct<forge::InspectorLog>();
forge::SpscQueue<forge::InspectorEvent,64> inspector_edges;
std::atomic<uint32_t> inspector_event_drops{0}, inspector_panel_drops{0}, inspector_sample_drops{0};
uint32_t inspector_tx[2]{}, inspector_tx_errors[2]{};
uint32_t inspector_storage_errors=0;
uint8_t inspector_last_storage_error=0;
// Battery reading instrumentation (core/battery_diag.h, Inspector page 11): observes the stock
// MP2722 reads and level check, changes nothing they do.
forge::power::BatteryMonitor battery_monitor;
// Read register 0x0C (the BATT_LOW threshold) back. Main loop only, and only when no stock DMA
// read is in flight: right after a stock write (MpWrite's blocking transmit waited for the read
// started before it, and nothing starts one in between) or with read_ready set (the last read has
// completed). libDaisy's blocking calls wait for the peripheral to be idle, then give up after 5 ms.
// The stock read sets the register pointer back to 0x11 before every read.
FORGE_COLD void ReadBackThreshold(forge::power::ReadBack kind) {
    uint8_t reg = forge::power::kThresholdRegister, value = 0;
    const bool ok = hw.i2c.TransmitBlocking(0x3f, &reg, 1, 5) == I2CHandle::Result::OK
                 && hw.i2c.ReceiveBlocking(0x3f, &value, 1, 5) == I2CHandle::Result::OK;
    battery_monitor.ReadBackDone(kind, ok, value);
}
FORGE_COLD void FillBattery(forge::power::BatteryDiag& b) {
    b = battery_monitor.Diag();
    b.uptime_ms = System::GetNow();
    for(unsigned i = 0; i < 6; ++i) b.reg[i] = hw.mp_buff_[i];
    b.battery_low = hw.batt_low_bounce; b.usb_good = hw.vin_gd_bounce;
    b.legacy = hw.legacy_cable_bounce; b.input_limit = hw.iindpm_stat_bounce;
    b.level = static_cast<uint8_t>(hw.GetBatteryLevel()); b.check_state = static_cast<uint8_t>(level_check.GetState());
    b.read_ready = hw.read_ready;
}
void InspectorEvent(forge::InspectorEventKind kind, uint8_t id, uint32_t value) {
    inspector_log.Add({0,System::GetNow(),value,kind,id});
}
void InspectorStorageError(forge::Error error) {
    if(error==forge::Error::None) return;
    ++inspector_storage_errors; inspector_last_storage_error=static_cast<uint8_t>(error);
    InspectorEvent(forge::InspectorEventKind::StorageError,0,static_cast<uint8_t>(error));
}
#endif

// Panel work for the main loop (audio callback is the producer).
struct FirmwarePanelSink : forge::PanelSink {
    bool PresetAction(const forge::MenuAction& action, const forge::Parameters& snapshot) override {
        const bool ok=panel_actions.Push({action, snapshot});
#ifdef FORGE_TEST_HOOKS
        if(!ok) inspector_panel_drops.fetch_add(1,std::memory_order_relaxed);
#endif
        return ok;
    }
    bool SampleJob(const forge::SampleJob& job) override {
        const bool ok=sample_jobs.Push(job);
#ifdef FORGE_TEST_HOOKS
        if(!ok) inspector_sample_drops.fetch_add(1,std::memory_order_relaxed);
#endif
        return ok;
    }
    void Flash(bool ok) override { audio_flash.store(ok ? 1 : 2, std::memory_order_relaxed); }
};
forge::PanelController& panel_controller = Construct<forge::PanelController>();                      // audio owner

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    cpu.OnBlockStart();
    forge::Request request;
    static forge::RecoveryGate recovery; // audio-owner only
    recovery.Observe(static_cast<uint8_t>(emergency_epoch.load(std::memory_order_acquire)), engine);
    // Backpressure instead of applying a patch whose acknowledgement cannot
    // be queued. Only this audio callback produces responses.
    // Sampler memory handoff: refuse file-slot notes while the loader rewrites them.
    engine.SetSampleFilesAvailable(sample_handoff.AudioBlock(engine));
    for(unsigned i = 0; i < 16 && responses.HasSpace() && requests.Pop(request); ++i) {
        if(!recovery.Admit(request, engine)) continue;
        forge::Response response;
        if(request.kind == forge::RequestKind::SampleJob) {     // host save: lock the take first
            responses.Push(forge::LockForSave(recorder, request));
            continue;
        }
        if(request.kind == forge::RequestKind::Panel) {         // development: injected panel event
            forge::PanelEvent event;
            event.kind = static_cast<forge::PanelEvent::Kind>(request.panel_kind);
            event.id = request.panel_id; event.value = request.panel_value;
            panel_controller.Inject(event);
            continue;
        }
        if(forge::ExecuteRequest(request, engine, response)) {
            response.cpu_average = cpu.GetAvgCpuLoad();
            response.cpu_max = cpu.GetMaxCpuLoad();
            if(responses.Push(response)) forge::StoreQueued(response, engine);
            if(request.kind == forge::RequestKind::Status && request.reset_cpu) cpu.Reset();   // audio owns the meter
        }
    }

    hw.ProcessAllControls();
    // The whole panel (menus, record gesture, keys, knobs) lives in
    // core/panel_controller.h; development builds merge injected events.
    static FirmwarePanelSink sink;
    forge::PanelInput input;
    input.frames = static_cast<uint16_t>(size);
    for(unsigned key = 0; key < forge::panel::kButtons; ++key)
        if(hw.button_sr.State(key)) input.keys |= uint64_t(1) << key;
    input.toggle_up = hw.GetToggleState();
    input.jack = hw.jack_detect.Read();
    input.tone_down = hw.enc[forge::panel::kToneEncoder].Pressed();
    for(unsigned i = 0; i < forge::panel::kEncoders; ++i) input.turns[i] = static_cast<int16_t>(hw.enc[i].Increment());
#ifdef FORGE_TEST_HOOKS
    const uint32_t inspector_now=System::GetNow();
    panel_controller.SetInspectorEvents(&inspector_edges,&inspector_event_drops,inspector_now);
#endif
    panel_controller.Block(input, engine, recorder, sink);
    engine.Block(static_cast<unsigned>(size));   // clock, arp and bass steps (0.14)
    const forge::RecordSource source = panel_controller.Source();
    sample_wanted.store(forge::PackSelection(engine.GetParameters()), std::memory_order_relaxed);
    recording_now.store(recorder.Recording(), std::memory_order_relaxed);
    menu_state.store(panel_controller.MenuPacked(), std::memory_order_relaxed);
    looper_state.store(forge::PackLooper(looper, engine.FxBeforeLoop()), std::memory_order_relaxed);
    if(const auto* h = engine.Harmony()) {                       // 0.20 display: only when a new chord sounded
        static uint32_t seen = ~0u;
        if(h->Changes() != seen) {
            seen = h->Changes(); uint32_t word, notes; engine.ChordView(word, notes);
            chord_notes.store(notes, std::memory_order_relaxed); chord_word.store(word, std::memory_order_release);
        }
    }
    loop_speed.store(looper.Speed(), std::memory_order_relaxed); loop_head.store(looper.HeadSpeed(), std::memory_order_relaxed);

    const bool recording = recorder.Recording();
    // TAPE's monitor positions (menu SW6 press, options.json): Headphones = in the record
    // position the dry input goes to the headphones; Both = the input always goes through
    // the effects and looper to both outputs; SendReturn = the mic through the effects in
    // the record position, line in always back to the headphones. The meter follows the input.
    const bool record_position = panel_controller.RecordPosition();
    const forge::panel::MonitorMode mode = panel_controller.Monitor();
    const bool monitor = record_position || recording || mode != forge::panel::MonitorMode::Headphones;
    float peak = 0.f;
    for(size_t i = 0; i < size; ++i) {
        float left, right, rec_l = 0.f, rec_r = 0.f, pre_l = 0.f, pre_r = 0.f;
        bool post = false;                               // dry input into the headphones after the effects
        if(monitor) {
            recorder.Input(source, in[0][i], in[2][i], in[3][i], 0.f, 0.f, rec_l, rec_r);
            if(source != forge::RecordSource::Resample) {
                const bool mic = source == forge::RecordSource::Mic;
                if(mode == forge::panel::MonitorMode::Both) { pre_l = rec_l; pre_r = rec_r; }
                else if(mode == forge::panel::MonitorMode::SendReturn) {
                    if(mic && record_position) { pre_l = rec_l; pre_r = rec_r; } else if(!mic) post = true;
                } else post = record_position;
                peak = std::fmax(peak, std::fmax(std::fabs(rec_l), std::fabs(rec_r)));
            }
        }
        // Upstream channel map: mic 0, aux L/R = 2/3 (aux feeds the delay route).
        engine.Process(in[2][i], in[3][i], left, right, 0.5f * pre_l, 0.5f * pre_r);
        if(recording) {
            if(source == forge::RecordSource::Resample) recorder.Input(source, 0.f, 0.f, 0.f, left, right, rec_l, rec_r);
            recorder.Write(rec_l, rec_r);
        }
        float hp_l = left, hp_r = right;
        if(post) { hp_l = forge::Clamp(left + 0.5f * rec_l, -1.f, 1.f); hp_r = forge::Clamp(right + 0.5f * rec_r, -1.f, 1.f); }
        out[0][i] = hp_l; out[1][i] = hp_r;              // headphones
        out[2][i] = left; out[3][i] = right;             // main (line) out
    }
    input_peak.store(peak, std::memory_order_relaxed);
#ifdef FORGE_TEST_HOOKS
    static uint32_t block=0, revision=0, selection=0; static bool was_recording=false;
    ++block;
    auto edge=[&](forge::InspectorEventKind kind,uint32_t value) {
        if(!inspector_edges.Push({0,inspector_now,value,kind,0})) inspector_event_drops.fetch_add(1,std::memory_order_relaxed);
    };
    engine.ObserveVoiceEdges(inspector_edges,inspector_event_drops,inspector_now);
    if(engine.PatchRevision()!=revision) { revision=engine.PatchRevision(); edge(forge::InspectorEventKind::PatchApply,revision); }
    const uint32_t live=forge::PackSelection(engine.GetParameters());
    if(live!=selection) { selection=live; edge(forge::InspectorEventKind::SampleSelection,live); }
    if(recorder.Recording()!=was_recording) {
        was_recording=recorder.Recording();
        edge(was_recording?forge::InspectorEventKind::RecordingStart:forge::InspectorEventKind::RecordingStop,recorder.Length());
    }
    if(auto* a=inspector_mailbox.AudioBegin()) {
        engine.Inspect(*a); panel_controller.Inspect(*a);
        a->block=block; a->time_ms=inspector_now; a->record_frames=recorder.Length();
        a->recording=recorder.Recording(); a->locked=recorder.Locked();
        // Completed earlier callbacks only; mirror work is included in this callback's meter.
        a->cpu_average=cpu.GetAvgCpuLoad(); a->cpu_peak=cpu.GetMaxCpuLoad();
        inspector_mailbox.AudioEnd();
    }
#endif
    cpu.OnBlockEnd();
}

void Send(uint8_t source, uint8_t* envelope, size_t payload_size) {
    Outgoing message;
    message.source = source; message.size = payload_size + 2;
    envelope[0] = 0xf0; envelope[payload_size + 1] = 0xf7;
    std::copy(envelope, envelope + message.size, message.bytes);
    if(!outgoing.Push(message)) ++dropped_commands;
}

// TAPE's per-slot sample settings in presets.json (core/slot_settings.h), shared with TAPE.
// Read at start-up and when a card goes in; written by the main loop 2 s after the last
// change (TAPE's own way: presets_temp.json, then a rename), once FORGE/presets_backup.json
// holds the card's original file.
forge::SlotSettings slot_settings;
forge::harmony::Player harmony_player;   // harmony mode (core/harmony.h): zero-initialised, audio owner
forge::parts::Parts& parts = Construct<forge::parts::Parts>();   // clock, arp, bass (core/parts.h, 0.14): audio owner
forge::seq::Sequencer& sequencer = Construct<forge::seq::Sequencer>();   // event recorder (0.15): audio owner
// Its save / load handoff lives in SDRAM (built in main() once SDRAM runs); the file buffer is
// main-loop only, in whole cache lines for the SD driver's DMA.
alignas(8) unsigned char DSY_SDRAM_BSS sequence_mailbox_memory[sizeof(forge::seq::Mailbox)];
forge::seq::Mailbox* sequence_mailbox = nullptr;
alignas(32) uint8_t sequence_file[forge::seq::kFileBuffer];
// A recalled preset's loop (if it has one) follows its patch to the audio owner.
FORGE_COLD void LoadSequence(uint8_t bank, uint8_t slot);
alignas(32) char presets_text[forge::SlotSettings::kFileMax];   // whole cache lines: SD DMA reads
uint32_t slot_settings_failures = 0;
FORGE_COLD void LoadSlotSettings() {
    slot_settings.Clear();
    if(!card.Ready()) return;
    static FIL file; UINT read = 0;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
    if(f_open(&file, "presets.json", FA_READ) != FR_OK && f_open(&file, "presets_old.json", FA_READ) != FR_OK) return;   // a write cut short
    f_read(&file, presets_text, sizeof(presets_text) - 1, &read);
    f_close(&file);
    presets_text[read] = 0;
    slot_settings.Parse(presets_text, read);
}
FORGE_COLD bool CopyFile(const char* from, const char* to) {
    static FIL in, out;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
    if(f_open(&in, from, FA_READ) != FR_OK) return false;
    bool ok = f_open(&out, to, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK;
    if(ok) {
        for(UINT read = 1; ok && read;) {
            UINT written = 0;
            ok = f_read(&in, presets_text, sizeof(presets_text), &read) == FR_OK
                 && (!read || (f_write(&out, presets_text, read, &written) == FR_OK && written == read));
        }
        ok = f_close(&out) == FR_OK && ok;
    }
    f_close(&in);
    return ok;
}
FORGE_COLD bool WritePresetsFile() {
    FILINFO info;
    if(f_stat("FORGE/presets_backup.json", &info) != FR_OK && f_stat("presets.json", &info) == FR_OK) {
        f_mkdir("FORGE");
        if(!CopyFile("presets.json", "FORGE/presets_backup.json")) return false;   // never write without the backup
    }
    const size_t n = slot_settings.Write(presets_text, sizeof(presets_text));
    if(!n) return false;
    static FIL file; UINT written = 0;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
    if(f_open(&file, "presets_temp.json", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    const bool ok = f_write(&file, presets_text, n, &written) == FR_OK && written == n;
    if(f_close(&file) != FR_OK || !ok) { f_unlink("presets_temp.json"); return false; }
    // FatFS rename does not replace: the old file steps aside first, so a power cut leaves
    // presets.json or presets_old.json (LoadSlotSettings reads either; 0.15.1).
    f_unlink("presets_old.json");
    if(f_stat("presets.json", &info) == FR_OK && f_rename("presets.json", "presets_old.json") != FR_OK) {
        f_unlink("presets_temp.json"); return false;
    }
    if(f_rename("presets_temp.json", "presets.json") != FR_OK) { f_rename("presets_old.json", "presets.json"); return false; }
    f_unlink("presets_old.json");
    return true;
}
void SaveSlotSettings(uint32_t now) {
    static forge::SlotSettingsSchedule schedule;
    if(!schedule.Due(slot_settings, now) || sample_loader.Busy() || !card.Ready()) return;
    slot_settings.TakeDirty();
    if(!WritePresetsFile()) { ++slot_settings_failures; schedule.Failed(slot_settings, now); }   // tried again later
}
// TAPE's options.json (core/options.h), read once before audio starts; Forge never writes it.
// Start-up step timing for the Inspector (development): "card" events id 3+ = step, value = ms.
inline void BootStep(uint8_t id, uint32_t& since) {
#ifdef FORGE_TEST_HOOKS
    const uint32_t now = System::GetNow(); InspectorEvent(forge::InspectorEventKind::Card, id, now - since); since = now;
#else
    (void)id; (void)since;
#endif
}
forge::Options boot_options;                   // read with the other start-up card files, applied later
FORGE_COLD void ReadOptions() {
    forge::Options& o = boot_options;
    o = forge::Options{};
    if(card.Ready()) {
        alignas(32) static char text[1024];     // whole cache lines: the SD driver invalidates around DMA reads
        static FIL file; UINT read = 0;   // static: a FIL holds a DMA sector buffer and the stack is DTCM (out of the SD DMA's reach)
        if(f_open(&file, "options.json", FA_READ) == FR_OK) {
            f_read(&file, text, sizeof(text), &read);
            f_close(&file);
            o = forge::options::Parse(text, read);
        }
    }
}
FORGE_COLD void LoadOptions() {
    const forge::Options& o = boot_options;
    panel_controller.SetOptions(o);
    looper.SetTapeSlew(o.tape_slew);
    engine.SetSplitDelay(o.split_delay);
}
// The panel's MIDI out (TAPE): UART and USB, separate from SysEx replies. A USB message
// waits up to 20 ms for the endpoint (a lost note-off would leave a note stuck elsewhere).
void SendPanelMidi() {
    static forge::MidiOut usb_pending; static bool usb_waiting = false; static uint32_t usb_since = 0;
    for(unsigned n = 0; n < 8; ++n) {
        if(usb_waiting) {
            bool sent = false;
            {
                ScopedIrqBlocker guard;
                if(hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceHS.pClassData) {
                    auto* cdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceHS.pClassData);
                    if(cdc->TxState == 0) {
                        static uint8_t packet[4];
                        forge::PackUsbChannel(usb_pending.status, usb_pending.data1, usb_pending.data2, packet);
                        sent = usb_sender.TransmitExternal(packet, 4) == UsbHandle::Result::OK;
                    }
                } else sent = true;                      // no USB host: nothing to wait for
            }
            if(!sent && System::GetNow() - usb_since < 20) return;
            usb_waiting = false;
        }
        forge::MidiOut m;
        if(!panel_controller.PopMidi(m)) {
            if(!parts.PopMidi(m)) return;             // the parts (0.14): arp channel + 0, bass + 1
            if(m.status < 0xf0) m.status = static_cast<uint8_t>((m.status & 0xf0) | ((panel_controller.GetOptions().midi_out + (m.status & 15u)) & 15u));
        }
        const uint8_t bytes[3] = {m.status, m.data1, m.data2};
        const uint16_t length = m.status >= 0xf8 ? 1 : 3;   // real-time: status only
        uart_midi.transport.GetUartHandle().BlockingTransmit(const_cast<uint8_t*>(bytes), length, 3);
        usb_pending = m; usb_waiting = true; usb_since = System::GetNow();
    }
}
// 0.19.2: Forge's battery level check (core/power.h LevelCheck) in place of the stock
// BMCMediumBattCheck. Blocking I2C, main loop only, called only with no stock DMA read in flight.
// The stock read sets the register pointer back to 0x11 before every read.
struct ChargerIo {
    bool Write(uint8_t value) {
        uint8_t tx[2] = {0x0c, value}, reg = 0x0c, back = 0;
        const bool ok = hw.i2c.TransmitBlocking(0x3f, tx, 2, 5) == I2CHandle::Result::OK
                     && hw.i2c.TransmitBlocking(0x3f, &reg, 1, 5) == I2CHandle::Result::OK
                     && hw.i2c.ReceiveBlocking(0x3f, &back, 1, 5) == I2CHandle::Result::OK;
#ifdef FORGE_TEST_HOOKS
        if(value != forge::power::LevelCheck::k31)
            battery_monitor.ReadBackDone(value == forge::power::LevelCheck::k33 ? forge::power::ReadBack::After33
                                                                               : forge::power::ReadBack::After30, ok, back);
#endif
        return ok && back == value;
    }
    bool Low(bool& low) {
        uint8_t reg = 0x16, value = 0;
        const bool ok = hw.i2c.TransmitBlocking(0x3f, &reg, 1, 5) == I2CHandle::Result::OK
                     && hw.i2c.ReceiveBlocking(0x3f, &value, 1, 5) == I2CHandle::Result::OK;
        low = (value >> 4) & 1u;
        return ok;
    }
};
ChargerIo charger_io;
void DrawLeds(); void SendResponses(); void TransmitPending(); void RunSampler();
// TAPE's USB/charger hand-over after a power event (plug/unplug, charge state).
forge::power::ChargerUsb charger_usb;
bool usb_lines_to_daisy = true;
void ServiceChargerUsb(uint32_t now) {
    const auto a = charger_usb.Poll(now, hw.mpc_int.Read(), hw.read_ready, hw.mp_buff_[0], hw.mp_buff_[1]);
    if(a.read) {
        level_check.Abort(now, charger_io);                 // its read must compare against 3.0 V
#ifdef FORGE_TEST_HOOKS
        battery_monitor.ReadStarted();
#endif
        hw.MpReadAll();
    }
    if(a.usb_to_daisy >= 0) { usb_lines_to_daisy = a.usb_to_daisy != 0; hw.usb_sw.Write(usb_lines_to_daisy); }
    if(a.force_detection) hw.MpWrite(0x0a, 0B00110100);   // FORCEDPDM (as TAPE)
}
// Firmware install confirmed on the panel: finish sending the replies, then
// restart; the bootloader finds the new FORGE.bin on the card and flashes it.
[[noreturn]] void Restart() {
    file_transfer.Abort(upload_files);
    DrawLeds();
    uint32_t start = System::GetNow();
    while(sample_loader.Busy() && System::GetNow() - start < 5000) RunSampler();   // never cut a sample save short
    if(slot_settings.TakeDirty() && card.Ready()) WritePresetsFile();              // nor the last knob turns
    start = System::GetNow();
    while(System::GetNow() - start < 300) { TransmitPending(); SendResponses(); }
    NVIC_SystemReset();
    while(true) {}
}

void TransmitPending() {
    if(!has_pending) {
        if(!outgoing.Pop(pending)) return;
        has_pending = true; pending_since = System::GetNow();
    }
    bool sent = false;
    if(pending.source == 0) {
        // 0.32 ms per byte at 31250 baud (83-byte v3 reply: 26.6 ms) plus 5 ms
        // margin; the upstream PollTx wrapper's 10 ms timeout is too short.
        // Audio remains interrupt-driven while this blocks the main loop.
        const uint32_t timeout_ms = static_cast<uint32_t>((pending.size * 320 + 999) / 1000 + 5);
        sent = uart_midi.transport.GetUartHandle().BlockingTransmit(pending.bytes, pending.size, timeout_ms)
            == UartHandler::Result::OK;
        if(!sent) {
#ifdef FORGE_TEST_HOOKS
            ++inspector_tx_errors[0];
#endif
            ++dropped_commands; has_pending = false;
        }
    } else {
        ScopedIrqBlocker guard;
        if(hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceHS.pClassData) {
            auto* cdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceHS.pClassData);
            if(cdc->TxState == 0) {
                const size_t size = forge::PackUsbSysEx(pending.bytes, pending.size,
                                                       usb_tx_packets, sizeof(usb_tx_packets));
                sent = size && usb_sender.TransmitExternal(usb_tx_packets, size) == UsbHandle::Result::OK;
            }
        }
    }
    if(sent) {
#ifdef FORGE_TEST_HOOKS
        ++inspector_tx[pending.source];
#endif
        has_pending = false;
    }
    else if(System::GetNow() - pending_since > 100) {
#ifdef FORGE_TEST_HOOKS
        ++inspector_tx_errors[pending.source];
#endif
        ++dropped_commands; has_pending = false;
    }
}

void RaiseEmergency() {
    emergency_epoch.store(emergency_epoch.load(std::memory_order_relaxed) + 1, std::memory_order_release);
}
bool Queue(forge::Request& request) {
    request.epoch = static_cast<uint8_t>(emergency_epoch.load(std::memory_order_relaxed));
    return requests.Push(request);
}

// Battery and charger, for the status reply (every build) and Inspector page 2.
void ReadPower(uint8_t& battery, uint8_t& flags, uint8_t& charge) {
    const auto power = forge::power::DecodeStatus(hw.mp_buff_, static_cast<forge::power::Battery>(hw.GetBatteryLevel()),
                                                  charger_usb.Handover() || !usb_lines_to_daisy);
    battery = static_cast<uint8_t>(power.level);
    flags = static_cast<uint8_t>((power.usb_power ? 1 : 0) | (power.fault ? 2 : 0) | (power.usb_to_charger ? 4 : 0)
                                 | forge::power::SupplyFlags(power.level, ChargerReadings()));
    charge = power.charge_state;
}
void SendResponse(forge::Response response) {
    if(response.kind == forge::ResponseKind::Status) ReadPower(response.battery, response.power_flags, response.charge_state);
    const uint32_t dropped = dropped_commands + uart_midi.dropped.load(std::memory_order_relaxed)
        + usb_midi.dropped.load(std::memory_order_relaxed);
    uint8_t envelope[kMaxEnvelope];
    const size_t size = forge::EncodeResponse(response, dropped, rejected_messages, envelope + 1);
    Send(response.source, envelope, size);
}
void Flash(bool ok) { flash_ok = ok; flash_until = System::GetNow() + 400; }
// Loads a slot and queues it for the audio owner. Silent requests (panel,
// program change) get no reply; a host recall is acknowledged with status.
FORGE_COLD forge::Error RecallPreset(forge::Request& request) {
    forge::Request apply;
    const forge::Error error = forge::RecallRequest(store, request, apply);
    if(error != forge::Error::None) { if(request.silent) Flash(false); return error; }
    if(!Queue(apply)) { ++dropped_commands; return forge::Error::Busy; }
    last_bank = request.bank; last_slot = request.slot;
    LoadSequence(request.bank, request.slot);
    return forge::Error::None;
}
FORGE_COLD void LoadSequence(uint8_t bank, uint8_t slot) {
    if(!sequence_mailbox || !forge::seq::LoadFile(card, *sequence_mailbox, bank, slot, sequence_file)) return;
    forge::Request load; load.kind = forge::RequestKind::SequenceLoad;
    if(!Queue(load)) { ++dropped_commands; sequence_mailbox->Done(); }
}
// Host device-preset requests. Store goes on to the audio owner for a
// snapshot; recall is replaced by the loaded patch; erase/list reply here.
forge::Error HandleStorage(forge::Request& request) {
    using forge::RequestKind;
    if(request.kind == RequestKind::Recall) {
        forge::Request apply;
        const forge::Error error = forge::RecallRequest(store, request, apply);
        // The project's loop is queued after the patch (HandleMessage, 0.15.1).
        if(error == forge::Error::None) { last_bank = request.bank; last_slot = request.slot; request = apply; }
        return error;
    }
    if(request.kind == RequestKind::Erase) {
        const forge::Response reply = forge::EraseReply(store, request);
        if(reply.error == forge::Error::None) forge::seq::EraseFile(card, request.bank, request.slot);
        if(reply.error == forge::Error::None) SendResponse(reply);
        return reply.error;
    }
    if(request.kind == RequestKind::List) {
        const forge::Response reply = forge::ListReply(store, request);
        if(reply.error == forge::Error::None) SendResponse(reply);
        return reply.error;
    }
    return forge::Error::None;
}
// Panel menu actions (queued by the audio callback).
FORGE_COLD void RunPanelActions() {
    PanelAction item;
    while(panel_actions.Pop(item)) {
        const forge::MenuAction& a = item.action;
        forge::Error error = forge::Error::None;
        switch(a.kind) {
            case forge::MenuAction::Kind::Recall: {
                forge::Request request; request.kind = forge::RequestKind::Recall;
                request.bank = a.bank; request.slot = a.slot; request.silent = true;
                RecallPreset(request);
                continue;   // RecallPreset flashes on failure; success shows as the white key
            }
            case forge::MenuAction::Kind::Save:   // a project: the preset and its recorded loop
                error = store.Save(a.bank, a.slot, item.patch);
                if(sequence_mailbox) {
                    const forge::Error e = forge::seq::SaveFile(card, *sequence_mailbox, a.bank, a.slot, sequence_file, error == forge::Error::None);
                    if(error == forge::Error::None && e == forge::Error::Storage) error = e;
                }
                // 0.19.2: a saved preset is also the one CHOMPI wakes up with (FORGE/LAST.TXT), as recalls are.
                if(error == forge::Error::None) { last_bank = a.bank; last_slot = a.slot; }
                break;
            case forge::MenuAction::Kind::Erase:
                error = store.Erase(a.bank, a.slot);
                if(error == forge::Error::None) forge::seq::EraseFile(card, a.bank, a.slot);
                break;
            case forge::MenuAction::Kind::Copy:
                error = store.Copy(a.bank, a.slot, a.to_bank, a.to_slot);
                if(error == forge::Error::None && !forge::seq::CopyFile(card, a.bank, a.slot, a.to_bank, a.to_slot, sequence_file)) error = forge::Error::Storage;
                break;
            default: continue;   // sample actions never reach this queue (handled in the audio callback)
        }
#ifdef FORGE_TEST_HOOKS
        InspectorStorageError(error);
#endif
        Flash(error == forge::Error::None);
    }
}
// Sampler main-loop work: panel jobs, one loader step, job replies.
FORGE_COLD void RunSampler() {
    for(forge::SampleJob job; sample_jobs.Pop(job);) {
        job.source = 0xff;
        if(!sample_loader.Queue(job)) {
            if(job.kind == forge::SampleJob::Kind::Save) { if(job.from_loop) looper.Unlock(); else recorder.Unlock(); }
            Flash(false);
        }
    }
    forge::SampleEvent event;
    if(sample_loader.Poll(sample_files, sample_wanted.load(std::memory_order_relaxed), record_memory, event, loop_memory)) {
#ifdef FORGE_TEST_HOOKS
        InspectorEvent(forge::InspectorEventKind::SampleJobDone,static_cast<uint8_t>(event.job.kind),event.ok?1:0);
#endif
        if(event.job.kind == forge::SampleJob::Kind::Save) { if(event.job.from_loop) looper.Unlock(); else recorder.Unlock(); }
        forge::FollowSampleJob(slot_settings, event);
        if(event.job.source == 0xff) Flash(event.ok);
        else {
            const forge::Response reply = forge::SampleDoneReply(event);
            if(reply.error != forge::Error::None) ++rejected_messages;
            SendResponse(reply);
        }
    }
    const uint32_t flash = audio_flash.exchange(0, std::memory_order_relaxed);
    if(flash) Flash(flash == 1);
}
// Key LEDs (~30 Hz), the knob page LEDs (1-4), PLAY/LOOP (7/8) and panel LED 0 (the CHOMPI key, as in TAPE): red while
// recording, a flash after an action, pink blink while saving/copying. The
// composed colours are kept for the development probe.
// White balance: CHOMPI's LEDs show full white as light blue (the owner, 2026-10-05; TAPE drives
// them the same way), so green and blue are trimmed. Applied only at the LED driver;
// the composed colours (tests, development probe) stay TAPE's.
constexpr float kBalanceG = .85f, kBalanceB = .6f;
forge::Rgb Balance(forge::Rgb c) { return forge::Rgb{c.r, c.g * kBalanceG, c.b * kBalanceB}; }
// 0.20: the through-hole LEDs (knob rings, CHOMPI, PLAY / LOOP, transport, SW6) show green about 30 %
// weaker than the keys' SMT LEDs (measured on the owner's CHOMPI, 2026-10-13: ring orange looked red,
// yellow orange, white magenta), so their red and blue are trimmed to match the keys. .74 overshot on
// camera (bright orange looked yellow, dim yellow green): .86, eased in with brightness (dim through-hole
// LEDs lose red on their own: full trim from half brightness up, none at off).
constexpr float kPthRedBlue = .86f;
constexpr forge::Rgb kSafeModeColour{1.f, 0.f, 1.f};
void Pth(unsigned led, forge::Rgb c) {
    c = Balance(c);
    const float level = std::fmax(c.r, std::fmax(c.g, c.b)), k = 1.f - (1.f - kPthRedBlue) * std::fmin(1.f, 2.f * level);
    SetPthLedFloat(led, c.r * k, c.g, c.b * k);
}
FORGE_COLD void DrawLeds() {
    static uint32_t last_draw = 0;
    const uint32_t now = System::GetNow();
    if(now - last_draw < 33) return;
    last_draw = now;
    forge::LedView view;
    view.menu = menu_state.load(std::memory_order_relaxed);
    view.preset_occupancy = store.Occupancy((view.menu >> 4) & 7u); view.preset_card = store.Ready();
    view.last_bank = last_bank; view.last_slot = last_slot;
    view.sample_occupancy = sample_loader.Occupancy((view.menu >> 22) & 1u, (view.menu >> 4) & 7u);
    view.sample_card = sample_files.Ready();
    view.recording_present = sample_table.slots[forge::kRamSlot].loaded.load(std::memory_order_acquire) > 0;
    view.recording_now = recording_now.load(std::memory_order_relaxed);
    view.live = sample_wanted.load(std::memory_order_relaxed);
    view.keys_down = panel_controller.KeysDown();
    view.kit_occupancy = sample_loader.Occupancy(1, (view.live >> 2) & 7u);
    view.harmony = panel_controller.HarmonyLights();
    view.parts = panel_controller.PartsLights(); view.parts_clock = panel_controller.PartsClock();
    view.sequence = panel_controller.SequenceLights();
    view.magic = panel_controller.MagicLights();
    view.dream = panel_controller.DreamLights();
    view.synth = panel_controller.SynthLights();
    view.fx = panel_controller.FxLights();
    view.dream_level = .5f + .5f * std::sin(now * (6.2831853f / 4000.f));   // a 4 s breath
    view.dream_colour = panel_controller.DreamColour();
    {   // 0.19.2 Dream: each key it plays glows and fades over ~1.5 s, leaving trails
        static float glow[25] = {}; static uint32_t last_draw = 0;
        const float fade = std::exp(-static_cast<float>(now - last_draw) / 600.f); last_draw = now;
        const bool show = (view.dream >> 26) & 1u;
        for(unsigned i = 0; i < 25; ++i) { glow[i] = show && ((view.dream >> i) & 1u) ? 1.f : glow[i] * fade; view.dream_glow[i] = glow[i]; }
    }
    view.blink = (now / 250) % 2 == 0; view.slow_blink = (now / 300) % 2 != 0;
    view.flash = now < flash_until ? (flash_ok ? 1 : 0) : -1;
    view.saving = sample_loader.Busy() && !sample_loader.Loading();
    view.looper = looper_state.load(std::memory_order_relaxed);
    {   // 0.18: a loop (looper first take, or a recorded take) closing starts a 0.5 s ripple
        static uint32_t last_looper = 0, last_sequence = 0, ripple_at = 0;
        const uint32_t looper_state = view.looper & 7u, sequence_state = view.sequence & 7u;
        if((last_looper == static_cast<uint32_t>(forge::Looper::State::FirstTake) && looper_state == static_cast<uint32_t>(forge::Looper::State::Playing))
           || (last_sequence == static_cast<uint32_t>(forge::seq::Sequencer::State::Recording) && sequence_state == static_cast<uint32_t>(forge::seq::Sequencer::State::Playing)))
            ripple_at = now ? now : 1;
        last_looper = looper_state; last_sequence = sequence_state;
        if(ripple_at && now - ripple_at < 500) view.ripple = static_cast<uint8_t>(1 + (now - ripple_at) * 254 / 500);
        else ripple_at = 0;
    }
    view.install = install_gate.Confirmed() ? 2 : install_gate.Armed() ? 1 : 0;
    view.record_position = panel_controller.RecordPosition();
    view.count_in = panel_controller.CountIn();
    static float meter = 0.f;                                    // TAPE-like VU: fast up, slow down
    meter = std::fmax(input_peak.load(std::memory_order_relaxed), meter * 0.85f);
    view.input_level = meter;
    forge::Rgb keys[25], chompi, play, loop;
    forge::ComposeLeds(view, keys, chompi);
    forge::ComposeLooperLeds(view.looper, view.blink, play, loop);
    if(view.record_position) { play = forge::knobs::Scale(play, .7f); loop = forge::knobs::Scale(loop, .7f); }  // TAPE kRecDim
    Pth(forge::panel::kPlayLed, play); Pth(forge::panel::kLoopLed, loop);
    forge::Rgb knobs[4], reverse, forward;
    const uint32_t knob_flags = panel_controller.KnobFlags();
    forge::ComposeKnobLeds(panel_controller.KnobValues(), panel_controller.KnobState(), view.record_position, knobs, knob_flags, view.magic);
    const uint32_t knob_state = panel_controller.KnobState();
    forge::Rgb volume = forge::knobs::VolumeColour((knob_state >> 20) & 1u, (knob_state >> 24) / 255.f);
    forge::ComposeMenuKnobLeds(panel_controller.MenuLights(), knobs, volume);   // TAPE's menu page
    if((view.menu & 1u) && ((view.menu >> 1) & 7u) == 7u) forge::ComposeHarmonyKnobLeds(view.harmony, knobs, knob_flags);   // harmony page
    if((view.menu & 1u) && ((view.menu >> 1) & 7u) == 6u) forge::ComposePartsKnobLeds(view.parts, view.parts_clock, knobs, knob_flags);   // parts page
    if((view.menu & 1u) && ((view.menu >> 1) & 7u) == 5u) {
        if((view.fx >> 31) & 1u) forge::ComposeFxKnobLeds(panel_controller.FxKnobs(), knobs, knob_flags,    // FX page (0.20)
                                                          panel_controller.FxCharacters(), (view.fx >> 26) & 15u);
        else forge::ComposeSynthKnobLeds(view.synth, panel_controller.SynthKnobs(), knobs, knob_flags);    // synth page (0.19)
    }
    if(((view.dream >> 26) & 1u) && !(view.menu & 1u)) {   // 0.19.2 Dream: the rings and CHOMPI breathe in the chord's colours
        for(unsigned k = 0; k < 4; ++k) {
            const float wave = .5f + .5f * std::sin(now * (6.2831853f / 4000.f) + 1.57f * k);
            knobs[k] = forge::knobs::Scale(forge::knobs::StepColour(view.dream_colour + k), .12f + .55f * wave);
        }
        chompi = forge::knobs::Scale(forge::knobs::StepColour(view.dream_colour), .15f + .6f * view.dream_level);
    }
    for(unsigned k = 0; k < 4; ++k) Pth(forge::panel::kKnobLed[k], knobs[k]);
    forge::ComposeTransportLeds(view.looper, loop_speed.load(std::memory_order_relaxed), loop_head.load(std::memory_order_relaxed),
                                knob_flags, view.record_position, reverse, forward);
    Pth(5, reverse); Pth(6, forward);
    for(unsigned i = 0; i < 25; ++i) { const forge::Rgb c = Balance(keys[i]); SetSmtLedFloat(i, c.r, c.g, c.b); }
    if(safe_mode && (now / 250) % 8 == 0) chompi = kSafeModeColour;   // a magenta blink every 2 s
    Pth(0, chompi);
    // SW6 (TAPE): held 2 s = battery; otherwise its page (volume / input gain) and value.
    if(panel_controller.BatteryView()) {
        const auto b = forge::power::BatteryColour(static_cast<forge::power::Battery>(hw.GetBatteryLevel()));
        volume = forge::Rgb{b.r, b.g, b.b};
    } else {                                                       // 0.12: low battery warning (core/power.h)
        const auto warning = forge::power::BatteryWarning(static_cast<forge::power::Battery>(hw.GetBatteryLevel()), ChargerReadings());
        if(forge::power::WarningLit(warning, now)) volume = forge::Rgb{forge::power::kWarningColour.r, forge::power::kWarningColour.g, forge::power::kWarningColour.b};
    }
    Pth(forge::panel::kVolumeLed, volume);
    fill_led_data();
    auto seven = [](float x) { return static_cast<uint8_t>(forge::Clamp(x, 0.f, 1.f) * 127.f + 0.5f); };
    for(unsigned i = 0; i < 26; ++i) {
        const forge::Rgb& c = i < 25 ? keys[i] : chompi;
        led_shadow[i][0] = seven(c.r); led_shadow[i][1] = seven(c.g); led_shadow[i][2] = seven(c.b);
    }
    // 0.20 display: the same colours as drawn (before the LED balance), in kViewLeds order.
    const forge::Rgb* lights[forge::kViewLeds - 27] = {&play, &loop, &knobs[0], &knobs[1], &knobs[2], &knobs[3], &reverse, &forward};
    for(unsigned i = 0; i < forge::kViewLeds; ++i) {
        const forge::Rgb& c = i < 26 ? (i < 25 ? keys[i] : chompi) : i < forge::kViewLeds - 1 ? *lights[i - 26] : volume;
        view_leds[i][0] = seven(c.r); view_leds[i][1] = seven(c.g); view_leds[i][2] = seven(c.b);
    }
}
// Card insert/remove: remount and rescan when the card comes back.
// Safe mode at start-up: the four knob lights flash magenta three times (1.2 s).
FORGE_COLD void FlashSafeMode() {
    for(unsigned i = 0; i < 6; ++i) {
        for(unsigned k = 0; k < 4; ++k) Pth(forge::panel::kKnobLed[k], i % 2 ? forge::Rgb{} : kSafeModeColour);
        fill_led_data();
        System::Delay(200);
        hw.LowBatteryLockoutCheck();
    }
}
// Both views of the card; the settings view stays closed in safe mode.
FORGE_COLD void MountCard() {
    const uint32_t started = System::GetNow();
    const FRESULT result = f_mount(&fsi.GetSDFileSystem(), fsi.GetSDPath(), 1);
    const bool mounted = result == FR_OK;
#ifdef FORGE_TEST_HOOKS
    InspectorEvent(forge::InspectorEventKind::Card, 1, System::GetNow() - started);   // mount time, ms
    InspectorEvent(forge::InspectorEventKind::Card, 2, static_cast<uint32_t>(result)); // FatFs result (0 = ok)
#endif
    install_card.SetMounted(mounted);
    card.SetMounted(mounted && !safe_mode);
#ifdef FORGE_TEST_HOOKS
    if(disk_status(0)==RES_OK && !mounted) InspectorStorageError(forge::Error::Storage);
#endif
}
FORGE_COLD void WatchCard() {
    // A card that is already mounted is left alone; a failing one is retried 1, 2, 4 … 32 s
    // apart, so a bad card cannot stall the main loop (and USB replies) every second.
    static uint32_t last_check = 0, retry_ms = 1000; static int8_t was_ready = -1;
    const uint32_t now = System::GetNow();
    if(was_ready < 0) was_ready = install_card.Ready() ? 1 : 0;   // the start-up mount's result
    if(now - last_check < (was_ready ? 1000 : retry_ms)) return;
    last_check = now;
    const bool present = disk_status(0) == RES_OK;
    if(present && !was_ready) {
        MountCard();
        store.Rescan();
        LoadSlotSettings();
        retry_ms = install_card.Ready() ? 1000 : (retry_ms < 32000 ? retry_ms * 2 : 32000);
    }
    if(!present) retry_ms = 1000;                  // a card taken out: look again soon
    was_ready = present && install_card.Ready() ? 1 : 0;
}

#ifdef FORGE_TEST_HOOKS
void CollectInspector() {
    for(unsigned i=0;i<64;++i) { forge::InspectorEvent e; if(!inspector_edges.Pop(e)) break; inspector_log.Add(e); }
    inspector_mailbox.Read(inspector_latest);
    static uint32_t refreshed=0;
    const uint32_t now=System::GetNow();
    if(now-refreshed>=50) { refreshed=now; inspector_mailbox.RequestRefresh(); }
    static uint32_t drops=0, loaded=0, errors=0; static uint8_t card_flags=127;
    const uint32_t d=dropped_commands+uart_midi.dropped.load()+usb_midi.dropped.load()
        +inspector_panel_drops.load()+inspector_sample_drops.load()+inspector_event_drops.load();
    if(d!=drops) { InspectorEvent(forge::InspectorEventKind::QueueError,0,d-drops); drops=d; }
    const uint8_t flags=(disk_status(0)==RES_OK?1:0)|(card.Ready()?2:0);
    if(flags!=card_flags) { card_flags=flags; InspectorEvent(forge::InspectorEventKind::Card,0,flags); }
    forge::InspectorStorage st; sample_loader.Inspect(st);
    if(st.loaded_selection!=loaded && st.loaded_selection!=0xffffffffu && !st.loading) {
        loaded=st.loaded_selection; InspectorEvent(forge::InspectorEventKind::SampleLoaded,0,loaded);
    }
    if(st.errors!=errors) {
        inspector_last_storage_error=static_cast<uint8_t>(forge::Error::Storage);
        InspectorEvent(forge::InspectorEventKind::StorageError,1,st.errors-errors); errors=st.errors;
    }
}
FORGE_COLD void CaptureInspector() {
    CollectInspector();
    auto& s=inspector_snapshot; ++s.generation; s.audio=inspector_latest;
    auto& sys=s.system; sys.uptime_ms=System::GetNow();
    sys.rx[0]=uart_midi.received.load(); sys.rx[1]=usb_midi.received.load();
    sys.ingress_drops[0]=uart_midi.dropped.load(); sys.ingress_drops[1]=usb_midi.dropped.load();
    for(unsigned i=0;i<2;++i) { sys.tx[i]=inspector_tx[i]; sys.tx_errors[i]=inspector_tx_errors[i]; }
    sys.dropped=dropped_commands+sys.ingress_drops[0]+sys.ingress_drops[1]; sys.rejected=rejected_messages;
    sys.panel_drops=inspector_panel_drops.load(); sys.sample_drops=inspector_sample_drops.load();
    sys.event_drops=inspector_event_drops.load(); sys.emergencies=emergency_epoch.load();
    ReadPower(sys.battery, sys.power_flags, sys.charge_state);
    sys.reset_flags=reset_flags; sys.crashed=last_fault.Valid(); sys.crash_pc=last_fault.pc;
    auto& st=s.storage; sample_loader.Inspect(st);
    st.present=disk_status(0)==RES_OK; st.mounted=card.Mounted(); st.record_capacity_frames=kRecordFrames;
    st.errors+=inspector_storage_errors;
    if(inspector_last_storage_error) st.last_error=inspector_last_storage_error;
    st.pending+=static_cast<uint8_t>(sample_jobs.Size());
}
#endif

// One received frame (shared by both transports, so the code exists once).
// 0.20 display (opcode 0D page 1): the panel's state words, all already published for the lights.
FORGE_COLD forge::ViewState ViewNow() {
    forge::ViewState v;
    const uint32_t chord = chord_word.load(std::memory_order_acquire), notes = chord_notes.load(std::memory_order_relaxed);
    const uint32_t words[forge::kViewWords] = {
        menu_state.load(std::memory_order_relaxed), looper_state.load(std::memory_order_relaxed),
        panel_controller.KnobState(), panel_controller.KnobValues(), panel_controller.KnobTargets(),
        panel_controller.FxLights(), panel_controller.FxKnobs(), panel_controller.SynthLights(), panel_controller.SynthKnobs(),
        panel_controller.HarmonyLights(), panel_controller.PartsLights(), panel_controller.PartsClock(),
        panel_controller.SequenceLights(), chord,
        (panel_controller.RecordPosition() ? 1u : 0u) | (recording_now.load(std::memory_order_relaxed) ? 2u : 0u)
            | (safe_mode ? 4u : 0u) | (sample_loader.Busy() ? 8u : 0u) | (install_gate.Armed() ? 16u : 0u),
        panel_controller.DreamLights()};
    for(unsigned i = 0; i < forge::kViewWords; ++i) v.words[i] = words[i];
    for(unsigned i = 0; i < 4; ++i) v.chord_notes[i] = (notes >> (8 * i)) & 127u;
    v.chord_notes[4] = (chord >> 25) & 127u;
    ReadPower(v.battery, v.power_flags, v.charge_state);
    return v;
}
FORGE_COLD void HandleFrame(const forge::MidiFrame& frame, uint8_t source) {
    forge::Request request;
    if(frame.kind == forge::MidiFrame::Kind::SysEx && forge::IsRequest(frame.data, frame.size)
       && frame.data[4] == forge::kFileOpcode) {
        uint8_t envelope[kMaxEnvelope];
        const size_t size = forge::ServeFileRequest(frame.data, frame.size, file_transfer, upload_files, install_gate,
                                                    System::GetNow(), envelope + 1, sample_loader.Busy(), InstallPowerOk());
        if(file_transfer.SampleWritten()) sample_loader.Rescan();
        Send(source, envelope, size);
        return;
    }
    if(frame.kind != forge::MidiFrame::Kind::SysEx) {
        switch(forge::TranslateChannel(frame, source, request, panel_controller.GetOptions().midi_in)) {
            case forge::Ingress::Emergency: RaiseEmergency(); break;
            case forge::Ingress::Critical:
                if(!Queue(request)) { ++dropped_commands; discard_ingress = true; RaiseEmergency(); }
                break;
            case forge::Ingress::Control: if(!Queue(request)) ++dropped_commands; break;
            case forge::Ingress::Storage: RecallPreset(request); break;   // program change
            case forge::Ingress::Ignore: break;
        }
        return;
    }
    if(!forge::IsRequest(frame.data, frame.size)) return;
    auto error = forge::DecodeRequest(frame.data, frame.size, request);
    request.source = source;
    if(error == forge::Error::None) error = HandleStorage(request);
    bool handled = request.kind == forge::RequestKind::Erase || request.kind == forge::RequestKind::List;
    if(error == forge::Error::None && request.kind == forge::RequestKind::View) {   // 0.20 display: main loop only
        uint8_t envelope[kMaxEnvelope];
        Send(source, envelope, request.page == 0 ? forge::EncodeViewLeds(request.sequence, view_leds, envelope + 1)
                                                 : forge::EncodeViewState(request.sequence, ViewNow(), envelope + 1));
        return;
    }
#ifdef FORGE_TEST_HOOKS
    // Development probe: answered here from the latest snapshots; panel events
    // go to the audio owner and are acknowledged once queued.
    if(error == forge::Error::None && request.kind == forge::RequestKind::Probe) {
        uint8_t envelope[kMaxEnvelope];
        size_t size;
        if(request.page>=2 && request.page!=11 && !inspector_latest.block) {   // page 11: main loop only
            inspector_mailbox.RequestRefresh();
            ++rejected_messages;
            Send(source,envelope,forge::EncodeError(request.sequence,forge::Error::Busy,envelope+1));
            return;
        }
        if(request.page==1) size = forge::EncodeProbeLeds(request.sequence, led_shadow, envelope + 1);
        else {
            if(request.page==11) FillBattery(inspector_snapshot.battery);
            else if(request.page==2 || !inspector_snapshot.generation) CaptureInspector();
            size=forge::EncodeInspector(request.sequence,request.page,inspector_snapshot,inspector_log,request.inspector_cursor,envelope+1);
        }
        Send(source, envelope, size);
        return;
    }
    if(error == forge::Error::None && request.kind == forge::RequestKind::Panel) {
        if(Queue(request)) { uint8_t envelope[kMaxEnvelope]; Send(source, envelope, forge::EncodePanelAck(request.sequence, envelope + 1)); return; }
        error = forge::Error::Busy;
        handled = true;
    }
#endif
    if(error == forge::Error::None && request.kind == forge::RequestKind::SampleList) {
        SendResponse(forge::SampleListReply(sample_loader, sample_files.Ready(), sample_table.slots[forge::kRamSlot],
                                            kRecordFrames, request));
        handled = true;
    } else if(error == forge::Error::None && request.kind == forge::RequestKind::SampleJob
              && request.action != forge::SampleAction::Save) {   // save goes to the audio owner first
        forge::SampleJob job;
        error = forge::FileJob(sample_loader, request, job);
        if(error == forge::Error::None && !sample_loader.Queue(job)) error = forge::Error::StorageBusy;
        handled = true;
    }
    if(error == forge::Error::None && !handled && !Queue(request)) {
        ++dropped_commands; error = forge::Error::Busy;
    } else if(error == forge::Error::None && !handled && request.kind == forge::RequestKind::Patch && request.recall) {
        LoadSequence(last_bank, last_slot);   // a host recall: its loop after its patch (0.15.1)
    }
    if(error != forge::Error::None) {
#ifdef FORGE_TEST_HOOKS
        if(error==forge::Error::Storage || error==forge::Error::StorageBusy) InspectorStorageError(error);
#endif
        ++rejected_messages;
        uint8_t envelope[kMaxEnvelope];
        const size_t size = forge::EncodeError(forge::Read14(frame.data + 5), error, envelope + 1);
        Send(source, envelope, size);
    }
}
template<typename Midi> void PollMidi(Midi& midi, uint8_t source) {
    midi.Listen();
    forge::MidiFrame frame;
    for(unsigned i = 0; i < 8 && midi.frames.Pop(frame); ++i) { HandleFrame(frame, source); panel_controller.NoteActivity(); }
}

FORGE_COLD void SendResponses() {
    forge::Response response;
    for(unsigned i = 0; i < 4 && outgoing.HasSpace() && responses.Pop(response); ++i) {
        if(response.kind == forge::ResponseKind::SampleSnapshot) {   // host save: the take is locked
            if(!sample_loader.Queue(forge::SaveJob(response))) {
                recorder.Unlock();
                response.kind = forge::ResponseKind::SampleDone; response.error = forge::Error::StorageBusy;
                ++rejected_messages; SendResponse(response);
            }
            continue;
        }
        if(response.kind == forge::ResponseKind::Snapshot) {   // host store: write, then acknowledge
            response = forge::StoreReply(store, response.sequence, response.source, response.bank, response.slot,
                                         response.patch);
            if(sequence_mailbox) {                                   // the project's loop (0.15)
                const forge::Error e = forge::seq::SaveFile(card, *sequence_mailbox, response.bank, response.slot, sequence_file,
                                                            response.error == forge::Error::None, forge::seq::Mailbox::Host);
                if(response.error == forge::Error::None && e == forge::Error::Storage) response.error = e;
            }
            if(response.error != forge::Error::None) ++rejected_messages;
#ifdef FORGE_TEST_HOOKS
            InspectorStorageError(response.error);
#endif
        }
        SendResponse(response);
    }
}
} // namespace

FORGE_COLD int main() {
    const uint32_t rsr = RCC->RSR;                 // why this start, before anything clears it
    RCC->RSR |= RCC_RSR_RMVF;
    reset_flags = forge::restart::Flags(rsr);
    hw.Init();
    hw.MpWrite(0x0c, 0B01010001); // retain upstream 3 V battery threshold
    hw.MpReadAll();
    for(unsigned i = 0; i < 10; ++i) {
        hw.LowBatteryLockoutCheck();
        System::Delay(10);
    }
    // SD card first, exactly where TAPE mounts it (before USB, MIDI and the engine). Mounted
    // after them it hung ~60 s at start-up (two of libDaisy's 30 s SD timeouts, the owner's unit,
    // 2026-10-10). Missing card: presets are unavailable (menu keys red), the rest works.
    {
        SdmmcHandler::Config sd_config;
        sd_config.speed = SdmmcHandler::Speed::FAST;
        sd_config.width = SdmmcHandler::BusWidth::BITS_4;
        sdmmc.Init(sd_config);
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        const uint32_t started = System::GetNow();
        MountCard();
        mount_ms = System::GetNow() - started;
    }
    // Then Forge's own start: fault handler (also opens backup SRAM), restart records, safe
    // mode (closes the card's settings view after the fact), lights.
    InstallFaultHandler();
    if(fault_record.Valid()) { last_fault = fault_record; fault_record.magic = 0; }
    safe_mode = forge::restart::SafeModeStart(startup_guard, last_fault.Valid());
    if(safe_mode) { forge::RuntimeBuildFlags() |= forge::kBuildSafeMode; card.SetMounted(false); }
    if(boot_count.magic != forge::restart::FaultRecord::kMagic) { boot_count.magic = forge::restart::FaultRecord::kMagic; boot_count.boots = 0; }
    ++boot_count.boots;
    LedSetup();
    // The start-up card work right after the mount, before USB / MIDI / the engine (TAPE reads
    // its options here too). The owner's unit stalled ~30-60 s when these ran after USB came up.
    uint32_t step = System::GetNow();
    store.Rescan();      BootStep(3, step);
    LogRestart();        BootStep(4, step);
    ReadOptions();       BootStep(5, step);
    LoadSlotSettings();  BootStep(6, step);
    ReadLastPreset();
    // Stock start-up scan (0.5 s, also clears shift-register junk): CHOMPI + PLAY +
    // LOOP held while CHOMPI starts switches it off (charger IC shipping mode).
    forge::power::BootGesture off_gesture;
    for(unsigned i = 0; i < forge::power::kBootScans; ++i) {
        hw.ProcessAllControls();
        off_gesture.Scan(hw.button_sr.State(int(Hardware::SwId::KEY_26)), hw.button_sr.State(int(Hardware::SwId::KEY_27)),
                         hw.button_sr.State(int(Hardware::SwId::KEY_28)));
        System::DelayUs(forge::power::kBootScanUs);
    }
    if(off_gesture.PowerOff()) {
        hw.LedsOff();
        hw.MpWrite(0x08, 0B10111111);   // SHIPPING MODE (as stock)
        while(true) System::Delay(10);
    }
    // Match the upstream charge-detection/USB switch sequence.
    hw.usb_sw.Write(false);
    System::Delay(1);
    hw.MpWrite(0x0a, 0B00100100);
    System::Delay(1);
    hw.usb_sw.Write(true);

    MidiUartTransport::Config uart_config;
    uart_midi.transport.Init(uart_config);
    uart_midi.Listen();
    MidiUsbTransport::Config usb_config;
    usb_config.periph = MidiUsbTransport::Config::EXTERNAL;
    usb_midi.transport.Init(usb_config);
    usb_midi.Listen();
    BootStep(7, step);   // charger hand-over, boot scan, MIDI and USB

    engine.SetSamples(&sample_table);
    recorder.Init(record_memory, kRecordFrames, &sample_table.slots[forge::kRamSlot], hw.seed.AudioSampleRate());
    looper.Init(loop_memory, kLoopFrames, hw.seed.AudioSampleRate());
    engine.SetLooper(&looper);
    engine.SetWarbleMemory(warble_memory);
    sample_loader.Init(&sample_table, &sample_handoff, sample_pool, kPoolSamples, sample_scratch, sizeof(sample_scratch));
    if(!engine.Init(hw.seed.AudioSampleRate(), delay_left, delay_right, kDelayCapacity,
                    reverb_memory, kReverbCapacity)) {
        SetPthLedFloat(0, 0.1f, 0.f, 0.f);
        fill_led_data();
        while(true) { hw.LowBatteryLockoutCheck(); System::Delay(20); }
    }
    SetPthLedFloat(0, 0.f, 0.05f, 0.1f);
    fill_led_data();
    engine.SetFxMemory(fx_memory, kFxFloats);   // FX section: off without it (Motion silent, cloud undiffused)
    BootStep(8, step);   // sampler, looper, engine
    LoadOptions();
    engine.SetSlotSettings(&slot_settings);
    engine.SetHarmony(&harmony_player);
    parts.Init(hw.seed.AudioSampleRate()); engine.SetParts(&parts);
    sequence_mailbox = new(sequence_mailbox_memory) forge::seq::Mailbox();   // SDRAM is running now
    engine.SetSequencer(&sequencer, sequence_mailbox);
    panel_controller.SetInstallGate(&install_gate);
    cpu.Init(hw.seed.AudioSampleRate(), hw.seed.AudioBlockSize());
    if(safe_mode) FlashSafeMode();
    // 0.17 "wakes up singing": the remembered preset (no loop: nothing plays by itself), then the chime.
    if(remembered_slot != 0xff) {
        forge::Request request, apply; request.kind = forge::RequestKind::Recall;
        request.bank = remembered_bank; request.slot = remembered_slot; request.silent = true;
        if(forge::RecallRequest(store, request, apply) == forge::Error::None && Queue(apply)) { last_bank = remembered_bank; last_slot = remembered_slot; }
    }
    panel_controller.RequestChime();
    hw.StartAudio(AudioCallback);
    BootStep(9, step);   // the rest, up to audio running
    uint32_t battery_check = System::GetNow();
    forge::power::LockoutGuard lockout_guard;
    bool stable = false;
    while(true) {
        TransmitPending();
        SendPanelMidi();
        SendResponses();
        static uint32_t seen_drops = 0;
        const uint32_t ingress_drops = uart_midi.dropped.load(std::memory_order_relaxed) + usb_midi.dropped.load(std::memory_order_relaxed);
        if(ingress_drops != seen_drops || discard_ingress) {
            ScopedIrqBlocker guard;
            forge::MidiFrame ignored;
            for(unsigned i = 0; i < 16; ++i) { uart_midi.frames.Pop(ignored); usb_midi.frames.Pop(ignored); }
            uart_midi.framer.Reset(); usb_midi.framer.Reset();
            RaiseEmergency();
            seen_drops = ingress_drops; discard_ingress = false;
        }
        PollMidi(uart_midi, 0);
        PollMidi(usb_midi, 1);
        RunPanelActions();
        RunSampler();
        WatchCard();
        WatchLastPreset();
        DrawLeds();
#ifdef FORGE_TEST_HOOKS
        CollectInspector();
#endif
        const uint32_t now = System::GetNow();
        if(now - battery_check >= 20) {
            // 0.19.2: the stock check's parts, its protection only after 3 s of low readings
            // (core/power.h LockoutGuard): a dip while playing no longer stops CHOMPI.
#ifdef FORGE_TEST_HOOKS
            battery_monitor.Sample(hw.read_ready, hw.mp_buff_[5]);
            if(battery_monitor.PeriodicDue(now, hw.read_ready)) ReadBackThreshold(forge::power::ReadBack::Periodic);
            battery_monitor.ReadStarted();
#endif
            const auto lockout = lockout_guard.Update(ChargerReadings());
            if(lockout != forge::power::Lockout::None) { level_check.Abort(now, charger_io); LogLockout(lockout); hw.LowBatteryLockoutCheck(); }
            else {
                // 0.19.2: Forge's level check (core/power.h LevelCheck) in place of BMCMediumBattCheck.
                // Charging = CHG_STAT 1-4 (reg 0x13 bits 7-5); the stock read keeps FULL (white).
                const uint8_t chg = (hw.mp_buff_[2] >> 5) & 7u;
                const uint32_t checks = level_check.Checks() + level_check.GivenUp();
                const auto level = level_check.Tick(now, hw.read_ready, static_cast<forge::power::Battery>(hw.GetBatteryLevel()),
                                                    chg >= 1 && chg <= 4, charger_io);
                if(level != static_cast<forge::power::Battery>(hw.GetBatteryLevel()))
                    hw.batt_level = static_cast<decltype(hw.batt_level)>(level);
#ifdef FORGE_TEST_HOOKS
                battery_monitor.LevelState(level_check.Paused(), level_check.Checks() + level_check.GivenUp() != checks);
#else
                (void)checks;
#endif
            }
            // After the level check: it needs the bus idle (the read started 20 ms ago has completed),
            // and no read may start while its 3.3 / 3.1 V threshold is set (0.19.2).
            if(!level_check.Paused()) hw.BMCPerformCheck();
            battery_check = now;
        }
        if(!stable && now >= forge::restart::kStableMs) { forge::restart::RanStably(startup_guard); stable = true; }
        ServiceChargerUsb(now);
        SaveSlotSettings(now);
        if(install_gate.Poll(now)) {
            if(InstallPowerOk()) Restart();
            else install_gate.Cancel();       // power changed since the request: the host sees the power flag
        }
        System::DelayUs(100);
    }
}
