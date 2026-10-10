#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include "../core/midi_framer.h"
#include "../core/runtime.h"
#include "../core/usb_packets.h"

using namespace forge;
std::vector<uint8_t> Patch() {
    std::vector<uint8_t> packet(18);
    Header(packet.data(), 1, 1240); packet[7] = 1;
    Write14(packet.data() + 8, 16383); Write14(packet.data() + 10, 8192);
    Write14(packet.data() + 12, 0); Write14(packet.data() + 14, 4096);
    packet[16] = 1; packet[17] = Checksum(packet.data(), 17); return packet;
}
void ProtocolAndAtomicity() {
    auto packet = Patch(); Request request;
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::None);
    assert(request.sequence == 1240 && request.patch.mix == 1.f && request.patch.bypass);
    std::vector<float> l(48002), r(48002); Engine engine;
    assert(engine.Init(48000.f, l.data(), r.data(), l.size()));
    Response response;
    assert(ExecuteRequest(request, engine, response));
    assert(response.error == Error::None && response.patch.mix == 1.f && response.patch.bypass);
    Request bad = request; bad.patch.level = std::numeric_limits<float>::quiet_NaN();
    bad.patch.mix = 0.f;
    assert(ExecuteRequest(bad, engine, response) && response.error == Error::Patch);
    assert(engine.GetParameters().mix == 1.f); // no partial mutation
    for(size_t i = 0; i < packet.size(); ++i) {
        auto broken = packet; broken[i] ^= 1;
        Request untouched; untouched.sequence = 123;
        assert(DecodeRequest(broken.data(), broken.size(), untouched) != Error::None);
        assert(untouched.sequence == 123);
    }
    for(size_t size = 0; size < packet.size(); ++size)
        assert(DecodeRequest(packet.data(), size, request) != Error::None);
    packet[16] = 2; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Patch);
    packet = Patch(); packet[7] = 8; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Version);
    packet = Patch(); packet[7] = 3; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Length); // v3 needs 69 bytes
    uint8_t reply[kMaxReply]; response.error = Error::None;
    response.cpu_average = 0.254f; response.cpu_max = 1.1f;
    assert(EncodeResponse(response, 99999999, 9, reply) == 18 + kStatusTail);
    assert(Checksum(reply, 18 + kStatusTail) == 0 && Read14(reply + 18) == 254);
    assert(reply[22] == 127 && reply[23] == 127 && reply[24] == 127);
}
// A v3 apply request with every field set to a distinct, valid value.
std::vector<uint8_t> PatchV3() {
    std::vector<uint8_t> packet(kV3Request);
    Header(packet.data(), 1, 77); packet[7] = 3;
    for(unsigned i = 8; i < 16; i += 2) Write14(packet.data() + i, 1000 + i * 100);
    packet[16] = 0; packet[17] = 1; packet[18] = 2;
    for(unsigned i = 19; i < 29; i += 2) Write14(packet.data() + i, 2000 + i * 100);
    size_t count; const V3Field* fields = V3Fields(count);
    for(size_t i = 0; i < count; ++i) {
        if(fields[i].index >= kV3Request - 1) continue;          // v4 sampler fields
        if(fields[i].kind == 0) Write14(packet.data() + fields[i].index, 300 + 500 * unsigned(i));
        else packet[fields[i].index] = fields[i].max;
    }
    packet[58] = 1; packet[59] = 2;
    packet[68] = Checksum(packet.data(), 68);
    return packet;
}
void ProtocolV3() {
    auto packet = PatchV3(); Request request;
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::None);
    const Parameters& p = request.patch;
    assert(p.version == 3 && p.synth && p.waveform == 2 && p.osc2_waveform == 3 && p.osc2_semitones == 48
           && p.lfo_waveform == 3 && p.lfo_wheel && p.voices == 2 && p.Valid());
    // Every v3 field must survive decode -> engine -> status encode unchanged.
    std::vector<float> l(48002), r(48002), rv(Reverb::Required(48000)); Engine engine;
    assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
    Response response; assert(ExecuteRequest(request, engine, response) && response.error == Error::None);
    uint8_t reply[kMaxReply];
    assert(EncodeResponse(response, 0, 0, reply) == kV3Request + kStatusTail && Checksum(reply, kV3Request + kStatusTail) == 0);
    for(size_t i = 7; i < 68; ++i) assert(reply[i + 1] == packet[i]);
    assert(reply[79] == kFirmwareMinor);
    // 0.15.2 build identity: patch, flags (none on a host build), commit 0 = unknown.
    assert(reply[80] == kFirmwarePatch && reply[81] == 0 && reply[82] == 0 && reply[83] == 0 && reply[84] == 0 && reply[85] == 0);
    assert(reply[86] == 4 && reply[87] == 0 && reply[88] == 0);              // power: battery unknown (filled by the main loop)
    response.battery = 1; response.power_flags = 1 | 32; response.charge_state = 5;
    assert(EncodeResponse(response, 0, 0, reply) == kV3Request + kStatusTail && reply[86] == 1 && reply[87] == 33 && reply[88] == 5);
    response.battery = 4; response.power_flags = 0; response.charge_state = 0;
    RuntimeBuildFlags() = kBuildSafeMode;
    assert(EncodeResponse(response, 0, 0, reply) == kV3Request + kStatusTail && reply[81] == kBuildSafeMode
           && Checksum(reply, kV3Request + kStatusTail) == 0);
    RuntimeBuildFlags() = 0;
    // Each byte field rejects one past its maximum; voices also rejects 0.
    for(auto bad : std::vector<std::pair<unsigned, uint8_t>>{{29, 4}, {32, 49}, {49, 4}, {58, 2}, {59, 0}, {59, 5}}) {
        auto broken = PatchV3(); broken[bad.first] = bad.second; broken[68] = Checksum(broken.data(), 68);
        Request untouched; untouched.sequence = 5;
        assert(DecodeRequest(broken.data(), broken.size(), untouched) == Error::Patch && untouched.sequence == 5);
    }
    auto shortened = PatchV3(); shortened.pop_back();
    shortened.back() = Checksum(shortened.data(), shortened.size() - 1); // valid checksum, 68 bytes
    assert(DecodeRequest(shortened.data(), shortened.size(), request) == Error::Length);
    // The framer accepts a whole v3 request and rejects anything longer than kMaxSysEx.
    MidiFramer parser; MidiFrame frame;
    parser.Feed(0xf0, frame); for(auto b : packet) assert(!parser.Feed(b, frame));
    assert(parser.Feed(0xf7, frame) && frame.size == packet.size());
    parser.Feed(0xf0, frame); for(unsigned i = 0; i <= kMaxSysEx; ++i) parser.Feed(1, frame);
    assert(!parser.Feed(0xf7, frame));
    // Release builds have no development opcodes (panel injection, probe).
    for(uint8_t op : {0x0a, 0x0b}) {
        uint8_t dev[11]; Header(dev, op, 3); dev[7] = 0; dev[8] = 15; dev[9] = 65; dev[10] = Checksum(dev, 10);
        assert(DecodeRequest(dev, op == 0x0a ? 11 : 9, request) == Error::Opcode || DecodeRequest(dev, 11, request) == Error::Opcode);
    }
    for(uint8_t page=0;page<=7;++page) {
        uint8_t dev[14]{}; Header(dev,0x0b,3); dev[7]=page; dev[8]=Checksum(dev,8);
        assert(DecodeRequest(dev,9,request)==Error::Opcode);
    }
    uint8_t cursor_request[14]{}; Header(cursor_request,0x0b,3); cursor_request[7]=6;
    cursor_request[13]=Checksum(cursor_request,13);
    assert(DecodeRequest(cursor_request,14,request)==Error::Opcode);
    // The largest reply (a v7 status) fits the firmware's USB packet buffer size.
    response.patch.version = 7;
    assert(EncodeResponse(response, 0, 0, reply) == kMaxReply);
    uint8_t envelope[kMaxReply + 2]; envelope[0] = 0xf0; envelope[kMaxReply + 1] = 0xf7;
    for(size_t i = 0; i < kMaxReply; ++i) envelope[i + 1] = reply[i];
    uint8_t usb[((kMaxReply + 2 + 2) / 3) * 4];
    assert(PackUsbSysEx(envelope, sizeof(envelope), usb, sizeof(usb)) == sizeof(usb));
    assert(PackUsbSysEx(envelope, sizeof(envelope), usb, sizeof(usb) - 1) == 0);
}
void Framing() {
    MidiFramer parser; MidiFrame frame;
    auto packet = Patch();
    assert(!parser.Feed(0xf0, frame));
    for(auto byte : packet) {
        assert(!parser.Feed(byte, frame));
        assert(parser.Feed(0xf8, frame) && frame.kind == MidiFrame::Kind::Clock && frame.data[0] == 0); // clock may interrupt SysEx
        assert(!parser.Feed(0xfe, frame));  // active sensing is ignored
    }
    assert(parser.Feed(0xf7, frame) && frame.size == packet.size());
    for(size_t i = 0; i < packet.size(); ++i) assert(frame.data[i] == packet[i]);
    parser.Feed(0xf0, frame);
    for(unsigned i = 0; i < 1000; ++i) parser.Feed(1, frame);
    assert(!parser.Feed(0xf7, frame));
    parser.Feed(0xf0, frame); parser.Feed(1, frame);
    parser.Feed(0xb0, frame); parser.Feed(20, frame); parser.Feed(0xfe, frame);
    assert(parser.Feed(127, frame) && frame.kind == MidiFrame::Kind::CC);
    parser.Feed(21, frame); // running status
    assert(parser.Feed(64, frame) && frame.data[1] == 21 && frame.data[2] == 64);
    parser.Feed(0x90, frame); parser.Feed(60, frame);
    assert(parser.Feed(127, frame) && frame.kind == MidiFrame::Kind::NoteOn);
    parser.Feed(61, frame); // note running status and zero-velocity release
    parser.Feed(0xf8, frame);
    assert(parser.Feed(0, frame) && frame.kind == MidiFrame::Kind::NoteOn && frame.data[2] == 0);
    parser.Feed(0x80, frame); parser.Feed(60, frame);
    assert(parser.Feed(64, frame) && frame.kind == MidiFrame::Kind::NoteOff);
    uint32_t random = 123;
    for(unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525u + 1013904223u;
        if(parser.Feed(random >> 24, frame)) assert(frame.size <= 48);
    }
}
void UsbPacketization() {
    for(size_t length = 2; length <= 48; ++length) {
        std::vector<uint8_t> message(length, 3), output(64);
        message.front() = 0xf0; message.back() = 0xf7;
        const size_t size = PackUsbSysEx(message.data(), message.size(), output.data(), output.size());
        assert(size == ((length + 2) / 3) * 4);
        std::vector<uint8_t> recovered;
        for(size_t i = 0; i < size; i += 4) {
            const size_t count = output[i] == 4 ? 3 : output[i] - 4;
            if(i + 4 < size) assert(output[i] == 4);
            else assert(output[i] >= 5 && output[i] <= 7);
            for(size_t j = 0; j < count; ++j) recovered.push_back(output[i + 1 + j]);
        }
        assert(recovered == message);
        assert(PackUsbSysEx(message.data(), message.size(), output.data(), size - 1) == 0);
    }
}
// v5 = v4 + four knob assignments (request 83-86): 88-byte request, 100-byte status.
void ProtocolV5() {
    Parameters p; p.version = 5; p.synth = true; p.voices = 7; p.resonance = 0.3f;
    p.knobs[0] = static_cast<uint8_t>(Parameter::Attack) + 1; p.knobs[2] = static_cast<uint8_t>(Parameter::ReverbSize) + 1;
    assert(p.Valid());
    std::vector<uint8_t> request(kV5Request); Header(request.data(), 1, 77);
    assert(EncodePatchData(p, request.data() + 7) == kV5Request - 8);
    assert(request[83] == p.knobs[0] && request[84] == 0 && request[85] == p.knobs[2] && request[86] == 0);
    request[kV5Request - 1] = Checksum(request.data(), kV5Request - 1);
    Request decoded; assert(DecodeRequest(request.data(), request.size(), decoded) == Error::None);
    assert(decoded.patch.version == 5 && decoded.patch.knobs[0] == p.knobs[0] && decoded.patch.knobs[2] == p.knobs[2]
           && decoded.patch.knobs[1] == 0 && decoded.patch.knobs[3] == 0);
    assert(decoded.patch.KnobParameter(0) == Parameter::Attack && decoded.patch.KnobParameter(1) == Parameter::Time
           && decoded.patch.KnobParameter(2) == Parameter::ReverbSize);
    std::vector<float> l(48002), r(48002); Engine engine; assert(engine.Init(48000.f, l.data(), r.data(), l.size()));
    Response response; assert(ExecuteRequest(decoded, engine, response) && response.error == Error::None);
    uint8_t reply[kMaxReply]; assert(EncodeResponse(response, 0, 0, reply) == kV5Request + kStatusTail && Checksum(reply, kV5Request + kStatusTail) == 0);
    for(size_t i = 7; i < kV5Request - 1; ++i) assert(reply[i + 1] == request[i]);
    // Not assignable (bypass, the knobs themselves, unknown ids) or a v4 size: rejected, nothing applied.
    for(uint8_t bad : {uint8_t(uint8_t(Parameter::Bypass) + 1), uint8_t(uint8_t(Parameter::Knob1) + 1), uint8_t(uint8_t(Parameter::Knob4) + 1),
                       uint8_t(uint8_t(Parameter::Count) + 1), uint8_t(127)}) {
        auto broken = request; broken[84] = bad; broken[kV5Request - 1] = Checksum(broken.data(), kV5Request - 1);
        Request untouched; untouched.sequence = 9;
        assert(DecodeRequest(broken.data(), broken.size(), untouched) == Error::Patch && untouched.sequence == 9);
    }
    // The whole v5 request passes the MIDI framer (kMaxSysEx) intact.
    MidiFramer parser; MidiFrame frame; assert(!parser.Feed(0xf0, frame));
    for(auto byte : request) assert(!parser.Feed(byte, frame));
    assert(parser.Feed(0xf7, frame) && frame.size == kV5Request && DecodeRequest(frame.data, frame.size, decoded) == Error::None);
    auto short_v5 = request; short_v5.resize(kV4Request); short_v5[kV4Request - 1] = Checksum(short_v5.data(), kV4Request - 1);
    assert(DecodeRequest(short_v5.data(), short_v5.size(), decoded) == Error::Length);
    // Older versions cannot carry assignments.
    Parameters v4 = p; v4.version = 4; assert(!v4.Valid()); v4.knobs[0] = v4.knobs[2] = 0; assert(v4.Valid());
    // The last control is assignable; every assignable control round-trips through Apply/Value.
    Parameters all; all.version = 5; all.synth = true;
    for(unsigned id = 0; id < static_cast<unsigned>(Parameter::Count); ++id) {
        const Parameter q = static_cast<Parameter>(id);
        all.knobs[1] = static_cast<uint8_t>(id + 1);
        assert(all.Valid() == Parameters::Assignable(q));
        if(!Parameters::Assignable(q)) continue;
        Parameters copy = all;
        assert(copy.Apply({Parameter::Knob2, q == Parameter::SampleStart ? 0.25f : 0.75f}));
        assert(std::fabs(copy.Value(q) - (q == Parameter::SampleStart ? 0.25f : 0.75f)) < 1e-6f);
        assert(std::fabs(copy.Value(Parameter::Knob2) - copy.Value(q)) < 1e-6f);
    }
}
// v6 (0.13) = v5 + the harmony word (request 87-89): 91-byte request, 103-byte status.
void ProtocolV6() {
    Parameters p; p.version = 6; p.synth = true; p.voices = 4;
    p.harmony = 9u | 1u << 4 | 1u << 8 | 2u << 11 | 1u << 15 | 1u << 16;          // A natural minor, 7th, inversion 2, Real, on
    assert(p.Valid());
    std::vector<uint8_t> request(kV6Request); Header(request.data(), 1, 78);
    assert(EncodePatchData(p, request.data() + 7) == kV6Request - 8);
    request[kV6Request - 1] = Checksum(request.data(), kV6Request - 1);
    Request decoded; assert(DecodeRequest(request.data(), request.size(), decoded) == Error::None);
    assert(decoded.patch.version == 6 && decoded.patch.harmony == p.harmony);
    std::vector<float> l(48002), r(48002); Engine engine; assert(engine.Init(48000.f, l.data(), r.data(), l.size()));
    harmony::Player player; engine.SetHarmony(&player);
    Response response; assert(ExecuteRequest(decoded, engine, response) && response.error == Error::None);
    assert(player.state.enabled && player.state.tonic == 9 && player.state.mode == harmony::Mode::NaturalMinor
           && player.state.extension == harmony::Extension::Seventh && player.state.inversion == 2 && player.state.layout == harmony::Layout::Real);
    uint8_t reply[kMaxReply]; assert(EncodeResponse(response, 0, 0, reply) == kV6Request + kStatusTail && Checksum(reply, kV6Request + kStatusTail) == 0);
    for(size_t i = 7; i < kV6Request - 1; ++i) assert(reply[i + 1] == request[i]);   // the apply echo
    // Invalid harmony words are rejected; v5 sizes are not v6.
    for(uint32_t bad : {12u, 9u << 4, 6u << 8}) {
        auto broken = request; broken[87] = bad & 127; broken[88] = (bad >> 7) & 127; broken[89] = (bad >> 14) & 127;
        broken[kV6Request - 1] = Checksum(broken.data(), kV6Request - 1);
        assert(DecodeRequest(broken.data(), broken.size(), decoded) == Error::Patch);
    }
    auto short_v6 = request; short_v6.resize(kV5Request); short_v6[kV5Request - 1] = Checksum(short_v6.data(), kV5Request - 1);
    assert(DecodeRequest(short_v6.data(), short_v6.size(), decoded) == Error::Length);
    // A v5 apply keeps the harmony state; a status snapshot carries it as v6 (a v3+ patch).
    Parameters v5 = p; v5.version = 5; v5.harmony = 0; assert(engine.ApplyPatch(v5));
    assert(player.state.enabled && engine.GetParameters().version == 5);
    const Parameters snap = engine.Snapshot(); assert(snap.version == 6 && snap.harmony == harmony::Pack(player.state) && snap.Valid());
    // v1/v2 patches are never moved up a version.
    Parameters v2; v2.version = 2; v2.synth = true; assert(engine.ApplyPatch(v2) && engine.Snapshot().version == 2);
    player.state.enabled = false; Parameters v4; v4.version = 4; v4.synth = true; assert(engine.ApplyPatch(v4) && engine.Snapshot().version == 4);
}
// v7 (0.14) = v6 + the parts words (request 90-95): 97-byte request, 109-byte status.
void ProtocolV7() {
    parts::Settings s; s.pattern = parts::Pattern::UpDown; s.rate = parts::Rate::SixteenthTriplet; s.octaves = 3; s.gate = 7;
    s.latch = false; s.bass = parts::Bass::Alternate; s.bass_rate = parts::BassRate::Half; s.bass_octave = 0; s.bpm = 97; s.seed = 1234;
    Parameters p; p.version = 7; p.synth = true; p.voices = 4;
    p.parts_arp = parts::PackArp(s); p.parts_clock = parts::PackClock(s);
    assert(p.Valid());
    std::vector<uint8_t> request(kMaxRequest); Header(request.data(), 1, 79);
    assert(EncodePatchData(p, request.data() + 7) == kMaxRequest - 8);
    request[kMaxRequest - 1] = Checksum(request.data(), kMaxRequest - 1);
    Request decoded; assert(DecodeRequest(request.data(), request.size(), decoded) == Error::None);
    assert(decoded.patch.version == 7 && decoded.patch.parts_arp == p.parts_arp && decoded.patch.parts_clock == p.parts_clock);
    std::vector<float> l(48002), r(48002); Engine engine; assert(engine.Init(48000.f, l.data(), r.data(), l.size()));
    parts::Parts parts; parts.Init(48000.f); engine.SetParts(&parts);
    Response response; assert(ExecuteRequest(decoded, engine, response) && response.error == Error::None);
    assert(parts.settings == s && parts.GetClock().Tempo() == 97);
    uint8_t reply[kMaxReply]; assert(EncodeResponse(response, 0, 0, reply) == kMaxReply && Checksum(reply, kMaxReply) == 0);
    for(size_t i = 7; i < kMaxRequest - 1; ++i) assert(reply[i + 1] == request[i]);   // the apply echo
    // Invalid parts words are rejected: pattern 6, gate 21, bass octave 3, tempo 39 and 301.
    parts::Settings slow = s; slow.bpm = 39; parts::Settings fast = s; fast.bpm = 301;
    const uint32_t bad[][2] = {{p.parts_arp | 7u, p.parts_clock}, {p.parts_arp | 20u << 8, p.parts_clock}, {p.parts_arp | 3u << 19, p.parts_clock},
                               {p.parts_arp, parts::PackClock(slow)}, {p.parts_arp, parts::PackClock(fast)}};
    for(const auto& w : bad) {
        auto broken = request;
        for(unsigned k = 0; k < 3; ++k) { broken[90 + k] = (w[0] >> (7 * k)) & 127; broken[93 + k] = (w[1] >> (7 * k)) & 127; }
        broken[kMaxRequest - 1] = Checksum(broken.data(), kMaxRequest - 1);
        assert(DecodeRequest(broken.data(), broken.size(), decoded) == Error::Patch);
    }
    auto short_v7 = request; short_v7.resize(kV6Request); short_v7[kV6Request - 1] = Checksum(short_v7.data(), kV6Request - 1);
    assert(DecodeRequest(short_v7.data(), short_v7.size(), decoded) == Error::Length);
    // A v3-v6 apply keeps the parts; while they play, status and presets carry them as v7.
    Parameters v6 = p; v6.version = 6; v6.parts_arp = v6.parts_clock = 0; assert(engine.ApplyPatch(v6));
    assert(parts.settings == s && engine.GetParameters().version == 6);
    const Parameters snap = engine.Snapshot();
    assert(snap.version == 7 && snap.parts_arp == p.parts_arp && snap.parts_clock == p.parts_clock && snap.harmony == 0 && snap.Valid());
    parts.settings.pattern = parts::Pattern::Off; parts.settings.bass = parts::Bass::Off; assert(engine.Snapshot().version == 6);
    Parameters v2; v2.version = 2; v2.synth = true; parts.settings.bass = parts::Bass::Root;
    assert(engine.ApplyPatch(v2) && engine.Snapshot().version == 2);
    // MIDI real-time bytes reach the clock (any channel); active sensing does not.
    MidiFrame frame; MidiFramer framer; Request clock;
    assert(framer.Feed(0xfa, frame) && TranslateChannel(frame, 1, clock, 5) == Ingress::Control && clock.kind == RequestKind::Clock && clock.note == 1);
    assert(!framer.Feed(0xfe, frame));
    assert(engine.ApplyPatch(p) && !ExecuteRequest(clock, engine, response) && parts.GetClock().External());
}
// Status with the optional flags byte: bit 0 asks for a fresh CPU peak after the reply.
void StatusResetsCpu() {
    auto status = [](std::vector<uint8_t> body) {
        std::vector<uint8_t> m(7); Header(m.data(), 2, 5); m.insert(m.end(), body.begin(), body.end());
        m.push_back(0); m.back() = Checksum(m.data(), m.size() - 1); return m;
    };
    Request r; auto m = status({});
    assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Status && !r.reset_cpu);
    m = status({1}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Status && r.reset_cpu);
    m = status({0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && !r.reset_cpu);
    m = status({2}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
    m = status({1, 0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Length);
}
int main() { ProtocolV3(); ProtocolV5(); ProtocolV6(); ProtocolV7(); StatusResetsCpu();
    ProtocolAndAtomicity(); Framing(); UsbPacketization();
    std::cout << "PASS: protocol rejection/atomicity, v3 round trip/bounds, v5 knob assignments, status CPU reset, MIDI real-time/resync/fuzz, USB packet endings\n";
}
