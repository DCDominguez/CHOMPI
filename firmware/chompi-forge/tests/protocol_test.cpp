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
    packet = Patch(); packet[7] = 3; packet[17] = Checksum(packet.data(), 17);
    assert(DecodeRequest(packet.data(), packet.size(), request) == Error::Version);
    uint8_t reply[30]; response.error = Error::None;
    response.cpu_average = 0.254f; response.cpu_max = 1.1f;
    assert(EncodeResponse(response, 99999999, 9, reply) == 30);
    assert(Checksum(reply, 30) == 0 && Read14(reply + 18) == 254);
    assert(reply[22] == 127 && reply[23] == 127 && reply[24] == 127);
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
int main() {
    ProtocolAndAtomicity(); Framing(); UsbPacketization();
    std::cout << "PASS: protocol rejection/atomicity, MIDI real-time/resync/fuzz, USB packet endings\n";
}
