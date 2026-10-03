// Device presets: SD record format, store behaviour under card faults, the
// TAPE-style panel menu, its LED model, and the storage protocol messages.
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../core/preset_menu.h"
#include "../core/runtime.h"
using namespace forge;

// In-memory card with fault injection.
struct MemoryStorage : Storage {
    std::map<std::string, std::vector<uint8_t>> files;
    bool ready = true, fail_writes = false, drop_writes = false;
    bool Ready() override { return ready; }
    bool Read(const char* path, uint8_t* buffer, size_t capacity, size_t& size) override {
        auto it = files.find(path);
        if(!ready || it == files.end() || it->second.size() > capacity) return false;
        size = it->second.size(); std::memcpy(buffer, it->second.data(), size); return true;
    }
    bool Write(const char* path, const uint8_t* data, size_t size) override {
        if(!ready || fail_writes) return false;
        if(!drop_writes) files[path].assign(data, data + size);
        return true;
    }
    bool Remove(const char* path) override { if(!ready) return false; files.erase(path); return true; }
};

Parameters Sample(uint8_t version, float seed) {
    Parameters p; p.version = version; p.synth = version >= 2; p.mix = seed; p.time = 1 - seed; p.level = .25f;
    if(version >= 2) { p.waveform = 2; p.cutoff = seed * .5f; }
    if(version == 3) { p.osc2_level = .3f; p.osc2_semitones = 31; p.voices = 2; p.lfo_wheel = true; p.reverb_mix = seed; }
    return p;
}
bool SameOnWire(const Parameters& a, const Parameters& b) {
    uint8_t x[kMaxRequest], y[kMaxRequest];
    const size_t n = EncodePatchData(a, x);
    return n == EncodePatchData(b, y) && std::memcmp(x, y, n) == 0;
}

void RecordsAndPaths() {
    char path[20];
    PresetPath(0, 0, path); assert(std::string(path) == "FORGE/B1S01.FPR");
    PresetPath(7, 14, path); assert(std::string(path) == "FORGE/B8S15.FPR");
    for(uint8_t b = 0; b < kPresetBanks; ++b) for(uint8_t s = 0; s < kPresetSlots; ++s) {
        PresetPath(b, s, path); const std::string name(path);
        assert(name.find(".bin") == std::string::npos && name.find(".BIN") == std::string::npos); // bootloader ignores
        assert(name.size() == 15 && name.substr(name.find('/') + 1).size() == 9);               // 8.3
    }
    for(uint8_t version = 1; version <= 3; ++version) {
        uint8_t record[kMaxPresetRecord];
        const Parameters p = Sample(version, .4f);
        const size_t size = EncodePresetRecord(p, record);
        assert(size == 6 + PatchDataSize(version));
        Parameters back; assert(DecodePresetRecord(record, size, back) == Error::None && SameOnWire(p, back));
        for(size_t i = 0; i < size; ++i) {                    // every single-byte corruption is rejected
            uint8_t bad[kMaxPresetRecord]; std::memcpy(bad, record, size); bad[i] ^= 0x10;
            Parameters untouched = Sample(1, .9f);
            assert(DecodePresetRecord(bad, size, untouched) != Error::None);
            assert(SameOnWire(untouched, Sample(1, .9f)));
        }
        assert(DecodePresetRecord(record, size - 1, back) != Error::None);   // torn write
    }
}
void StoreBehaviour() {
    MemoryStorage card; PresetStore store(card);
    store.Rescan(); for(uint8_t b = 0; b < kPresetBanks; ++b) assert(store.Occupancy(b) == 0);
    const Parameters a = Sample(3, .2f), b = Sample(2, .7f);
    assert(store.Save(0, 0, a) == Error::None && store.Save(7, 14, b) == Error::None);
    assert(store.Occupied(0, 0) && store.Occupied(7, 14) && store.Occupancy(7) == (1u << 14));
    Parameters out;
    assert(store.Load(0, 0, out) == Error::None && SameOnWire(out, a));
    assert(store.Load(3, 3, out) == Error::Empty);
    assert(store.Copy(0, 0, 4, 5) == Error::None && store.Load(4, 5, out) == Error::None && SameOnWire(out, a));
    assert(store.Copy(3, 3, 4, 6) == Error::Empty && !store.Occupied(4, 6));
    assert(store.Erase(0, 0) == Error::None && !store.Occupied(0, 0) && store.Load(0, 0, out) == Error::Empty);
    assert(store.Save(8, 0, a) == Error::Patch && store.Save(0, 15, a) == Error::Patch && store.Load(8, 0, out) == Error::Patch);
    Parameters invalid = a; invalid.voices = 0; assert(store.Save(1, 1, invalid) == Error::Patch);
    // Rescan rebuilds occupancy from the card (e.g. after it is swapped).
    PresetStore fresh(card); fresh.Rescan();
    assert(fresh.Occupied(7, 14) && fresh.Occupied(4, 5) && !fresh.Occupied(0, 0));
    // A corrupted file reads as empty, never as a broken patch.
    card.files["FORGE/B8S15.FPR"][5] ^= 1; fresh.Rescan(); assert(!fresh.Occupied(7, 14));
    // Card faults.
    card.fail_writes = true; assert(store.Save(2, 2, a) == Error::Storage && !store.Occupied(2, 2));
    card.fail_writes = false; card.drop_writes = true;
    assert(store.Save(2, 2, a) == Error::Storage && !store.Occupied(2, 2));     // read-back catches silent loss
    card.drop_writes = false; card.ready = false;
    assert(store.Save(2, 2, a) == Error::Storage && store.Load(4, 5, out) == Error::Storage && store.Erase(4, 5) == Error::Storage);
}
struct Panel {   // drives the menu like the audio callback does
    PresetMenu menu; bool toggle = true;
    void Chompi(bool down) { menu.Update(toggle, down); }
    bool Press(uint8_t button) { bool c = menu.Key(button, true); menu.Key(button, false); return c; }
    std::vector<MenuAction> Actions() { std::vector<MenuAction> v; MenuAction a; while(menu.PopAction(a)) v.push_back(a); return v; }
};
const uint8_t kWhite[15] = {15, 8, 9, 10, 11, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28};   // KEY_1..KEY_15
void MenuGestures() {
    for(uint8_t s = 0; s < 15; ++s) assert(panel::KeyToSlot(kWhite[s]) == s);
    for(uint8_t id : {5, 6, 7, 12, 13, 21, 29, 30, 31, 32, 33}) assert(panel::KeyToSlot(id) == panel::kNoSlot);
    Panel p;
    // Closed: nothing is consumed; the CHOMPI key alone (toggle down) does not open it.
    p.toggle = false; p.Chompi(true); p.Chompi(false); assert(!p.menu.Active() && !p.Press(kWhite[0]));
    // Toggle up + CHOMPI opens; hold CHOMPI + white key recalls; release closes.
    p.toggle = true; p.Chompi(true); assert(p.menu.Active());
    assert(p.Press(kWhite[3]));
    p.Press(panel::kBankUp); p.Press(panel::kBankUp); p.Press(kWhite[14]);
    p.Press(panel::kBankDown); p.Press(panel::kBankDown); p.Press(panel::kBankDown); assert(p.menu.Bank() == 7);  // wraps
    auto a = p.Actions(); assert(a.size() == 2);
    assert(a[0].kind == MenuAction::Kind::Recall && a[0].bank == 0 && a[0].slot == 3);
    assert(a[1].kind == MenuAction::Kind::Recall && a[1].bank == 2 && a[1].slot == 14);
    assert(!p.menu.Key(kWhite[3], false));             // releases pass through (note-offs)
    p.Chompi(false); assert(!p.menu.Active());
    // Encoder 1 turns banks only while open (clamped), other encoders never.
    assert(!p.menu.Encoder(0, 1));
    p.Chompi(true); assert(p.menu.Bank() == 7);           // the bank persists between openings, as in TAPE
    assert(p.menu.Encoder(0, -2) && p.menu.Bank() == 5 && !p.menu.Encoder(1, 1));
    assert(p.menu.Encoder(0, -9) && p.menu.Bank() == 0 && p.menu.Encoder(0, 99) && p.menu.Bank() == 7);
    p.menu.Encoder(0, -7); p.Chompi(false);
    // Save: mode key, release CHOMPI (menu stays open), pick a slot, confirm with CHOMPI.
    p.Chompi(true); p.Press(panel::kSave); p.Chompi(false); assert(p.menu.Active() && p.menu.Mode() == MenuMode::Save);
    p.Chompi(true); assert(p.Actions().empty());       // confirm without a slot does nothing
    p.Chompi(false); p.Press(kWhite[6]); p.menu.Encoder(0, 2);   // change bank after choosing: target stays bank 0
    p.Chompi(true); a = p.Actions();
    assert(a.size() == 1 && a[0].kind == MenuAction::Kind::Save && a[0].bank == 0 && a[0].slot == 6);
    assert(p.menu.Mode() == MenuMode::None); p.Chompi(false); assert(!p.menu.Active());
    // Erase, and cancelling by pressing the mode key again.
    p.Chompi(true); p.Press(panel::kErase); p.Press(panel::kErase); assert(p.menu.Mode() == MenuMode::None);
    p.Press(panel::kErase); p.Press(panel::kSave); assert(p.menu.Mode() == MenuMode::Erase);  // other mode ignored
    p.Press(kWhite[2]); p.Chompi(false); p.Chompi(true); a = p.Actions();
    assert(a.size() == 1 && a[0].kind == MenuAction::Kind::Erase && a[0].slot == 2); p.Chompi(false);
    // Copy across banks; copying onto itself is refused.
    p.Chompi(true); p.Press(panel::kCopy); p.Press(kWhite[1]); assert(p.menu.Mode() == MenuMode::CopyDest);
    p.Press(kWhite[1]); p.Chompi(false); p.Chompi(true); assert(p.Actions().empty());
    p.Press(panel::kBankUp); p.Press(kWhite[9]); p.Chompi(false); p.Chompi(true); a = p.Actions();
    assert(a.size() == 1 && a[0].kind == MenuAction::Kind::Copy && a[0].bank == 2 && a[0].slot == 1
           && a[0].to_bank == 3 && a[0].to_slot == 9);
    p.Chompi(false);
    // Toggle down cancels everything and closes.
    p.Chompi(true); p.Press(panel::kSave); p.Press(kWhite[0]); p.toggle = false; p.Chompi(true);
    assert(!p.menu.Active() && p.menu.Mode() == MenuMode::None && !p.Press(kWhite[0]));
    p.toggle = true; p.Chompi(false); p.Chompi(true); p.Chompi(true); assert(p.Actions().empty());
    // Function keys and other black keys never play notes while open; encoder switches pass through.
    assert(p.Press(13) && p.Press(21) && !p.Press(0) && !p.Press(33));
    // A full action queue drops extra recalls instead of overwriting.
    for(int i = 0; i < 10; ++i) p.Press(kWhite[0]);
    assert(p.Actions().size() == 3);
}
void LedModel() {
    PresetMenu menu; Rgb leds[25];
    RenderMenuLeds(menu.Packed(), 0x7fff, true, 0, 0, true, leds);
    for(auto& l : leds) assert(l.r == 0 && l.g == 0 && l.b == 0);         // closed: all off
    menu.Update(true, true);
    RenderMenuLeds(menu.Packed(), 0b101, true, 0, 2, true, leds);
    assert(leds[panel::SlotLed(2)].r == 1 && leds[panel::SlotLed(2)].g == 1);   // last recalled: white
    assert(leds[panel::SlotLed(0)].r > 0 && leds[panel::SlotLed(0)].r < 1);     // occupied: dim
    assert(leds[panel::SlotLed(1)].r == 0 && leds[panel::SlotLed(1)].b == 0);   // empty: off
    assert(leds[panel::BlackLed(panel::kBankUp)].r > 0);                        // bank colour shown
    RenderMenuLeds(menu.Packed(), 0b101, false, 0, 2, true, leds);
    assert(leds[panel::SlotLed(5)].r > 0 && leds[panel::SlotLed(5)].g == 0);    // no card: red
    menu.Key(panel::kErase, true); menu.Key(4 /*NC*/, true); menu.Key(kWhite[0], true);
    RenderMenuLeds(menu.Packed(), 0b101, true, 0, 2, false, leds);
    assert(leds[panel::SlotLed(0)].r == 1 && leds[panel::SlotLed(0)].g == 0);   // erase selection: red
    assert(leds[panel::SlotLed(2)].r == 0);                                     // blink off phase
    assert(leds[panel::BlackLed(panel::kErase)].r == 1);                        // active mode bright
}
void Protocol() {
    // Opcodes 4-6 (bank, slot) and 7 (list); out-of-range addresses rejected.
    auto make = [](uint8_t op, std::vector<uint8_t> data) {
        std::vector<uint8_t> m(7 + data.size() + 1); Header(m.data(), op, 99);
        for(size_t i = 0; i < data.size(); ++i) m[7 + i] = data[i];
        m.back() = Checksum(m.data(), m.size() - 1); return m;
    };
    Request r;
    auto m = make(4, {7, 14}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Store && r.bank == 7 && r.slot == 14);
    m = make(5, {1, 2}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Recall && !r.silent);
    m = make(6, {0, 0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Erase);
    m = make(7, {}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::List);
    m = make(4, {8, 0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
    m = make(5, {0, 15}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
    m = make(4, {0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Length);
    m = make(8, {}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Opcode);
    // Replies.
    uint8_t out[kMaxReply];
    Response ack; ack.kind = ResponseKind::Stored; ack.sequence = 99; ack.bank = 3; ack.slot = 4;
    assert(EncodeResponse(ack, 0, 0, out) == 12 && out[4] == 0x42 && out[8] == 1 && out[9] == 3 && out[10] == 4 && Checksum(out, 12) == 0);
    Response list; list.kind = ResponseKind::Occupancy; list.occupancy[0] = 0x7fff; list.occupancy[7] = 0x4001;
    assert(EncodeResponse(list, 0, 0, out) == 33 && out[4] == 0x43 && Checksum(out, 33) == 0);
    assert(out[8] == 127 && out[9] == 127 && out[10] == 1 && out[29] == 1 && out[30] == 0 && out[31] == 1);
    Response empty; empty.kind = ResponseKind::Stored; empty.error = Error::Empty;
    assert(EncodeResponse(empty, 0, 0, out) == 9 && out[4] == 0x41 && out[7] == 7);
}
void RuntimeFlows() {
    std::vector<float> l(48002), rr(48002), rv(Reverb::Required(48000)); Engine engine;
    assert(engine.Init(48000, l.data(), rr.data(), l.size(), rv.data(), rv.size()));
    MemoryStorage card; PresetStore store(card); store.Rescan();
    Parameters patch = Sample(3, .6f); assert(engine.ApplyPatch(patch));
    // Host store: the audio owner answers with a snapshot, the main loop writes it.
    Request store_req; store_req.kind = RequestKind::Store; store_req.bank = 2; store_req.slot = 9; store_req.sequence = 5;
    Response snapshot; assert(ExecuteRequest(store_req, engine, snapshot) && snapshot.kind == ResponseKind::Snapshot);
    Response stored = StoreReply(store, snapshot.sequence, 0, snapshot.bank, snapshot.slot, snapshot.patch);
    assert(stored.error == Error::None && store.Occupied(2, 9));
    // Recall a different sound back: queued as a patch and acknowledged with status.
    assert(engine.ApplyPatch(Sample(1, .1f)));
    Request recall; recall.kind = RequestKind::Recall; recall.bank = 2; recall.slot = 9; recall.sequence = 6;
    Request apply; assert(RecallRequest(store, recall, apply) == Error::None);
    Response status; assert(ExecuteRequest(apply, engine, status) && status.sequence == 6);
    assert(SameOnWire(engine.GetParameters(), patch));
    // Silent recall (panel / program change): applied, no reply.
    assert(engine.ApplyPatch(Sample(1, .1f)));
    recall.silent = true; assert(RecallRequest(store, recall, apply) == Error::None && apply.silent);
    Response none; assert(!ExecuteRequest(apply, engine, none) && SameOnWire(engine.GetParameters(), patch));
    recall.slot = 10; assert(RecallRequest(store, recall, apply) == Error::Empty);
    Request erase; erase.kind = RequestKind::Erase; erase.bank = 2; erase.slot = 9;
    assert(EraseReply(store, erase).error == Error::None && !store.Occupied(2, 9));
    card.ready = false; Request list; list.kind = RequestKind::List;
    assert(ListReply(store, list).error == Error::Storage);
}
int main() {
    RecordsAndPaths(); StoreBehaviour(); MenuGestures(); LedModel(); Protocol(); RuntimeFlows();
    std::cout << "PASS: preset records/paths, store faults, TAPE-style menu, LEDs, storage protocol, runtime flows\n";
}
