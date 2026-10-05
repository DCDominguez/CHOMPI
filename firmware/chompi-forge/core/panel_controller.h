#pragma once
#include <cstdint>
#include "command_queue.h"
#include "engine.h"
#include "file_transfer.h"
#include "knob_layout.h"
#include "options.h"
#include "power.h"
#include "preset_menu.h"
#include "recorder.h"
#include "sample_loader.h"
#include "sampler_runtime.h"

namespace forge {
namespace panel {
// Matches upstream NormalPage::key_map: 25 chromatic keys, MIDI 48..72.
constexpr uint8_t kKeyNotes[40] = {0,0,0,0,0,0,0,49,50,52,53,55,51,54,56,48,
    57,59,60,62,64,58,61,63,65,67,69,71,72,66,68,70,0,0,0,0,0,0,0,0};
constexpr unsigned kButtons = 40, kEncoders = 6, kVolumeEncoder = 5, kToneEncoder = 4;
// Looper keys (as TAPE): KEY_27 PLAY, KEY_28 LOOP; their LEDs are through-hole 7 / 8.
// Menu (presets page): KEY_21 effects before the loop, KEY_20 after.
constexpr uint8_t kPlayKey = 33, kLoopKey = 34, kPlayLed = 7, kLoopLed = 8, kFxBefore = 22, kFxAfter = 21;
// SW6 (volume) press is ENC_6_SW; its ring LED is through-hole 9 (TAPE). Held 2 s: battery.
constexpr uint8_t kVolumePress = 32, kVolumeLed = 9;
// Knob pages (docs/forge/KNOBS.md, core/knob_layout.h). Releasing logical knob n's
// switch (button kKnobEncoder[n]) steps its page, as TAPE; holding it 1.5 s without
// turning resets its control; SW4 + SW3 held together 1 s = panic.
// Knob n's ring LED is through-hole LED n + 1 (ENC_4, ENC_1, ENC_2, ENC_3 as TAPE); SW5's are 5 and 6.
constexpr uint8_t kKnobLed[4] = {1, 2, 3, 4};
constexpr uint32_t kResetHoldFrames = 48 * 1500, kPanicHoldFrames = 48 * 1000;   // 48 kHz
constexpr uint32_t kPresetsHoldFrames = 48 * 1000;     // menu: hold KEY_22 1 s = Forge presets page
// TAPE's monitor positions (options.json "Monitor Position" 1-3; menu SW6 press cycles).
enum class MonitorMode : uint8_t { Headphones, Both, SendReturn };
} // namespace panel

// One block of debounced hardware input (audio owner).
struct PanelInput {
    uint64_t keys = 0;                   // bit k: button k held (upstream SwId order)
    bool toggle_up = false, jack = false;
    bool tone_down = false;              // SW5 switch held
    int16_t turns[panel::kEncoders]{};   // increments by hardware encoder index
    uint16_t frames = 48;                // audio frames in this block (looper key timing)
};

// Development-only panel events (firmware built with FORGE_TEST_HOOKS, and
// the offline harness). They are merged with the hardware so the same code
// paths run: a virtual key press is indistinguishable from a real one.
struct PanelEvent {
    enum class Kind : uint8_t { Key, Turn, Press, Toggle, Jack, Release };
    Kind kind = Kind::Release;
    uint8_t id = 0;      // Key: button 0-39; Turn: hardware encoder 0-5
    int8_t value = 0;    // Key: 1 down / 0 up; Turn: increment; Toggle/Jack: 0, 1, or -1 = hardware again
};

// MIDI the panel sends out (TAPE: keys, knob CCs, PLAY/LOOP/CHOMPI CCs), audio -> main loop.
struct MidiOut { uint8_t status = 0, data1 = 0, data2 = 0; };

// Work the panel hands to the rest of the firmware (main-loop side).
class PanelSink {
public:
    virtual ~PanelSink() = default;
    virtual bool PresetAction(const MenuAction& action, const Parameters& snapshot) = 0;  // false: queue full
    virtual bool SampleJob(const forge::SampleJob& job) = 0;
    virtual void Flash(bool ok) = 0;
};

// Everything the CHOMPI panel does, once per audio block: the TAPE-style menu
// (presets and samples pages), the record gesture, keys -> notes, knobs ->
// parameters, SW5 cutoff/panic, SW6 level, jack -> record source.
class PanelController {
public:
    void Inject(const PanelEvent& e) {
        switch(e.kind) {
            case PanelEvent::Kind::Key:
                if(e.id < panel::kButtons) { const uint64_t bit = uint64_t(1) << e.id; virtual_keys_ = e.value ? virtual_keys_ | bit : virtual_keys_ & ~bit; }
                break;
            case PanelEvent::Kind::Turn: if(e.id < panel::kEncoders) virtual_turns_[e.id] = static_cast<int16_t>(virtual_turns_[e.id] + e.value); break;
            case PanelEvent::Kind::Press: virtual_tone_ = e.value == 1 ? 1 : e.value == 2 ? 0 : 2; break;  // 0 click, 1 down, 2 up
            case PanelEvent::Kind::Toggle: toggle_override_ = e.value; break;
            case PanelEvent::Kind::Jack: jack_override_ = e.value; break;
            case PanelEvent::Kind::Release:
                virtual_keys_ = 0; toggle_override_ = jack_override_ = -1; virtual_tone_ = 0;
                for(auto& t : virtual_turns_) t = 0;
                break;
        }
    }
    bool Overridden() const { return virtual_keys_ || toggle_override_ >= 0 || jack_override_ >= 0; }
#ifdef FORGE_TEST_HOOKS
    void SetInspectorEvents(SpscQueue<InspectorEvent,64>* events, std::atomic<uint32_t>* drops, uint32_t now) {
        events_=events; event_drops_=drops; event_time_=now;
    }
#endif

    FORGE_COLD void Block(const PanelInput& hardware, Engine& engine, Recorder& recorder, PanelSink& sink) {
        engine.SyncSlotSettings();                                         // kit pads follow menu copies/erases
        uint64_t keys = hardware.keys | virtual_keys_;
        if(install_gate_) install_gate_->Filter(keys, panel::kChompiKey);   // firmware install confirmation owns CHOMPI
        const bool toggle_up = toggle_override_ >= 0 ? toggle_override_ != 0 : hardware.toggle_up;
        const bool jack = jack_override_ >= 0 ? jack_override_ != 0 : hardware.jack;
#ifdef FORGE_TEST_HOOKS
        for(unsigned key=0;key<panel::kButtons;++key) {
            const uint64_t bit=uint64_t(1)<<key;
            if((keys^event_keys_)&bit) {
                const bool down=(keys&bit)!=0;
                const uint64_t physical=down?hardware.keys:physical_keys_;
                const uint64_t injected=down?virtual_keys_:event_virtual_keys_;
                LogEdge(down?InspectorEventKind::KeyDown:InspectorEventKind::KeyUp,key,
                        (physical&bit?1u:0u)|(injected&bit?2u:0u));
            }
        }
        event_keys_=keys; event_virtual_keys_=virtual_keys_;
        physical_keys_=hardware.keys; logical_keys_=keys;
        physical_flags_=(hardware.toggle_up?1:0)|(hardware.jack?2:0)|(hardware.tone_down?4:0);
        logical_flags_=(toggle_up?1:0)|(jack?2:0)|((hardware.tone_down||virtual_tone_)?4:0)|(Overridden()?8:0);
#endif
        const bool chompi = (keys >> panel::kChompiKey) & 1u;
        menu_.Update(toggle_up, chompi);
        // TAPE: jack insertion selects line in, removal the mic.
        if(first_ || jack != jack_) {
            jack_ = jack; first_ = false;
            source_ = jack ? RecordSource::Line : RecordSource::Mic;
            menu_.SetRecordSource(static_cast<uint8_t>(source_));
        }
        // TAPE: toggle down + hold CHOMPI records; on release the take becomes
        // the chromatic recording slot and plays at once.
        // Forge: a 1.5 s count-in (CHOMPI and the white keys blink red) before recording.
        switch(gesture_.Update(toggle_up, chompi, hardware.frames)) {
            case RecordGesture::Event::Start:
                if(!recorder.Start()) { gesture_.Cancel(); sink.Flash(false); }
                break;
            case RecordGesture::Event::Stop: {
                recorder.Stop();
                // TAPE: the take plays at once, chromatic, with pitch, gain, start and end back to default.
                const Parameters& p = engine.GetParameters();
                Parameters take = SelectSample(p, 0, p.sample_bank, kRamSlot);
                take.sample_start = 0.f; take.sample_end = 1.f;
                engine.ApplyPatch(take, SlotPolicy::Select);
                engine.ResetControl(Parameter::Speed); engine.ResetControl(Parameter::VoiceGain);
                break;
            }
            default: break;
        }
        recording_position_.store(!toggle_up, std::memory_order_relaxed);
        if(!toggle_up && chompi != chompi_was_) SendMidi(0xb0, 21, chompi ? 127 : 0);   // TAPE: CHOMPI = CC 21 in the record position
        chompi_was_ = chompi;
        count_in_.store(gesture_.CountInPhase(), std::memory_order_relaxed);
        const uint64_t rising = keys & ~prev_keys_, falling = prev_keys_ & ~keys;
        prev_keys_ = keys;
        // TAPE: SW6 held 2 s shows the battery on its light while it stays down; a
        // shorter press switches SW6 between volume and input gain.
        if((keys >> panel::kVolumePress) & 1u) {
            if(volume_held_ < kBatteryHoldFrames) volume_held_ += hardware.frames;
        } else {
            if((falling >> panel::kVolumePress) & 1u && volume_held_ < kBatteryHoldFrames && !volume_in_menu_) volume_page_ = !volume_page_;
            if((falling >> panel::kVolumePress) & 1u) volume_in_menu_ = false;
            volume_held_ = 0;
        }
        battery_view_.store(volume_held_ >= kBatteryHoldFrames, std::memory_order_relaxed);
        keys_down_.store(static_cast<uint32_t>(keys), std::memory_order_relaxed);   // all 25 note keys are switches 0-31
        for(unsigned key = 0; key < panel::kButtons; ++key) if(panel::kKeyNotes[key]) {
            if(((rising >> key) & 1u) && !menu_.Key(static_cast<uint8_t>(key), true))
                PlayKey(engine, panel::kKeyNotes[key], 127);   // TAPE: full velocity
            if((falling >> key) & 1u) {
                menu_.Key(static_cast<uint8_t>(key), false); PlayKey(engine, panel::kKeyNotes[key], 0);
            }
        }
        Looper* looper = engine.GetLooper();
        if(looper) {
            // PLAY / LOOP. In the menu they set the overdub feedback (-/+ 10 %, as TAPE);
            // releases always reach the looper so no key can stay held there.
            for(const uint8_t key : {panel::kPlayKey, panel::kLoopKey}) {
                const bool play = key == panel::kPlayKey;
                if((rising >> key) & 1u) {
                    if(menu_.Active()) { if(play || !menu_.SelectLoopSource()) looper->AdjustFeedback(play ? -0.1f : 0.1f); }
                    else if(play) looper->Play(true); else looper->Loop(true);
                }
                if((falling >> key) & 1u) { if(play) looper->Play(false); else looper->Loop(false); }
                if(((rising | falling) >> key) & 1u) SendMidi(0xb0, play ? 26 : 27, (keys >> key) & 1u ? 127 : 0);   // TAPE
            }
            looper->Tick(hardware.frames);
        }
        // Effects before/after the looper (menu keys), outside the looper so it works without one.
        const bool presets_page = menu_.Active() && menu_.Page() == MenuPage::Presets;
        if(presets_page && ((rising >> panel::kFxBefore) & 1u)) engine.SetFxBeforeLoop(true);
        if(presets_page && ((rising >> panel::kFxAfter) & 1u)) engine.SetFxBeforeLoop(false);
        // TAPE's menu page: KEY_21 effects before the looper, KEY_22 after (on release, so a
        // 1 s hold can open Forge's presets page instead).
        const bool tape_page = menu_.Active() && menu_.Page() == MenuPage::Samples;
        if(tape_page && ((rising >> panel::kFxBefore) & 1u)) engine.SetFxBeforeLoop(true);
        if(tape_page && ((rising >> panel::kPage) & 1u)) { page_key_frames_ = 0; page_key_armed_ = true; }
        if(page_key_armed_ && ((keys >> panel::kPage) & 1u)) {
            page_key_frames_ += hardware.frames;
            if(page_key_frames_ >= panel::kPresetsHoldFrames) { menu_.ShowPage(MenuPage::Presets); page_key_armed_ = false; }
        }
        if(page_key_armed_ && ((falling >> panel::kPage) & 1u)) { if(tape_page) engine.SetFxBeforeLoop(false); page_key_armed_ = false; }
        RunActions(engine, recorder, sink);
        const Parameters& live = engine.GetParameters();
        if(live.Sampler()) menu_.FollowSampler(live.sample_mode, live.sample_bank, live.sample_slot);
        // Encoders: SW5 turn = cutoff, push and turn = looper speed (scrub when paused),
        // click = speed back to 1x; knobs 1-4 in TAPE's layout; SW6 = level / input gain.
        // Virtual turns are consumed with the hardware ones.
        int16_t turns[panel::kEncoders];
        for(unsigned i = 0; i < panel::kEncoders; ++i) { turns[i] = static_cast<int16_t>(hardware.turns[i] + virtual_turns_[i]); virtual_turns_[i] = 0; }
#ifdef FORGE_TEST_HOOKS
        for(unsigned i=0;i<panel::kEncoders;++i) {
            raw_turns_[i]+=uint32_t(hardware.turns[i]); turns_[i]+=uint32_t(turns[i]);
            if(turns[i]) LogEdge(InspectorEventKind::Knob, i, uint32_t(turns[i]));
        }
#endif
        // TAPE's menu page: the knobs are TAPE's shift layer (MenuControls).
        if(menu_.Active() && menu_.Page() == MenuPage::Samples) MenuControls(hardware, keys, rising, turns, engine);
        else {
            // SW5 (virtual: 2 = one click, 1 = held).
            const bool tone_down = hardware.tone_down || virtual_tone_ != 0;
            if(virtual_tone_ == 2) virtual_tone_ = 3; else if(virtual_tone_ == 3) virtual_tone_ = 0;   // click: down one block
            if(tone_down && !tone_was_down_) tone_turned_ = false;
            if(const int t = turns[panel::kToneEncoder]) {
                if(tone_down) {
                    tone_turned_ = true;
                    if(looper && looper->HasLoop()) {
                        if(looper->GetState() == Looper::State::Playing) TurnLoopSpeed(looper, t, !options_.quantise_menu);
                        else looper->Scrub(t);
                    }
                } else engine.Apply({Parameter::Cutoff, engine.Value(Parameter::Cutoff) + t * knobs::Step(4, Parameter::Cutoff)});
            }
            if(!tone_down && tone_was_down_ && !tone_turned_ && looper) looper->ResetSpeed();
            tone_was_down_ = tone_down;
            // Knobs 1-4. Presses act on release (TAPE); a 1.5 s hold resets; SW4 + SW3 = panic.
            const Parameters& patch = engine.GetParameters();
            const bool combo = hold_[0].down && hold_[3].down;
            if(combo) {
                combo_frames_ += hardware.frames;
                if(combo_frames_ >= panel::kPanicHoldFrames && !hold_[0].used) { engine.Panic(); hold_[0].used = hold_[3].used = true; }
            } else combo_frames_ = 0;
            for(unsigned knob = 0; knob < 4; ++knob) {
                KnobHold& h = hold_[knob];
                if(knob_page_[knob] >= knobs::Pages(knob, patch)) knob_page_[knob] = 0;   // a new patch has fewer pages
                const Parameter target = knobs::Target(knob, knob_page_[knob], patch);
                const bool down = (keys >> panel::kKnobEncoder[knob]) & 1u;
                const int increment = turns[panel::kKnobEncoder[knob]];
                if(down) {
                    if(!h.down) h = KnobHold{}, h.down = true; else h.frames += hardware.frames;
                    if(increment) h.turned = true;
                    if(!h.used && !h.turned && !combo && h.frames >= panel::kResetHoldFrames && !menu_.Active()) {
                        engine.ResetControl(target); h.used = true; reset_flash_[knob] = 48 * 300;
                    }
                } else if(h.down) {
                    if(!h.used) knob_page_[knob] = static_cast<uint8_t>((knob_page_[knob] + 1) % knobs::Pages(knob, patch));
                    h = KnobHold{};
                }
                if(reset_flash_[knob]) reset_flash_[knob] = reset_flash_[knob] > hardware.frames ? reset_flash_[knob] - hardware.frames : 0;
                if(increment && !menu_.Encoder(static_cast<uint8_t>(knob), increment)) {  // knob 1 picks the bank while the menu is open
                    if(target == Parameter::Speed && !options_.quantise_menu) TurnSpeed(engine, increment, true);
                    else engine.Apply({target, engine.Value(target) + increment * knobs::Step(knob, target)});
                }
            }
            const Parameter volume = volume_page_ ? Parameter::InputGain : Parameter::Level;
            if(turns[panel::kVolumeEncoder])
                engine.Apply({volume, engine.Value(volume) + turns[panel::kVolumeEncoder] * knobs::Step(5, volume)});
        }
        recorder.SetInputGain(engine.GetPerformance().input_gain);
        PublishKnobs(engine);
        if(!(menu_.Active() && menu_.Page() == MenuPage::Samples)) SendKnobCcs(engine, turns);
    }
    uint32_t MenuPacked() const { return menu_.Packed(); }
    void SetInstallGate(InstallGate* gate) { install_gate_ = gate; }
    // Knob n's page in bits 3n..3n+2.
    uint16_t KnobPages() const {
        return static_cast<uint16_t>(knob_page_[0] | knob_page_[1] << 3 | knob_page_[2] << 6 | knob_page_[3] << 9);
    }
    // Main loop (knob lights): pages, values and flags published once per block.
    uint32_t KnobValues() const { return knob_values_.load(std::memory_order_relaxed); }
    uint32_t KnobState() const { return knob_state_.load(std::memory_order_relaxed); }
    bool RecordPosition() const { return recording_position_.load(std::memory_order_relaxed); }
    uint8_t CountIn() const { return count_in_.load(std::memory_order_relaxed); }
    bool VolumePage() const { return volume_page_; }
    panel::MonitorMode Monitor() const { return monitor_; }
    // Main loop: the next MIDI message to send (TAPE's MIDI out).
    bool PopMidi(MidiOut& m) { return midi_out_.Pop(m); }
    const Options& GetOptions() const { return options_; }
    // TAPE's options.json (start-up): record latch, monitor position, which pitch mode is quantised.
    void SetOptions(const Options& o) {
        options_ = o; gesture_.SetLatch(o.record_latch); monitor_ = static_cast<panel::MonitorMode>(o.monitor % 3);
    }
    void SetMonitor(panel::MonitorMode mode) { monitor_ = mode; }
    // Main loop (lights): bit 0 TAPE menu page open, 1 sample loop on, 2 sample hold (sustain) on,
    // bits 3-4 monitor mode.
    uint8_t MenuLights() const { return menu_lights_.load(std::memory_order_relaxed); }
    RecordSource Source() const { return source_; }
    // Main loop (LED drawing): SW6 has been held long enough to show the battery.
    bool BatteryView() const { return battery_view_.load(std::memory_order_relaxed); }
    // Main loop (LED drawing): note keys held this block (physical and injected); switches 0-31
    // (32 bits: the Cortex-M7 has no lock-free 64-bit atomics).
    uint32_t KeysDown() const { return keys_down_.load(std::memory_order_relaxed); }
    const PresetMenu& Menu() const { return menu_; }
#ifdef FORGE_TEST_HOOKS
    void Inspect(InspectorAudio& a) const {
        a.physical_keys=physical_keys_; a.logical_keys=logical_keys_;
        a.physical_flags=physical_flags_; a.logical_flags=logical_flags_; a.menu=MenuPacked();
        a.record_source=static_cast<uint8_t>(source_); a.knob_pages=KnobPages();
        for(unsigned i=0;i<panel::kEncoders;++i) { a.raw_turns[i]=raw_turns_[i]; a.turns[i]=turns_[i]; }
    }
#endif
private:
    static constexpr uint32_t kBatteryHoldFrames = 48 * power::kBatteryHoldMs;   // 48 kHz
    struct KnobHold { bool down = false, turned = false, used = false; uint32_t frames = 0; };
    // Knob values 0..255 (8 bits each, knob 0 lowest); state: pages (12 bits), patch-page
    // flags (bits 12-15), reset flashes (16-19), SW6 page (20), effects-only patch (21),
    // SW6 value 0..255 (24-31).
    FORGE_COLD void PublishKnobs(const Engine& engine) {
        const Parameters& p = engine.GetParameters();
        uint32_t values = 0, state = KnobPages();
        for(unsigned k = 0; k < 4; ++k) {
            const float v = Clamp(engine.Value(knobs::Target(k, knob_page_[k], p)), 0.f, 1.f);
            values |= static_cast<uint32_t>(v * 255.f + .5f) << (8 * k);
            if(knobs::IsPatchPage(k, knob_page_[k], p)) state |= 1u << (12 + k);
            if(reset_flash_[k]) state |= 1u << (16 + k);
        }
        if(volume_page_) state |= 1u << 20;
        if(knobs::KindOf(p) == knobs::Kind::Effects) state |= 1u << 21;
        const float vol = Clamp(engine.Value(volume_page_ ? Parameter::InputGain : Parameter::Level), 0.f, 1.f);
        state |= static_cast<uint32_t>(vol * 255.f + .5f) << 24;
        knob_values_.store(values, std::memory_order_relaxed); knob_state_.store(state, std::memory_order_relaxed);
        const bool tape_menu = menu_.Active() && menu_.Page() == MenuPage::Samples;
        menu_lights_.store(static_cast<uint8_t>((tape_menu ? 1u : 0u) | (p.sample_loop ? 2u : 0u) | (p.sample_gate ? 4u : 0u)
                                                | (static_cast<unsigned>(monitor_) << 3)), std::memory_order_relaxed);
    }
    // TAPE's menu knob layer (MenuPage.h): turns and presses while the TAPE menu page is open.
    FORGE_COLD void MenuControls(const PanelInput& hardware, uint64_t keys, uint64_t rising, const int16_t (&turns)[panel::kEncoders], Engine& engine) {
        const Parameters& p = engine.GetParameters();
        for(unsigned knob = 0; knob < 4; ++knob) {
            KnobHold& h = hold_[knob];
            const bool down = (keys >> panel::kKnobEncoder[knob]) & 1u;
            if(down && !h.down) { h = KnobHold{}; h.down = true; h.used = true; MenuPress(knob, engine); }   // no page step after
            const int t = turns[panel::kKnobEncoder[knob]];
            if(!t) continue;
            const unsigned page = knob_page_[knob];
            auto nudge = [&](Parameter q, float step) { engine.Apply({q, engine.Value(q) + t * step}); };
            switch(knob) {
                case 0:
                    if(page == 0) TurnSpeed(engine, t, options_.quantise_menu);   // the other pitch mode (TAPE default: quantised)
                    else nudge(Parameter::Pan, .01f);
                    break;
                case 1: case 2:
                    if(page == 0 && p.Sampler()) {          // move the start-end window together (.03 per click)
                        const float start = p.sample_start, end = p.sample_end;      // p follows the engine: copy first
                        const float d = Clamp(t * .03f, -start, 1.f - end);
                        if(d > 0.f) { engine.Apply({Parameter::SampleEnd, end + d}); engine.Apply({Parameter::SampleStart, start + d}); }
                        else { engine.Apply({Parameter::SampleStart, start + d}); engine.Apply({Parameter::SampleEnd, end + d}); }
                    } else {                                // attack and release together (TAPE: attack and "decay")
                        const float v = Clamp(engine.Value(Parameter::Attack) + t * .03f, 0.f, 1.f);
                        engine.Apply({Parameter::Attack, v}); engine.Apply({Parameter::Release, v});
                    }
                    break;
                default:
                    if(page == 0) { nudge(Parameter::Time, .03f); engine.Apply({Parameter::ReverbSize, engine.Value(Parameter::Time)}); }
                    else if(page == 1) nudge(Parameter::Warble, .03f);
                    else nudge(Parameter::DjResonance, .03f);
                    break;
            }
        }
        for(unsigned knob = 0; knob < 4; ++knob) if(!((keys >> panel::kKnobEncoder[knob]) & 1u)) hold_[knob].down = false;
        // SW5: the looper's pitch (TAPE: free even while paused); click: back to 1x.
        Looper* looper = engine.GetLooper();
        if(looper && turns[panel::kToneEncoder]) TurnLoopSpeed(looper, turns[panel::kToneEncoder], options_.quantise_menu);
        if(looper && hardware.tone_down && !tone_was_down_) looper->ResetSpeed();
        tone_was_down_ = hardware.tone_down;
        // SW6: the output compressor; press: next monitor position.
        if(turns[panel::kVolumeEncoder])
            engine.Apply({Parameter::Compressor, engine.Value(Parameter::Compressor) + turns[panel::kVolumeEncoder] * .03f});
        if((rising >> panel::kVolumePress) & 1u) { monitor_ = static_cast<panel::MonitorMode>((static_cast<unsigned>(monitor_) + 1) % 3); volume_in_menu_ = true; }
    }
    // TAPE menu presses: SW4 resets pitch (page 1) or gain and pan (page 2); SW1 auto-loop
    // on/off; SW2 sustain (hold) on/off; SW3 resets every effect.
    FORGE_COLD void MenuPress(unsigned knob, Engine& engine) {
        switch(knob) {
            case 0:
                if(knob_page_[0] == 0) engine.ResetControl(Parameter::Speed);
                else { engine.ResetControl(Parameter::VoiceGain); engine.ResetControl(Parameter::Pan); }
                break;
            case 1: engine.ToggleSampleLoop(); break;
            case 2: engine.ToggleSampleHold(); break;
            default:
                for(Parameter q : {Parameter::Space, Parameter::Saturation, Parameter::DjFilter, Parameter::Time,
                                   Parameter::DjResonance, Parameter::Warble}) engine.ResetControl(q);
                break;
        }
    }
    // Pitch turns: free (.003 per click) or quantised (fifths/octaves, one step per 4 clicks).
    FORGE_COLD void TurnSpeed(Engine& engine, int t, bool quantised) {
        if(!quantised) { engine.Apply({Parameter::Speed, engine.Value(Parameter::Speed) + t * knobs::Step(0, Parameter::Speed)}); return; }
        speed_detents_ += t;
        while(speed_detents_ >= 4 || speed_detents_ <= -4) {
            const int dir = speed_detents_ > 0 ? 1 : -1; speed_detents_ -= 4 * dir;
            engine.Apply({Parameter::Speed, TapeSpeedKnob(QuantisedSpeedStep(TapeSpeedRatio(engine.GetPerformance().speed), dir))});
        }
    }
    FORGE_COLD void TurnLoopSpeed(Looper* looper, int t, bool quantised) {
        if(!quantised) { looper->NudgeSpeed(t * knobs::kLoopSpeedPerClick); return; }
        loop_detents_ += t;
        while(loop_detents_ >= 4 || loop_detents_ <= -4) {
            const int dir = loop_detents_ > 0 ? 1 : -1; loop_detents_ -= 4 * dir;
            looper->SetSpeed(QuantisedSpeedStep(looper->SpeedTarget(), dir));
        }
    }
    int speed_detents_ = 0, loop_detents_ = 0;
    Options options_;
    uint32_t page_key_frames_ = 0;
    bool page_key_armed_ = false, volume_in_menu_ = false;
    panel::MonitorMode monitor_ = panel::MonitorMode::Headphones;
    std::atomic<uint8_t> menu_lights_{0};
    // TAPE's MIDI out: cc_map[page][knob] for knobs 1-4 (TAPE's pages; Forge's extra pages
    // send nothing), CC 24 = SW5 (cutoff), CC 25 / 32 = SW6 (volume / input gain). A CC is
    // sent when that knob was turned (as TAPE: physical turns, not the menu) and its 7-bit value changed.
    // A keybed key to the engine, and what it played (a chord in harmony mode) to MIDI out.
    FORGE_COLD void PlayKey(Engine& engine, uint8_t note, uint8_t velocity) {
        uint8_t played[2 * harmony::kMaxNotes]; unsigned released = 0;
        const unsigned n = engine.Note(note, velocity, 2, played, &released);
        for(unsigned i = 0; i < n; ++i) {
            const bool off = i < released;
            SendMidi(off ? 0x80 : 0x90, played[i], off ? 0 : velocity);
        }
    }
    void SendMidi(uint8_t status, uint8_t data1, uint8_t data2) {
        if(!midi_out_.Push({static_cast<uint8_t>(status | (options_.midi_out & 15u)), data1, data2})) ++midi_out_drops_;
    }
    FORGE_COLD void SendKnobCcs(const Engine& engine, const int16_t (&turns)[panel::kEncoders]) {
        static const uint8_t cc_map[3][4] = {{20, 21, 22, 23}, {28, 29, 30, 31}, {0, 0, 0, 33}};
        const Parameters& p = engine.GetParameters();
        auto send = [&](unsigned slot, uint8_t cc, Parameter q) {
            if(!cc || !turns[slot < 4 ? panel::kKnobEncoder[slot] : slot == 4 ? panel::kToneEncoder : panel::kVolumeEncoder]) return;
            const uint8_t v = static_cast<uint8_t>(Clamp(engine.Value(q), 0.f, 1.f) * 127.f + .5f);
            if(v != cc_sent_[slot] || cc != cc_number_[slot]) { cc_sent_[slot] = v; cc_number_[slot] = cc; SendMidi(0xb0, cc, v); }
        };
        for(unsigned k = 0; k < 4; ++k) {
            const unsigned page = knob_page_[k];
            send(k, page < 3 && page < knobs::TapePages(k) ? cc_map[page][k] : 0, knobs::Target(k, page, p));
        }
        send(4, 24, Parameter::Cutoff);
        send(5, volume_page_ ? 32 : 25, volume_page_ ? Parameter::InputGain : Parameter::Level);
    }
    uint8_t cc_sent_[6] = {255, 255, 255, 255, 255, 255}, cc_number_[6]{};
    bool chompi_was_ = false;
    SpscQueue<MidiOut, 64> midi_out_;
    uint32_t midi_out_drops_ = 0;
    KnobHold hold_[4];
    uint32_t combo_frames_ = 0, reset_flash_[4]{};
    bool tone_was_down_ = false, tone_turned_ = false, volume_page_ = false;
    std::atomic<uint32_t> knob_values_{0}, knob_state_{0};
    std::atomic<bool> recording_position_{false};
    std::atomic<uint8_t> count_in_{0};
    uint32_t volume_held_ = 0;
    std::atomic<bool> battery_view_{false};
    std::atomic<uint32_t> keys_down_{0};
#ifdef FORGE_TEST_HOOKS
    uint64_t physical_keys_=0, logical_keys_=0;
    uint32_t raw_turns_[6]{}, turns_[6]{};
    uint8_t physical_flags_=0, logical_flags_=0;
    SpscQueue<InspectorEvent,64>* events_=nullptr; std::atomic<uint32_t>* event_drops_=nullptr;
    uint32_t event_time_=0; uint64_t event_keys_=0, event_virtual_keys_=0;
    void LogEdge(InspectorEventKind kind, uint8_t id, uint32_t value) {
        if(events_ && !events_->Push({0,event_time_,value,kind,id})) event_drops_->fetch_add(1,std::memory_order_relaxed);
    }
#endif
    FORGE_COLD void RunActions(Engine& engine, Recorder& recorder, PanelSink& sink) {
        for(MenuAction action; menu_.PopAction(action);) {
            using Kind = MenuAction::Kind;
            if(action.kind == Kind::SampleSelect) {                 // applied here, as TAPE does
                engine.ApplyPatch(SelectSample(engine.GetParameters(), action.mode, action.bank, action.slot), SlotPolicy::Select);
            } else if(action.kind == Kind::RecordSource) {
                source_ = static_cast<RecordSource>(action.slot);
            } else if(action.kind == Kind::SampleCopy && action.slot == PresetMenu::kLoopSource) {
                // Save the loop into a sample slot (locked while the file is written).
                Looper* looper = engine.GetLooper();
                forge::SampleJob job;
                job.kind = SampleJob::Kind::Save; job.from_loop = true;
                job.mode = action.to_mode; job.bank = action.to_bank; job.slot = action.to_slot;
                if(!looper || !looper->Lock()) { sink.Flash(false); continue; }
                job.frames = looper->Length(); job.gain = 1.f;
                if(!sink.SampleJob(job)) { looper->Unlock(); sink.Flash(false); }
            } else if(action.kind == Kind::SampleSave || action.kind == Kind::SampleErase || action.kind == Kind::SampleCopy) {
                forge::SampleJob job;
                job.mode = action.mode; job.bank = action.bank; job.slot = action.slot;
                job.to_mode = action.to_mode; job.to_bank = action.to_bank; job.to_slot = action.to_slot;
                job.kind = action.kind == Kind::SampleSave ? SampleJob::Kind::Save
                         : action.kind == Kind::SampleErase ? SampleJob::Kind::Erase : SampleJob::Kind::Copy;
                if(job.kind == SampleJob::Kind::Save) {
                    if(!recorder.Lock()) { sink.Flash(false); continue; }
                    job.frames = recorder.Length(); job.gain = recorder.Gain();
                }
                if(!sink.SampleJob(job)) {
                    if(job.kind == SampleJob::Kind::Save) recorder.Unlock();
                    sink.Flash(false);
                }
            } else {
                sink.PresetAction(action, engine.GetParameters());   // full queue: dropped, LEDs show no change
            }
        }
    }
    PresetMenu menu_;
    RecordGesture gesture_;
    RecordSource source_ = RecordSource::Line;
    uint64_t prev_keys_ = 0, virtual_keys_ = 0;
    int8_t toggle_override_ = -1, jack_override_ = -1;
    bool first_ = true, jack_ = false;
    uint8_t virtual_tone_ = 0;
    int16_t virtual_turns_[panel::kEncoders]{};
    uint8_t knob_page_[4]{};
    InstallGate* install_gate_ = nullptr;
};

// Everything the key LEDs and the CHOMPI key LED show (main loop, pure), as
// drawn by the firmware and reported by the development probe.
struct LedView {
    uint32_t menu = 0;                            // PresetMenu::Packed()
    uint16_t preset_occupancy = 0;                // shown preset bank
    bool preset_card = false;
    uint8_t last_bank = 0, last_slot = panel::kNoSlot;
    uint16_t sample_occupancy = 0;                // shown sample mode/bank
    bool sample_card = false, recording_present = false, recording_now = false;
    uint32_t live = 0;                            // PackSelection of the playing patch
    bool blink = false, slow_blink = false;       // 250 ms / 300 ms phases
    int8_t flash = -1;                            // -1 none, 0 failed, 1 ok (panel LED feedback)
    bool saving = false;                          // a sample save/copy is running
    uint32_t looper = 0;                          // PackLooper()
    uint8_t install = 0;                          // 1 waiting for the CHOMPI press, 2 restarting
    uint64_t keys_down = 0;                       // panel keys held (PanelController::KeysDown)
    bool record_position = false;                 // toggle in the record position (TAPE: switch_state false)
    uint8_t count_in = 0;                         // RecordGesture::CountInPhase (0 none, 1..6)
    float input_level = 0.f;                      // input meter 0..1 (record position)
    uint16_t kit_occupancy = 0;                   // sample files of the live kit bank
};
// Menu closed (TAPE NormalPage): a key lights white while held. With a sampler patch,
// kit mode shows the bank's occupied slots dim in the bank colour (the recording key
// dim pink); chromatic mode marks C3, C4 and C5 dim (pink when playing the recording).
inline void RenderPlayLeds(uint64_t keys_down, uint32_t live, uint16_t kit_occupancy, bool recording, Rgb (&leds)[25]) {
    for(auto& led : leds) led = Rgb{};
    auto dim = [](Rgb c) { return Rgb{c.r * .25f, c.g * .25f, c.b * .25f}; };
    const Rgb pink{1, .2f, .6f};
    if(live & 1u) {
        const bool kit = (live >> 1) & 1u;
        const uint8_t bank = (live >> 2) & 7u, slot = (live >> 5) & 15u;
        const Rgb colour = !kit && slot == kRamSlot ? pink : SampleBankColour(bank);
        if(kit) {
            for(uint8_t s = 0; s < kRamSlot; ++s) if((kit_occupancy >> s) & 1u) leds[panel::SlotLed(s)] = dim(colour);
            if(recording) leds[panel::SlotLed(kRamSlot)] = dim(pink);
        } else for(uint8_t s : {uint8_t(0), uint8_t(7), uint8_t(14)}) leds[panel::SlotLed(s)] = dim(colour);
    }
    for(uint8_t key = 0; key < panel::kButtons; ++key) {
        if(!panel::kKeyNotes[key] || !((keys_down >> key) & 1u)) continue;
        const uint8_t slot = panel::KeyToSlot(key);
        const uint8_t led = slot != panel::kNoSlot ? panel::SlotLed(slot) : panel::BlackLed(key);
        if(led < 25) leds[led] = Rgb{1, 1, 1};
    }
}
// Looper state for the LEDs (audio -> main loop in one word): bits 0-2 state,
// 3 overdub, 4 effects before the loop, 5 has a loop, 6-15 position x 1023.
inline uint32_t PackLooper(const Looper& l, bool fx_before) {
    return static_cast<uint32_t>(l.GetState()) | (l.Overdubbing() ? 8u : 0u) | (fx_before ? 16u : 0u)
         | (l.HasLoop() ? 32u : 0u) | (static_cast<uint32_t>(l.Position() * 1023.f) << 6);
}
// PLAY / LOOP LEDs (as TAPE). PLAY: off empty, white armed, teal recording or
// playing (fading over the loop), white fading when paused. LOOP: red first
// take, red blink armed, yellow overdub, white otherwise (rising over the loop).
inline void ComposeLooperLeds(uint32_t looper, bool blink, Rgb& play, Rgb& loop) {
    const auto state = static_cast<Looper::State>(looper & 7u);
    const float position = ((looper >> 6) & 1023u) / 1023.f;
    const Rgb teal{0.f, .7f, .6f}, red{1.f, 0.f, 0.f}, yellow{1.f, .75f, 0.f}, white{1.f, 1.f, 1.f};
    auto scaled = [](Rgb c, float k) { return Rgb{c.r * k, c.g * k, c.b * k}; };
    play = loop = Rgb{};
    switch(state) {
        case Looper::State::Armed: play = white; loop = blink ? red : Rgb{}; break;
        case Looper::State::FirstTake: play = teal; loop = red; break;
        case Looper::State::Playing:
            play = scaled(teal, 1.f - position);
            loop = scaled((looper >> 3) & 1u ? yellow : white, position);
            break;
        case Looper::State::Paused: play = scaled(white, 1.f - position); loop = scaled(white, position); break;
        default: break;
    }
}
// SW5's two lights (TAPE): with a loop playing, the speed (LED 6 forward, LED 5
// reverse, blue -> green -> yellow -> red with speed, red spilling over near the ends);
// dimmed in the record position.
inline void ComposeTransportLeds(bool playing, float speed, bool record_position, Rgb& reverse, Rgb& forward) {
    reverse = forward = Rgb{};
    if(!playing) return;
    const float v = Clamp((speed + 2.f) * .25f, 0.f, 1.f);
    const float idx = v < .5f ? v * 2.f : (1.f - v) * 2.f;
    using namespace knobs::colour;
    Rgb on = knobs::Fade4(med_blue, green, yellow, red, idx), off{};
    if(idx > .8f) off = knobs::Scale(red, (idx - .8f) * 5.f);
    if(record_position) { on = knobs::Scale(on, .7f); off = knobs::Scale(off, .7f); }
    if(v > .5f) { forward = on; reverse = off; } else { reverse = on; forward = off; }
}
// Knob ring LEDs (TAPE): each knob's colour from its page and value (knob_layout.h);
// a reset flashes white; in the record position knob lights 1-4 are off (TAPE).
inline void ComposeKnobLeds(uint32_t values, uint32_t state, bool record_position, Rgb (&rings)[4]) {
    for(unsigned k = 0; k < 4; ++k) {
        const unsigned page = (state >> (3 * k)) & 7u;
        rings[k] = knobs::KnobColour(k, page, ((values >> (8 * k)) & 255u) / 255.f, (state >> (12 + k)) & 1u, (state >> 21) & 1u);
        if((state >> (16 + k)) & 1u) rings[k] = Rgb{1.f, 1.f, 1.f};
        else if(record_position) rings[k] = Rgb{};
    }
}
// TAPE's menu page (MenuLights bit 0): SW1 white while auto-loop is on, SW2 white while
// sustain is on (dim otherwise); SW6 shows the monitor position (orange headphones, blue
// both, yellow send/return).
inline void ComposeMenuKnobLeds(uint8_t lights, Rgb (&rings)[4], Rgb& volume) {
    if(!(lights & 1u)) return;
    const Rgb on{1.f, 1.f, 1.f}, off{.08f, .08f, .08f};
    rings[1] = (lights >> 1) & 1u ? on : off;
    rings[2] = (lights >> 2) & 1u ? on : off;
    static const Rgb monitor[3] = {knobs::colour::orange, knobs::colour::blue, knobs::colour::yellow};
    volume = monitor[((lights >> 3) & 3u) % 3];
}
inline void ComposeLeds(const LedView& v, Rgb (&keys)[25], Rgb& chompi) {
    if(!(v.menu & 1u))
        RenderPlayLeds(v.keys_down, v.live, v.kit_occupancy, v.recording_present, keys);
    else if((v.menu >> 21) & 1u)
        RenderSampleLeds(v.menu, v.sample_occupancy, v.sample_card, v.recording_present, v.live, v.blink, keys);
    else
        RenderMenuLeds(v.menu, v.preset_occupancy, v.preset_card, v.last_bank, v.last_slot, v.blink, keys);
    // CHOMPI key LED (as TAPE): red recording, flash after an action, pink blink while
    // saving; otherwise the input meter in the record position, purple while held in
    // the menu position. Forge's count-in: CHOMPI and the white keys blink red 3 times.
    const bool count_lit = v.count_in & 1u;
    if(v.recording_now) chompi = Rgb{1.f, 0.f, 0.f};
    else if(v.count_in) {
        chompi = count_lit ? Rgb{1.f, 0.f, 0.f} : Rgb{};
        if(!(v.menu & 1u)) for(uint8_t s = 0; s < 15; ++s) keys[panel::SlotLed(s)] = count_lit ? Rgb{1.f, 0.f, 0.f} : Rgb{};
    }
    else if(v.flash >= 0) chompi = v.flash ? Rgb{0.f, .3f, 0.f} : Rgb{.3f, 0.f, 0.f};
    else if(v.saving) chompi = v.slow_blink ? Rgb{1.f, 0.f, .6f} : Rgb{};
    else if(v.record_position)
        chompi = knobs::Fade4(Rgb{.1f, .1f, .1f}, knobs::colour::green, knobs::colour::yellow, knobs::colour::pink, Clamp(v.input_level, 0.f, 1.f));
    else chompi = (v.keys_down >> panel::kChompiKey) & 1u ? knobs::colour::purple : Rgb{};
    // Firmware install: CHOMPI blinks white until pressed, then stays white while CHOMPI restarts.
    if(v.install == 1) chompi = v.blink ? Rgb{1.f, 1.f, 1.f} : Rgb{};
    else if(v.install == 2) chompi = Rgb{1.f, 1.f, 1.f};
    // Menu, presets page: KEY_21 / KEY_20 show where the effects sit (before / after the loop).
    if((v.menu & 1u) && !((v.menu >> 21) & 1u)) {
        const bool before = (v.looper >> 4) & 1u;
        keys[panel::BlackLed(panel::kFxBefore)] = before ? Rgb{.8f, .8f, .8f} : Rgb{.1f, .1f, .1f};
        keys[panel::BlackLed(panel::kFxAfter)] = before ? Rgb{.1f, .1f, .1f} : Rgb{.8f, .8f, .8f};
    }
}
} // namespace forge
