#pragma once
#include <cstdint>
#include "protocol.h"

namespace forge {
// CHOMPI panel IDs (upstream Hardware::SwId order) used by the preset menu.
namespace panel {
constexpr uint8_t kChompiKey = 5, kToggle = 6;
constexpr uint8_t kBankDown = 7, kBankUp = 12;            // black KEY_16 / KEY_17
constexpr uint8_t kErase = 29, kCopy = 30, kSave = 31;    // black KEY_23 / 24 / 25, as in TAPE
// Samples page (TAPE's shift menu): KEY_16 chromatic (JAMMI) / KEY_17 kit
// (CUBBI) share the bank keys' IDs; KEY_18/19/20 pick the record source.
constexpr uint8_t kChromatic = 7, kKit = 12, kMic = 13, kLine = 14, kResample = 21;
constexpr uint8_t kPage = 23;                              // black KEY_22: Presets <-> Samples
constexpr uint8_t kNoSlot = 0xff;
// Stock logical knob order: TAPE/TEMPO/WAVE ui.h `encoder_map = {1, 2, 3, 0, 4, 5}`
// maps hardware encoder SW1..SW6 to logical knob 1, 2, 3, 0, 4, 5, and CC20+n
// turns logical knob n. Logical knob n is hardware encoder kKnobEncoder[n].
constexpr uint8_t kKnobEncoder[6] = {3, 0, 1, 2, 4, 5};
// White keys KEY_1..KEY_15 -> slot 0..14 (TAPE's KeyToSlot, 0-based).
inline uint8_t KeyToSlot(uint8_t button) {
    if(button >= 8 && button <= 11) return button - 7;    // KEY_2..KEY_5 -> 1..4
    if(button == 15) return 0;                             // KEY_1
    if(button >= 16 && button <= 20) return button - 11;  // KEY_6..KEY_10 -> 5..9
    if(button >= 24 && button <= 28) return button - 14;  // KEY_11..KEY_15 -> 10..14
    return kNoSlot;
}
inline uint8_t SlotLed(uint8_t slot) { return 24 - slot; }  // SMT LED under white key (TAPE: 25 - slot#)
inline uint8_t BlackLed(uint8_t button) {                   // SMT LED under black key KEY_16..KEY_25
    switch(button) {
        case 7: return 0; case 12: return 1; case 13: return 2; case 14: return 3; case 21: return 4;
        case 22: return 5; case 23: return 6; case 29: return 7; case 30: return 8; case 31: return 9;
        default: return 0xff;
    }
}
} // namespace panel

// TAPE-style menu (audio owner; no I/O). Toggle up + press the CHOMPI key
// opens it; KEY_22 switches between the Presets and Samples pages. While
// open, keys do not play notes. Presets page:
//   white key            recall that slot of the current bank (hold CHOMPI)
//   KEY_16 / KEY_17       bank down / up (wraps); knob 0 (hardware SW4) also turns banks
//   KEY_25 save, KEY_23 erase: choose the mode, pick a white key, press CHOMPI
//   KEY_24 copy: pick source, then destination (any bank), press CHOMPI
// Samples page (TAPE's shift menu; banks a-e per mode):
//   KEY_16 / KEY_17       chromatic / kit: select it, press again for the next bank
//   white key            chromatic: play that slot (KEY_15 = the recording);
//                         kit: play that bank. With a function key: choose the slot
//   KEY_18/19/20          record from mic / line in / resample
//   KEY_25 save the recording, KEY_23 erase, KEY_24 copy (any mode/bank), confirm with CHOMPI
// Pressing an active function key again cancels it. The menu closes when the
// CHOMPI key is released with nothing pending, or when the toggle goes down.
// SD work is queued for the main loop; sample selection and source changes
// are applied by the audio owner directly.
struct MenuAction {
    enum class Kind : uint8_t { Recall, Save, Erase, Copy,
                                SampleSelect, SampleSave, SampleErase, SampleCopy, RecordSource } kind = Kind::Recall;
    uint8_t bank = 0, slot = 0, to_bank = 0, to_slot = 0;
    uint8_t mode = 0, to_mode = 0;   // sample actions: 0 chromatic, 1 kit; RecordSource: slot = source
};
enum class MenuPage : uint8_t { Presets, Samples };
enum class MenuMode : uint8_t { None, Save, Erase, CopySource, CopyDest };

class PresetMenu {
public:
    // Once per audio block with debounced inputs.
    void Update(bool toggle_up, bool chompi_down) {
        const bool rising = chompi_down && !chompi_;
        chompi_ = chompi_down;
        if(!toggle_up) { Close(); return; }
        if(!active_) {
            if(rising) { active_ = true; mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; loop_source_ = false; }
            return;
        }
        if(rising) Confirm();
        else if(!chompi_down && mode_ == MenuMode::None) Close();
    }
    // Key edge. Returns true if the menu consumed it (the caller must not play
    // a note). Releases are never consumed, so notes held before the menu opened
    // still end.
    FORGE_NOINLINE bool Key(uint8_t button, bool pressed) {
        if(!active_ || !pressed) return false;
        if(button == panel::kPage) {
            page_ = page_ == MenuPage::Presets ? MenuPage::Samples : MenuPage::Presets;
            mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; loop_source_ = false;
            return true;
        }
        const uint8_t slot = panel::KeyToSlot(button);
        if(page_ == MenuPage::Samples) return SampleKey(button, slot);
        if(slot != panel::kNoSlot) { Slot(slot); return true; }
        switch(button) {
            case panel::kBankDown: bank_ = static_cast<uint8_t>((bank_ + kPresetBanks - 1) % kPresetBanks); return true;
            case panel::kBankUp: bank_ = static_cast<uint8_t>((bank_ + 1) % kPresetBanks); return true;
            case panel::kSave: Toggle(MenuMode::Save); return true;
            case panel::kErase: Toggle(MenuMode::Erase); return true;
            case panel::kCopy: Toggle(MenuMode::CopySource); return true;
            default: return BlackKey(button);
        }
    }
    // Logical knob turn; knob 0 (CC20's knob, hardware SW4) selects the bank while the menu is open.
    bool Encoder(uint8_t index, int increment) {
        if(!active_ || index != 0 || !increment) return false;
        if(page_ == MenuPage::Samples) {                   // knob 1 turns the current mode's bank
            const int bank = sample_bank_[sample_mode_] + increment;
            sample_bank_[sample_mode_] = static_cast<uint8_t>(bank < 0 ? 0 : bank >= kSampleBanks ? kSampleBanks - 1 : bank);
            return true;
        }
        int bank = bank_ + increment;
        bank_ = static_cast<uint8_t>(bank < 0 ? 0 : bank >= kPresetBanks ? kPresetBanks - 1 : bank);
        return true;
    }
    bool PopAction(MenuAction& out) {
        if(head_ == tail_) return false;
        out = actions_[tail_]; tail_ = (tail_ + 1) % kActions;
        return true;
    }
    bool Active() const { return active_; }
    MenuPage Page() const { return page_; }
    uint8_t SampleMode() const { return sample_mode_; }
    uint8_t SampleBank() const { return sample_bank_[sample_mode_]; }
    // Keeps the samples page in step with the live patch (e.g. after a recall).
    void FollowSampler(uint8_t mode, uint8_t bank, uint8_t slot) {
        if(mode > 1 || bank >= kSampleBanks) return;
        sample_mode_ = mode; sample_bank_[mode] = bank; if(mode == 0) chromatic_slot_ = slot;
    }
    void SetRecordSource(uint8_t source) { if(source < 3) record_source_ = source; }
    MenuMode Mode() const { return mode_; }
    uint8_t Bank() const { return bank_; }
    void SetBank(uint8_t bank) { if(bank < kPresetBanks) bank_ = bank; }
    // Compact state for the main loop's LED drawing (one atomic word).
    // Samples page: bank_ fields hold the sample bank; bits 21-29 add the page,
    // sample mode, selected/source modes and the record source.
    // Samples page, COPY: the LOOP key picks the loop as the source (TAPE's
    // "slot 16"); then a white key and CHOMPI save the loop into that slot.
    static constexpr uint8_t kLoopSource = 16;
    bool SelectLoopSource() {
        if(!active_ || page_ != MenuPage::Samples || mode_ != MenuMode::CopySource) return false;
        selected_ = source_ = panel::kNoSlot; loop_source_ = true; mode_ = MenuMode::CopyDest;
        return true;
    }
    bool LoopSource() const { return loop_source_; }
    uint32_t Packed() const {
        const bool samples = page_ == MenuPage::Samples;
        return (active_ ? 1u : 0u) | (static_cast<uint32_t>(mode_) << 1)
            | (static_cast<uint32_t>(samples ? sample_bank_[sample_mode_] : bank_) << 4)
            | (static_cast<uint32_t>(selected_ & 0xf) << 7) | (static_cast<uint32_t>(selected_bank_) << 11)
            | (static_cast<uint32_t>(source_ & 0xf) << 14) | (static_cast<uint32_t>(source_bank_) << 18)
            | (samples ? 1u << 21 : 0u) | (static_cast<uint32_t>(sample_mode_) << 22)
            | (static_cast<uint32_t>(selected_mode_) << 23) | (static_cast<uint32_t>(source_mode_) << 24)
            | (static_cast<uint32_t>(record_source_) << 25) | (static_cast<uint32_t>(chromatic_slot_ & 0xf) << 27)
            | (loop_source_ ? 1u << 31 : 0u);
    }
private:
    static constexpr unsigned kActions = 4;
    bool BlackKey(uint8_t button) { return panel::BlackLed(button) != 0xff; }  // other black keys: swallowed
    void Close() { active_ = false; mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; loop_source_ = false; }
    FORGE_NOINLINE bool SampleKey(uint8_t button, uint8_t slot) {
        if(slot != panel::kNoSlot) {
            if(mode_ == MenuMode::None) {
                if(sample_mode_ == 0) chromatic_slot_ = slot;
                Push(SampleSelectAction());
            } else if(slot != kRamSlot) {                  // the recording is not a file slot
                if(mode_ == MenuMode::CopySource) {
                    source_ = slot; source_bank_ = sample_bank_[sample_mode_]; source_mode_ = sample_mode_;
                    mode_ = MenuMode::CopyDest; selected_ = panel::kNoSlot;
                } else { selected_ = slot; selected_bank_ = sample_bank_[sample_mode_]; selected_mode_ = sample_mode_; }
            }
            return true;
        }
        switch(button) {
            case panel::kChromatic: case panel::kKit: {
                const uint8_t mode = button == panel::kKit ? 1 : 0;
                if(sample_mode_ == mode) sample_bank_[mode] = static_cast<uint8_t>((sample_bank_[mode] + 1) % kSampleBanks);
                sample_mode_ = mode;
                if(mode_ == MenuMode::None) Push(SampleSelectAction());   // the instrument follows, as in TAPE
                return true;
            }
            case panel::kMic: case panel::kLine: case panel::kResample:
                record_source_ = button == panel::kMic ? 0 : button == panel::kLine ? 1 : 2;
                Push({MenuAction::Kind::RecordSource, 0, record_source_, 0, 0, 0, 0});
                return true;
            case panel::kSave: Toggle(MenuMode::Save); return true;
            case panel::kErase: Toggle(MenuMode::Erase); return true;
            case panel::kCopy: Toggle(MenuMode::CopySource); return true;
            default: return BlackKey(button);
        }
    }
    MenuAction SampleSelectAction() const {
        MenuAction a; a.kind = MenuAction::Kind::SampleSelect; a.mode = sample_mode_;
        a.bank = sample_bank_[sample_mode_]; a.slot = chromatic_slot_;
        return a;
    }
    void Toggle(MenuMode mode) {
        const bool same = mode_ == mode || (mode == MenuMode::CopySource && mode_ == MenuMode::CopyDest);
        if(mode_ != MenuMode::None && !same) return;    // finish or cancel the current mode first
        mode_ = same ? MenuMode::None : mode;
        selected_ = source_ = panel::kNoSlot; loop_source_ = false;
    }
    void Slot(uint8_t slot) {
        switch(mode_) {
            case MenuMode::None: Push({MenuAction::Kind::Recall, bank_, slot, 0, 0}); break;
            case MenuMode::Save: case MenuMode::Erase: case MenuMode::CopyDest:
                selected_ = slot; selected_bank_ = bank_; break;
            case MenuMode::CopySource:
                source_ = slot; source_bank_ = bank_; mode_ = MenuMode::CopyDest; selected_ = panel::kNoSlot; break;
        }
    }
    FORGE_NOINLINE void Confirm() {
        if(selected_ == panel::kNoSlot) return;
        if(page_ == MenuPage::Samples) {
            MenuAction a; a.mode = selected_mode_; a.bank = selected_bank_; a.slot = selected_;
            if(mode_ == MenuMode::Save) a.kind = MenuAction::Kind::SampleSave;
            else if(mode_ == MenuMode::Erase) a.kind = MenuAction::Kind::SampleErase;
            else if(mode_ == MenuMode::CopyDest) {
                if(!loop_source_ && source_mode_ == selected_mode_ && source_bank_ == selected_bank_ && source_ == selected_) return;
                a.kind = MenuAction::Kind::SampleCopy; a.to_mode = selected_mode_; a.to_bank = selected_bank_; a.to_slot = selected_;
                a.mode = source_mode_; a.bank = source_bank_; a.slot = loop_source_ ? kLoopSource : source_;
            } else return;
            Push(a);
            mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; loop_source_ = false;
            return;
        }
        if(mode_ == MenuMode::Save) Push({MenuAction::Kind::Save, selected_bank_, selected_, 0, 0});
        else if(mode_ == MenuMode::Erase) Push({MenuAction::Kind::Erase, selected_bank_, selected_, 0, 0});
        else if(mode_ == MenuMode::CopyDest) {
            if(source_bank_ == selected_bank_ && source_ == selected_) return;
            Push({MenuAction::Kind::Copy, source_bank_, source_, selected_bank_, selected_});
        } else return;
        mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; loop_source_ = false;
    }
    void Push(const MenuAction& action) {
        const unsigned next = (head_ + 1) % kActions;
        if(next == tail_) return;                 // main loop busy: drop, LEDs show nothing happened
        actions_[head_] = action; head_ = next;
    }
    MenuAction actions_[kActions]{};
    unsigned head_ = 0, tail_ = 0;
    bool active_ = false, chompi_ = false;
    MenuMode mode_ = MenuMode::None;
    uint8_t bank_ = 0, selected_ = panel::kNoSlot, selected_bank_ = 0, source_ = panel::kNoSlot, source_bank_ = 0;
    bool loop_source_ = false;
    MenuPage page_ = MenuPage::Presets;
    uint8_t sample_mode_ = 0, sample_bank_[2]{}, chromatic_slot_ = 0, selected_mode_ = 0, source_mode_ = 0, record_source_ = 1;
};

// TAPE's record gesture: with the toggle down, holding the CHOMPI key records;
// releasing it (or flipping the toggle up) stops. Audio owner, once per block.
class RecordGesture {
public:
    enum class Event : uint8_t { None, Start, Stop };
    Event Update(bool toggle_up, bool chompi_down) {
        Event event = Event::None;
        if(recording_ && (toggle_up || !chompi_down)) { recording_ = false; event = Event::Stop; }
        else if(!recording_ && !toggle_up && chompi_down && !chompi_) { recording_ = true; event = Event::Start; }
        chompi_ = chompi_down;
        return event;
    }
    void Cancel() { recording_ = false; }          // the recorder refused to start
    bool Recording() const { return recording_; }
private:
    bool recording_ = false, chompi_ = false;
};

// LED colours for the 25 key LEDs while the menu is open (main loop, pure).
struct Rgb { float r = 0, g = 0, b = 0; };
FORGE_NOINLINE inline void RenderMenuLeds(uint32_t packed, uint16_t occupancy, bool card_ready, uint8_t last_bank,
                           uint8_t last_slot, bool blink_on, Rgb (&leds)[25]) {
    for(auto& led : leds) led = Rgb{};
    if(!(packed & 1u)) return;
    const MenuMode mode = static_cast<MenuMode>((packed >> 1) & 7u);
    const uint8_t bank = (packed >> 4) & 7u, selected = (packed >> 7) & 0xfu, selected_bank = (packed >> 11) & 7u;
    const uint8_t source = (packed >> 14) & 0xfu, source_bank = (packed >> 18) & 7u;
    static const Rgb kBankColours[kPresetBanks] = {{1, .2f, .2f}, {1, .6f, 0}, {.9f, .9f, 0}, {.2f, 1, .2f},
                                                   {0, .9f, .9f}, {.2f, .4f, 1}, {.7f, .2f, 1}, {1, .3f, .8f}};
    const Rgb red{1, 0, 0}, green{0, 1, 0}, blue{0, .4f, 1}, white{1, 1, 1}, dim{.25f, .2f, .12f};
    leds[panel::BlackLed(panel::kBankDown)] = leds[panel::BlackLed(panel::kBankUp)] = kBankColours[bank];
    auto scaled = [](Rgb c, float k) { return Rgb{c.r * k, c.g * k, c.b * k}; };
    leds[panel::BlackLed(panel::kPage)] = Rgb{.3f, .3f, .3f};      // page key: white = presets, magenta = samples
    leds[panel::BlackLed(panel::kSave)] = scaled(blue, mode == MenuMode::Save ? 1.f : .25f);
    leds[panel::BlackLed(panel::kErase)] = scaled(red, mode == MenuMode::Erase ? 1.f : .25f);
    leds[panel::BlackLed(panel::kCopy)] = scaled(green, mode == MenuMode::CopySource || mode == MenuMode::CopyDest ? 1.f : .25f);
    for(uint8_t slot = 0; slot < kPresetSlots; ++slot) {
        Rgb& led = leds[panel::SlotLed(slot)];
        if(!card_ready) { led = scaled(red, .5f); continue; }
        const bool occupied = (occupancy >> slot) & 1u;
        if(mode == MenuMode::None) led = bank == last_bank && slot == last_slot && occupied ? white : occupied ? dim : Rgb{};
        else if(selected == slot && selected_bank == bank) led = mode == MenuMode::Erase ? red : blue;
        else if((mode == MenuMode::CopySource || mode == MenuMode::CopyDest) && source == slot && source_bank == bank) led = green;
        else led = blink_on && occupied ? dim : Rgb{};   // choosing: occupied slots blink
    }
}
// Samples page LEDs. occupancy: file slots of the shown mode/bank; recording:
// the RAM slot holds a take; live: PackSelection of the playing patch.
// TAPE's sample bank colours (a-e): purple, orange, teal, dark orange, yellow-green.
inline Rgb SampleBankColour(uint8_t bank) {
    static const Rgb kBankColours[kSampleBanks] = {{.6f, .1f, 1}, {1, .45f, 0}, {0, .8f, .7f}, {.8f, .25f, 0}, {.6f, 1, 0}};
    return kBankColours[bank < kSampleBanks ? bank : 0];
}
FORGE_NOINLINE inline void RenderSampleLeds(uint32_t packed, uint16_t occupancy, bool card_ready, bool recording, uint32_t live,
                             bool blink_on, Rgb (&leds)[25]) {
    for(auto& led : leds) led = Rgb{};
    if(!(packed & 1u)) return;
    const MenuMode mode = static_cast<MenuMode>((packed >> 1) & 7u);
    const uint8_t bank = (packed >> 4) & 7u, selected = (packed >> 7) & 0xfu, selected_bank = (packed >> 11) & 7u;
    const uint8_t source = (packed >> 14) & 0xfu, source_bank = (packed >> 18) & 7u;
    const uint8_t sample_mode = (packed >> 22) & 1u, selected_mode = (packed >> 23) & 1u, source_mode = (packed >> 24) & 1u;
    const uint8_t record_source = (packed >> 25) & 3u;
    const Rgb red{1, 0, 0}, green{0, 1, 0}, blue{0, .4f, 1}, white{1, 1, 1}, dim{.25f, .2f, .12f}, pink{1, .2f, .6f};
    auto scaled = [](Rgb c, float k) { return Rgb{c.r * k, c.g * k, c.b * k}; };
    leds[panel::BlackLed(panel::kPage)] = Rgb{.8f, 0, .8f};
    leds[panel::BlackLed(panel::kChromatic)] = scaled(SampleBankColour(bank), sample_mode == 0 ? 1.f : .15f);
    leds[panel::BlackLed(panel::kKit)] = scaled(SampleBankColour(bank), sample_mode == 1 ? 1.f : .15f);
    const uint8_t source_keys[3] = {panel::kMic, panel::kLine, panel::kResample};
    for(uint8_t s = 0; s < 3; ++s) leds[panel::BlackLed(source_keys[s])] = scaled(pink, s == record_source ? 1.f : .15f);
    leds[panel::BlackLed(panel::kSave)] = scaled(blue, mode == MenuMode::Save ? 1.f : .25f);
    leds[panel::BlackLed(panel::kErase)] = scaled(red, mode == MenuMode::Erase ? 1.f : .25f);
    leds[panel::BlackLed(panel::kCopy)] = scaled(green, mode == MenuMode::CopySource || mode == MenuMode::CopyDest ? 1.f : .25f);
    const bool live_here = (live & 1u) && ((live >> 1) & 1u) == sample_mode && ((live >> 2) & 7u) == bank;
    const uint8_t live_slot = (live >> 5) & 15u;
    for(uint8_t slot = 0; slot < kSampleSlots; ++slot) {
        Rgb& led = leds[panel::SlotLed(slot)];
        const bool ram = slot == kRamSlot;
        if(!card_ready && !ram) { led = scaled(red, .5f); continue; }
        const bool occupied = ram ? recording : ((occupancy >> slot) & 1u) != 0;
        const bool playing = live_here && (sample_mode == 1 ? occupied : slot == live_slot);
        if(mode == MenuMode::None) led = playing ? white : ram && recording ? pink : occupied ? dim : Rgb{};
        else if(ram) led = Rgb{};
        else if(selected == slot && selected_bank == bank && selected_mode == sample_mode) led = mode == MenuMode::Erase ? red : blue;
        else if((mode == MenuMode::CopySource || mode == MenuMode::CopyDest) && source == slot && source_bank == bank
                && source_mode == sample_mode) led = green;
        else led = blink_on && occupied ? dim : Rgb{};
    }
}
} // namespace forge
