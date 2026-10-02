// Offline harness: raw hex MIDI on stdin -> one hex reply per request.
// Optional --render writes four seconds of a synthetic stereo pluck through
// the SAME Engine as firmware. Diagnostics here are synthetic zero readings.
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../core/midi_framer.h"
#include "../core/runtime.h"

void Word(std::ostream& out, uint32_t value, unsigned bytes) {
    for(unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
void Render(forge::Engine& engine, const char* path) {
    std::ofstream out(path, std::ios::binary);
    if(!out) throw std::runtime_error("Cannot open WAV output");
    constexpr unsigned rate = 48000, frames = rate * 4, data_size = frames * 4;
    out.write("RIFF", 4); Word(out, 36 + data_size, 4); out.write("WAVEfmt ", 8);
    Word(out, 16, 4); Word(out, 1, 2); Word(out, 2, 2); Word(out, rate, 4);
    Word(out, rate * 4, 4); Word(out, 4, 2); Word(out, 16, 2);
    out.write("data", 4); Word(out, data_size, 4);
    float left, right;
    // Settle parameter ramps before the test signal.
    for(unsigned i = 0; i < rate; ++i) engine.Process(0, 0, left, right);
    if(engine.GetParameters().synth) { engine.Note(60, 100, 2); engine.Note(64, 90, 2); engine.Note(67, 90, 2); }
    for(unsigned i = 0; i < frames; ++i) {
        if(i == rate) { engine.Note(60, 0, 2); engine.Note(64, 0, 2); engine.Note(67, 0, 2); }
        const float t = float(i) / rate;
        const float envelope = i < rate / 2 ? std::exp(-t * 12.f) : 0.f;
        const float l = 0.6f * envelope * std::sin(6.2831853f * 220.f * t);
        const float r = 0.6f * envelope * std::sin(6.2831853f * 330.f * t);
        engine.Process(l, r, left, right);
        Word(out, static_cast<uint16_t>(static_cast<int16_t>(left * 32767.f)), 2);
        Word(out, static_cast<uint16_t>(static_cast<int16_t>(right * 32767.f)), 2);
    }
    if(!out) throw std::runtime_error("Failed writing WAV");
}
int main(int argc, char** argv) {
    if(argc != 1 && !(argc == 3 && std::string(argv[1]) == "--render")) return 2;
    forge::Engine engine; std::vector<float> l(48002), r(48002);
    if(!engine.Init(48000, l.data(), r.data(), l.size())) return 2;
    forge::MidiFramer parser; forge::MidiFrame frame;
    unsigned byte, replies = 0;
    while(std::cin >> std::hex >> byte) {
        if(byte > 255) return 2;
        if(!parser.Feed(static_cast<uint8_t>(byte), frame)) continue;
        if(frame.kind != forge::MidiFrame::Kind::SysEx || !forge::IsRequest(frame.data, frame.size)) continue;
        forge::Request request; uint8_t reply[42]; size_t size;
        auto error = forge::DecodeRequest(frame.data, frame.size, request);
        if(error != forge::Error::None) size = forge::EncodeError(forge::Read14(frame.data + 5), error, reply);
        else {
            forge::Response response; forge::ExecuteRequest(request, engine, response);
            size = forge::EncodeResponse(response, 0, 0, reply);
        }
        for(size_t i = 0; i < size; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(reply[i]) << ' ';
        std::cout << std::endl; ++replies; // flush: lets a host keep one stateful probe open
    }
    if(!std::cin.eof() || !replies) return 2;
    try { if(argc == 3) Render(engine, argv[2]); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
