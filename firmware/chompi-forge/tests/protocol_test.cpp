#include <cassert>
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
    packet = Patch(); packet[7] = 5; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Version);
    packet = Patch(); packet[7] = 3; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Length); // v3 needs 69 bytes
    uint8_t reply[kMaxReply]; response.error = Error::None;
    response.cpu_average = 0.254f; response.cpu_max = 1.1f;
    assert(EncodeResponse(response, 99999999, 9, reply) == 30);
    assert(Checksum(reply, 30) == 0 && Read14(reply + 18) == 254);
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
    assert(EncodeResponse(response, 0, 0, reply) == kV3Request + 12 && Checksum(reply, kV3Request + 12) == 0);
    for(size_t i = 7; i < 68; ++i) assert(reply[i + 1] == packet[i]);
    assert(reply[79] == kFirmwareMinor);
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
    // The largest reply (a v4 status) fits the firmware's USB packet buffer size.
    response.patch.version = 4;
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
        assert(!parser.Feed(0xf8, frame)); // clock may interrupt SysEx
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
int main() { ProtocolV3();
    ProtocolAndAtomicity(); Framing(); UsbPacketization();
    std::cout << "PASS: protocol rejection/atomicity, v3 round trip/bounds, MIDI real-time/resync/fuzz, USB packet endings\n";
}
