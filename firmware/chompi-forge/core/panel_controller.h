#pragma once
#include <cstdint>
#include "engine.h"
#include "file_transfer.h"
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
// Knob pages (docs/forge/KNOBS.md). Pressing logical knob n (its encoder switch,
// button kKnobEncoder[n]) steps its page; page 0 = the patch's knob, 1-3 fixed.
// Knob n's ring LED is through-hole LED n + 1 (ENC_4, ENC_1, ENC_2, ENC_3 as TAPE).
constexpr unsigned kKnobPages = 4;
constexpr uint8_t kKnobLed[4] = {1, 2, 3, 4};
inline Parameter KnobPageParameter(unsigned knob, unsigned page, bool sampler) {
    static const Parameter pages[4][3] = {
        {Parameter::Cutoff, Parameter::Resonance, Parameter::FilterAmount},     // knob 1: filter
        {Parameter::Attack, Parameter::Decay, Parameter::Release},              // knob 2: envelope
        {Parameter::LfoRate, Parameter::LfoFilter, Parameter::Osc2Detune},      // knob 3: movement
        {Parameter::Mix, Parameter::Feedback, Parameter::ReverbMix}};           // knob 4: space
    if(page == 0 || page >= kKnobPages) return static_cast<Parameter>(static_cast<unsigned>(Parameter::Knob1) + (knob & 3));
    if(sampler && knob == 2 && page == 3) return Parameter::SampleXfade;      // no second oscillator
    return pages[knob & 3][page - 1];
}
} // namespace panel

// One block of debounced hardware input (audio owner).
struct PanelInput {
    uint64_t keys = 0;                   // bit k: button k held (upstream SwId order)
    bool toggle_up = false, jack = false;
    bool tone_press = false;             // SW5 switch rising edge this block
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
            case PanelEvent::Kind::Press: virtual_press_ = true; break;
            case PanelEvent::Kind::Toggle: toggle_override_ = e.value; break;
            case PanelEvent::Kind::Jack: jack_override_ = e.value; break;
            case PanelEvent::Kind::Release:
                virtual_keys_ = 0; toggle_override_ = jack_override_ = -1; virtual_press_ = false;
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

    void Block(const PanelInput& hardware, Engine& engine, Recorder& recorder, PanelSink& sink) {
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
        physical_flags_=(hardware.toggle_up?1:0)|(hardware.jack?2:0)|(hardware.tone_press?4:0);
        logical_flags_=(toggle_up?1:0)|(jack?2:0)|((hardware.tone_press||virtual_press_)?4:0)|(Overridden()?8:0);
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
        switch(gesture_.Update(toggle_up, chompi)) {
            case RecordGesture::Event::Start:
                if(!recorder.Start()) { gesture_.Cancel(); sink.Flash(false); }
                break;
            case RecordGesture::Event::Stop: {
                recorder.Stop();
                const Parameters& p = engine.GetParameters();
                engine.ApplyPatch(SelectSample(p, 0, p.sample_bank, kRamSlot));
                break;
            }
            default: break;
        }
        const uint64_t rising = keys & ~prev_keys_, falling = prev_keys_ & ~keys;
        prev_keys_ = keys;
        for(unsigned key = 0; key < panel::kButtons; ++key) if(panel::kKeyNotes[key]) {
            if(((rising >> key) & 1u) && !menu_.Key(static_cast<uint8_t>(key), true)) engine.Note(panel::kKeyNotes[key], 100, 2);
            if((falling >> key) & 1u) { menu_.Key(static_cast<uint8_t>(key), false); engine.Note(panel::kKeyNotes[key], 0, 2); }
        }
        Looper* looper = engine.GetLooper();
        if(looper) {
            // PLAY / LOOP. In the menu they set the overdub feedback (-/+ 10 %, as TAPE);
            // releases always reach the looper so no key can stay held there.
            const bool presets_page = menu_.Active() && !((menu_.Packed() >> 21) & 1u);
            for(const uint8_t key : {panel::kPlayKey, panel::kLoopKey}) {
                const bool play = key == panel::kPlayKey;
                if((rising >> key) & 1u) {
                    if(menu_.Active()) { if(play || !menu_.SelectLoopSource()) looper->AdjustFeedback(play ? -0.1f : 0.1f); }
                    else if(play) looper->Play(true); else looper->Loop(true);
                }
                if((falling >> key) & 1u) { if(play) looper->Play(false); else looper->Loop(false); }
            }
            if(presets_page && ((rising >> panel::kFxBefore) & 1u)) engine.SetFxBeforeLoop(true);
            if(presets_page && ((rising >> panel::kFxAfter) & 1u)) engine.SetFxBeforeLoop(false);
            looper->Tick(hardware.frames);
        }
        RunActions(engine, recorder, sink);
        const Parameters& live = engine.GetParameters();
        if(live.Sampler()) menu_.FollowSampler(live.sample_mode, live.sample_bank, live.sample_slot);
        // Encoders: SW5 turn = cutoff, press = panic (while a loop exists: the
        // looper transport, as TAPE); knobs 1-4 in stock order; SW6 = level.
        // Virtual turns are consumed with the hardware ones.
        int16_t turns[panel::kEncoders];
        for(unsigned i = 0; i < panel::kEncoders; ++i) { turns[i] = static_cast<int16_t>(hardware.turns[i] + virtual_turns_[i]); virtual_turns_[i] = 0; }
#ifdef FORGE_TEST_HOOKS
        for(unsigned i=0;i<panel::kEncoders;++i) {
            raw_turns_[i]+=uint32_t(hardware.turns[i]); turns_[i]+=uint32_t(turns[i]);
            if(turns[i]) LogEdge(InspectorEventKind::Knob, i, uint32_t(turns[i]));
        }
#endif
        const bool press = hardware.tone_press || virtual_press_; virtual_press_ = false;
        if(looper && looper->HasLoop()) {
            if(press) looper->ResetSpeed();
            if(turns[panel::kToneEncoder]) {
                if(looper->GetState() == Looper::State::Playing) looper->NudgeSpeed(turns[panel::kToneEncoder]);
                else looper->Scrub(turns[panel::kToneEncoder]);
            }
        } else {
            if(press) engine.Panic();
            if(turns[panel::kToneEncoder])
                engine.Apply({Parameter::Cutoff, engine.GetParameters().cutoff + turns[panel::kToneEncoder] / 127.f});
        }
        for(unsigned knob = 0; knob < 4; ++knob) {
            // Encoder switches 0-3 (no note): a press steps the knob's page.
            if((rising >> panel::kKnobEncoder[knob]) & 1u) knob_page_[knob] = static_cast<uint8_t>((knob_page_[knob] + 1) % panel::kKnobPages);
            const int increment = turns[panel::kKnobEncoder[knob]];
            if(increment && !menu_.Encoder(static_cast<uint8_t>(knob), increment)) {   // knob 1 picks the bank while the menu is open
                const Parameters& p = engine.GetParameters();
                const Parameter target = panel::KnobPageParameter(knob, knob_page_[knob], p.Sampler());
                engine.Apply({target, p.Value(target) + increment / 127.f});
            }
        }
        if(turns[panel::kVolumeEncoder])
            engine.Apply({Parameter::Level, engine.GetParameters().level + turns[panel::kVolumeEncoder] / 127.f});
    }
    uint32_t MenuPacked() const { return menu_.Packed(); }
    void SetInstallGate(InstallGate* gate) { install_gate_ = gate; }
    // Knob n's page in bits 2n..2n+1.
    uint8_t KnobPages() const {
        return static_cast<uint8_t>(knob_page_[0] | knob_page_[1] << 2 | knob_page_[2] << 4 | knob_page_[3] << 6);
    }
    RecordSource Source() const { return source_; }
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
    FORGE_NOINLINE void RunActions(Engine& engine, Recorder& recorder, PanelSink& sink) {
        for(MenuAction action; menu_.PopAction(action);) {
            using Kind = MenuAction::Kind;
            if(action.kind == Kind::SampleSelect) {                 // applied here, as TAPE does
                engine.ApplyPatch(SelectSample(engine.GetParameters(), action.mode, action.bank, action.slot));
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
    bool first_ = true, jack_ = false, virtual_press_ = false;
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
};
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
// Knob ring LEDs: each knob's page as a colour (page 1 dim white, 2 red, 3 green, 4 blue).
inline void ComposeKnobLeds(uint8_t pages, Rgb (&knobs)[4]) {
    static const Rgb colours[panel::kKnobPages] = {{.12f, .12f, .12f}, {.6f, 0.f, 0.f}, {0.f, .5f, 0.f}, {0.f, 0.f, .7f}};
    for(unsigned k = 0; k < 4; ++k) knobs[k] = colours[(pages >> (2 * k)) & 3u];
}
inline void ComposeLeds(const LedView& v, Rgb (&keys)[25], Rgb& chompi) {
    if((v.menu >> 21) & 1u)
        RenderSampleLeds(v.menu, v.sample_occupancy, v.sample_card, v.recording_present, v.live, v.blink, keys);
    else
        RenderMenuLeds(v.menu, v.preset_occupancy, v.preset_card, v.last_bank, v.last_slot, v.blink, keys);
    // CHOMPI key LED (as TAPE): red recording, flash after an action, pink blink while saving.
    if(v.recording_now) chompi = Rgb{1.f, 0.f, 0.f};
    else if(v.flash >= 0) chompi = v.flash ? Rgb{0.f, .3f, 0.f} : Rgb{.3f, 0.f, 0.f};
    else if(v.saving) chompi = v.slow_blink ? Rgb{1.f, 0.f, .6f} : Rgb{};
    else chompi = Rgb{0.f, .05f, .1f};
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
