// NetChecksum.h -- network event checksum, as implemented in the original.
//
// Evidence: FUN_6f30ade0 (event pump, 954 B) decompiled in
//   ~/workspace/warcraf_ghidra/sync_decomp.txt (lines ~830-910), and
//   ~/workspace/warcraf_stage_network.md (S1) section 3.2 / section 6.
//
// What the decompile shows (transcribed, not copied):
//   - For event types 0x1e and 0x1f, the pump rolls the event payload:
//     acc starts at 0; each little-endian dword feeds
//     acc = rotl32(acc ^ word, 3); each tail byte feeds
//     acc = rotl32(acc ^ byte, 3).
//     (The tail loop in the decompile reads "(acc ^ byte) << 3 | acc >> 29";
//     that is the same value as rotl32(acc ^ byte, 3), because the byte
//     only touches bits 0..7 while the >> 29 side reads bits 29..31.)
//   - The rolled value is folded per byte with the 0x7a2d constant:
//     fold(x) = sum over the 4 bytes b of x of ((b * 0x7a2d) ^ b).
//     The event value is the event type byte plus fold(roll) for 0x1e/0x1f,
//     and just the type byte for other game event types.
//   - The session accumulator (CNetData +0x227c) grows by
//     fold(event value) for each pumped game event that passes the gate.
//   - The gate (also from the decompile): the checksum block runs only
//     when (event flag != 0) == (local slot == 0) and frame state
//     (+0x278) > 3. The fold into the session accumulator runs only when
//     the pump's second argument (param_2) is non-zero; with param_2 == 0
//     the pump stops after computing the first gate-passing event's
//     checksum value -- before folding it and before dispatching that
//     event. A dispatched 0x1e or 0x1f event also stops the pump
//     (local_38 is set in the shared 0x1e/0x1f checksum block), so at
//     most one 0x1e/0x1f event is dispatched per pump call.
//
// NOTE on a discrepancy: S1 section 3.2 says "type 0x1f events take part
// in the checksum". The decompile shows 0x1e falls into the same checksum
// block as 0x1f (the `if (type != 0x1e)` guard skips only the dispatch
// prep, not the roll). This implementation follows the decompile.
//
// The seven subsystem checksums ("%scheat / %scombat / ..." at
// 0x6f987de8-0x6f987e6b) are NOT implemented here: their hash functions
// are unrecovered (callback-registered, no static callers -- S1 section 6).
// Only the event-stream checksum above is evidenced.

#ifndef WARCRAF_NET_CHECKSUM_H
#define WARCRAF_NET_CHECKSUM_H

#include <cstddef>
#include <cstdint>

namespace warcraf::net {

// Rolls the payload of one event, exactly as FUN_6f30ade0 does for
// 0x1e/0x1f events: acc = rotl32(acc ^ word, 3) per dword, then
// acc = rotl32(acc ^ byte, 3) per tail byte. Empty payload rolls to 0.
uint32_t RollPayloadChecksum(const uint8_t* data, size_t length);

// Folds one 32-bit value per byte with the 0x7a2d constant (WARCRAF_MIX_CONST):
//   fold(x) = sum over bytes b of x of ((b * 0x7a2d) ^ b)   (mod 2^32)
// Used twice in FUN_6f30ade0: once to combine the rolled payload with the
// event type, once to fold the event value into the session accumulator.
uint32_t FoldChecksumWord(uint32_t value);

// The per-event value the pump folds into the session accumulator
// (local_28 in FUN_6f30ade0): the event type byte, plus
// FoldChecksumWord(RollPayloadChecksum(payload)) for types 0x1e/0x1f.
// For other game event types the value is just the type byte.
uint32_t EventChecksumValue(uint8_t type, const uint8_t* payload, size_t length);

// Adds fold(eventValue) to the session accumulator (CNetData +0x227c).
void FoldIntoSessionChecksum(uint32_t& sessionChecksum, uint32_t eventValue);

}  // namespace warcraf::net

#endif  // WARCRAF_NET_CHECKSUM_H
