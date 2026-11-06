// NetChecksum.h -- implementation.
//
// Each function transcribes the corresponding fragment of FUN_6f30ade0
// (sync_decomp.txt) in this project's own code. No decompiled pseudocode
// is pasted; the constants (3, 0x7a2d) and the control flow are the
// original's.

#include "warcraf/net/NetChecksum.h"

namespace warcraf::net {
namespace {

// rotl32: the decompile's `(x << 3) | (x >> 0x1d)` pattern.
inline uint32_t RotateLeft3(uint32_t value) {
    return (value << 3) | (value >> 29);
}

}  // namespace

uint32_t RollPayloadChecksum(const uint8_t* data, size_t length) {
    uint32_t acc = 0;
    size_t i = 0;
    // Whole dwords, little-endian (the original reads uint32s from the
    // payload pointer directly).
    for (; i + 4 <= length; i += 4) {
        uint32_t word = static_cast<uint32_t>(data[i]) |
                        (static_cast<uint32_t>(data[i + 1]) << 8) |
                        (static_cast<uint32_t>(data[i + 2]) << 16) |
                        (static_cast<uint32_t>(data[i + 3]) << 24);
        acc = RotateLeft3(acc ^ word);
    }
    // Tail bytes, one at a time.
    for (; i < length; ++i) {
        acc = RotateLeft3(acc ^ data[i]);
    }
    return acc;
}

uint32_t FoldChecksumWord(uint32_t value) {
    uint32_t sum = 0;
    // One term per byte: (byte * 0x7a2d) ^ byte. The decompile adds the
    // four terms in the order byte3, byte0, byte1, byte2 across two
    // statements; addition is commutative, so the order is irrelevant.
    for (int shift = 24; shift >= 0; shift -= 8) {
        uint32_t byte = (value >> shift) & 0xff;
        sum += (byte * 0x7a2d) ^ byte;
    }
    return sum;
}

uint32_t EventChecksumValue(uint8_t type, const uint8_t* payload,
                            size_t length) {
    uint32_t value = type;
    // The decompile's checksum block runs for 0x1e and 0x1f (0x1e falls
    // into the 0x1f block: `if (type != 0x1e)` skips only the dispatch
    // prep). S1's text mentions 0x1f only; the decompile governs.
    if (type == 0x1e || type == 0x1f) {
        value += FoldChecksumWord(RollPayloadChecksum(payload, length));
    }
    return value;
}

void FoldIntoSessionChecksum(uint32_t& sessionChecksum, uint32_t eventValue) {
    sessionChecksum += FoldChecksumWord(eventValue);
}

}  // namespace warcraf::net
