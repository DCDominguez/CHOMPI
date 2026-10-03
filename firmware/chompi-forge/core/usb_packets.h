#pragma once
#include <cstddef>
#include <cstdint>

namespace forge {
// Encode ONE complete SysEx envelope into USB-MIDI event packets. The F7
// belongs in the final packet; it must not be split off as another message.
inline size_t PackUsbSysEx(const uint8_t* message, size_t size, uint8_t* out, size_t capacity) {
    if(size < 2 || message[0] != 0xf0 || message[size - 1] != 0xf7
       || ((size + 2) / 3) * 4 > capacity) return 0;
    for(size_t i = 1; i + 1 < size; ++i) if(message[i] > 127) return 0;
    size_t written = 0;
    for(size_t i = 0; i < size; i += 3) {
        const size_t remaining = size - i;
        const size_t count = remaining > 3 ? 3 : remaining;
        out[written++] = remaining > 3 ? 0x04 : static_cast<uint8_t>(0x04 + count);
        for(size_t j = 0; j < 3; ++j) out[written++] = j < count ? message[i + j] : 0;
    }
    return written;
}
} // namespace forge
