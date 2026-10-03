#pragma once
#include <cstdint>
#include "protocol.h"

namespace forge {
// CHOMPI panel IDs (upstream Hardware::SwId order) used by the preset menu.
namespace panel {
constexpr uint8_t kChompiKey = 5, kToggle = 6;
constexpr uint8_t kBankDown = 7, kBankUp = 12;            // black KEY_16 / KEY_17
constexpr uint8_t kErase = 29, kCopy = 30, kSave = 31;    // black KEY_23 / 24 / 25, as in TAPE
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

// TAPE-style preset menu (audio owner; no I/O). Toggle up + press the CHOMPI
// key opens it. While open, keys do not play notes:
//   white key            recall that slot of the current bank (hold CHOMPI)
//   KEY_16 / KEY_17       bank down / up (wraps); knob 0 (hardware SW4) also turns banks
//   KEY_25 save, KEY_23 erase: choose the mode, pick a white key, press CHOMPI
//   KEY_24 copy: pick source, then destination (any bank), press CHOMPI
// Pressing an active function key again cancels it. The menu closes when the
// CHOMPI key is released with nothing pending, or when the toggle goes down.
// Actions are queued for the main loop, which performs all SD access.
struct MenuAction {
    enum class Kind : uint8_t { Recall, Save, Erase, Copy } kind = Kind::Recall;
    uint8_t bank = 0, slot = 0, to_bank = 0, to_slot = 0;
};
enum class MenuMode : uint8_t { None, Save, Erase, CopySource, CopyDest };

class PresetMenu {
public:
    // Once per audio block with debounced inputs.
    void Update(bool toggle_up, bool chompi_down) {
        const bool rising = chompi_down && !chompi_;
        chompi_ = chompi_down;
        if(!toggle_up) { Close(); return; }
        if(!active_) {
            if(rising) { active_ = true; mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; }
            return;
        }
        if(rising) Confirm();
        else if(!chompi_down && mode_ == MenuMode::None) Close();
    }
    // Key edge. Returns true if the menu consumed it (the caller must not play
    // a note). Releases are never consumed, so notes held before the menu opened
    // still end.
    bool Key(uint8_t button, bool pressed) {
        if(!active_ || !pressed) return false;
        const uint8_t slot = panel::KeyToSlot(button);
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
    MenuMode Mode() const { return mode_; }
    uint8_t Bank() const { return bank_; }
    void SetBank(uint8_t bank) { if(bank < kPresetBanks) bank_ = bank; }
    // Compact state for the main loop's LED drawing (one atomic word).
    uint32_t Packed() const {
        return (active_ ? 1u : 0u) | (static_cast<uint32_t>(mode_) << 1) | (static_cast<uint32_t>(bank_) << 4)
            | (static_cast<uint32_t>(selected_ & 0xf) << 7) | (static_cast<uint32_t>(selected_bank_) << 11)
            | (static_cast<uint32_t>(source_ & 0xf) << 14) | (static_cast<uint32_t>(source_bank_) << 18);
    }
private:
    static constexpr unsigned kActions = 4;
    bool BlackKey(uint8_t button) { return panel::BlackLed(button) != 0xff; }  // other black keys: swallowed
    void Close() { active_ = false; mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot; }
    void Toggle(MenuMode mode) {
        const bool same = mode_ == mode || (mode == MenuMode::CopySource && mode_ == MenuMode::CopyDest);
        if(mode_ != MenuMode::None && !same) return;    // finish or cancel the current mode first
        mode_ = same ? MenuMode::None : mode;
        selected_ = source_ = panel::kNoSlot;
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
    void Confirm() {
        if(selected_ == panel::kNoSlot) return;
        if(mode_ == MenuMode::Save) Push({MenuAction::Kind::Save, selected_bank_, selected_, 0, 0});
        else if(mode_ == MenuMode::Erase) Push({MenuAction::Kind::Erase, selected_bank_, selected_, 0, 0});
        else if(mode_ == MenuMode::CopyDest) {
            if(source_bank_ == selected_bank_ && source_ == selected_) return;
            Push({MenuAction::Kind::Copy, source_bank_, source_, selected_bank_, selected_});
        } else return;
        mode_ = MenuMode::None; selected_ = source_ = panel::kNoSlot;
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
};

// LED colours for the 25 key LEDs while the menu is open (main loop, pure).
struct Rgb { float r = 0, g = 0, b = 0; };
inline void RenderMenuLeds(uint32_t packed, uint16_t occupancy, bool card_ready, uint8_t last_bank,
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
} // namespace forge
