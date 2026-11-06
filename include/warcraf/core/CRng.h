// CRng.h -- deterministic table RNG, seeding, rand checksum, software float.
//
// Evidence: ~/workspace/warcraf_stage_rng.md (S11).
//   - Core extract FUN_6f199400 (178 B, 67 callers); 8-byte state (acc + idx);
//     64-dword fixed table DAT_6f964e48 (256 B, full text in S11 appendix A);
//     steps -28/-24/-12/-4, moduli 244/236/212/188, cycles 61/59/53/47,
//     tuple period 8,965,109 (Python-reproduced).
//   - Seed FUN_6f1ae7e0 (seed%61/59/53/47 packing); SetRandomSeed ->
//     FUN_6f1f64b0 + 45-round warmup FUN_6f675fe0; GetRandomInt FUN_6f1e43e0;
//     GetRandomReal FUN_6f1e4420 (23-bit mantissa); boot seeding FUN_6f30d9c0
//     (3 states from game settings DAT_6fbe40a4 +0x1c/+0x40/+0x50); rand
//     checksum FUN_6f3130c0 (stored +0x1c64, XOR const 0x17ab); software
//     float FUN_6f0527e0 __fmul / FUN_6f0528b0 __fsub / FUN_6f0529d0 __fadd;
//     string-hash seed FUN_6f1ae7a0 (h = b + (h*0x7a2d ^ h)).
//   - CRT rand/srand are DEAD imports (0 callers); Storm.dll has no RNG.
//
// UNVERIFIED: the three RNG states' individual purposes / slot mapping; the
// CRandData RTTI (name string at 0x6fbb0370, no type info / vtable found --
// the 8-byte state may or may not be CRandData); the accumulator's full
// output period (a multiple of 8,965,109); GetRandomReal's scaling-chain
// operand mapping; PlaceRandomUnit/Item bodies; FUN_6f30d9c0's trigger point.

#ifndef WARCRAF_CRNG_H
#define WARCRAF_CRNG_H

#include "warcraf/core/Types.h"

// ---- RNG state (8 bytes, S11 §2.1) ------------------------------------------
// Pointer passed in ECX (__fastcall). state[0] = accumulator, also the
// return value of each draw; state[1] = four per-byte counters packed.
struct RandState { /* size = 8 */
    _DWORD acc;  // state[0]: accumulator
    _DWORD idx;  // state[1]: 4 counters packed (one byte per lane)
};

// ---- Fixed random table DAT_6f964e48 (256 B = 64 dwords, S11 appendix A) ----
// Ghidra-measured bytes; all table reads are at multiples of 4, so the
// table is equivalently 64 dwords indexed by byte/4. The same table is
// reused by the checksum hash family (FUN_6f1b0040 et al.).
static const _DWORD kRandTable[64] = {
    0x9927148e, 0x08c7aafd, 0x1f3ee6d5, 0xda55bbf6,
    0x6a4aa075, 0xff97bde8, 0x9fbc9bde, 0x46a18a81,
    0x63e30b6e, 0x5d6c7a76, 0xca69d388, 0x25b947c3,
    0x3fa2ab83, 0xba7c41a6, 0x0195ace5, 0xc109cf7e,
    0x717062d9, 0x0205db8d, 0x54ef8724, 0x3037d4c6,
    0x7bcb1bd0, 0xecd8e4b8, 0xdcadce49, 0xc494a913,
    0x0dae398f, 0x0edd5218, 0x85f5fa78, 0x6dafd258,
    0x3b53b2a4, 0xbe50a551, 0x11f42dfc, 0xf1169848,
    0x663ddf86, 0x2f2e445e, 0x176b0736, 0xb64c298b,
    0xe75f89e2, 0xe121a7cd, 0xed65c94d, 0x239ceefe,
    0x04b77d33, 0x402a9a9e, 0xf35b10b3, 0x921c7782,
    0x571e4e20, 0x8c067222, 0xfb732c67, 0xbf0ac259,
    0x0cf95c79, 0x68121a28, 0x42193474, 0xf884c0b1,
    0x9d15f038, 0x6f3af260, 0x91eb90b4, 0x61357f1d,
    0x5603325a, 0x932bc5a3, 0x434b0f80, 0x3ce0a8f7,
    0x2664d196, 0x4fcc45d7, 0xb5e9b0c8, 0xea31d600,
};

// ---- Extract (FUN_6f199400, S11 §2.3) ----------------------------------------
//   b0 = idx & 0xff, b1 = (idx>>8) & 0xff, b2 = (idx>>16) & 0xff,
//   b3 = (idx>>24) & 0xff;
//   nb0 = b0 >= 28 ? b0-28 : b0+216;  // step -28, mod 244
//   nb1 = b1 >= 24 ? b1-24 : b1+212;  // step -24, mod 236
//   nb2 = b2 >= 12 ? b2-12 : b2+200;  // step -12, mod 212
//   nb3 = b3 >=  4 ? b3- 4 : b3+184;  // step -4,  mod 188
//   mix = rotl32(T[nb1],3) ^ rotl32(T[nb2],2) ^ T[nb0] ^ rotl32(T[nb3],1);
//   state[0] += mix;
//   state[1] = (nb3<<24)|(nb2<<16)|(nb1<<8)|nb0;  return state[0];
// T[i] is an unaligned 32-bit read at byte offset i of the table. Lanes stay
// multiples of 4, so the effective index is b/4 into the 64 dwords above.
// Lane periods: 244/4 = 61, 236/4 = 59, 212/4 = 53, 188/4 = 47 (all prime);
// tuple period = 61*59*53*47 = 8,965,109 draws.
#define RNG_PERIOD_TUPLE  8965109

// ---- Seeding (FUN_6f1ae7e0, S11 §2.5) ----------------------------------------
//   state[0] = seed;
//   state[1] = (seed % 59) * 0x400 | (seed % 61) * 4
//            | (seed % 53) * 0x40000
//            | (((seed / 47) * 0x11 + seed) * 0x4000000);
// Per lane: b0 = (seed%61)*4, b1 = (seed%59)*4, b2 = (seed%53)*4,
// b3 = (((seed/47)*17 + seed) & 0x3f) * 4. b3 may seed to 252 (above the
// 184 cycle cap) but falls into the cycle within 17 draws; table reads
// never exceed offset 252+3.
//   FUN_6f30d9c0 (139 B, no direct callers -- likely callback-dispatched):
//     seeds THREE states from the synced game-settings object (global
//     DAT_6fbe40a4): seed 1 = FUN_6f3071d0() aggregate; seed 2 = *(+0x40),
//     or *(+0x1c) if the former is -1; seed 3 = *(+0x50). Each seeding is
//     followed by FUN_6f3131a0 (state-byte fold). Which state serves which
//     purpose is UNVERIFIED.
//   FUN_6f3071d0 (71 B): aggregate seed -- walks (id, pointer) pairs
//     (count +0x1c, array +0xc, 8 B/item); for id == -2 with a non-null
//     pointer, calls its vtable +0x3c and keeps the last return value.
//     The -2 items' identities and the +0x3c method are UNVERIFIED.
//   FUN_6f1ae7a0 (48 B): string-hash seed: h = s[0]; for each next byte
//     h = b + (h*0x7a2d ^ h). Which string is hashed is UNVERIFIED.
//   FUN_6f025d00 (46 B): object-list aggregate -- walks a pointer array
//     (count +0xc, table +0x10), calls each non-null item's vtable FIRST
//     method, folds with acc = rotl(acc ^ v, 3).

// ---- JASS natives (S11 §3) ---------------------------------------------------
//   GetRandomInt  FUN_6f1e43e0 (55 B): low==high -> low; else
//     low + ((uint64)draw * (|high-low|+1) >> 32) (unbiased range mapping).
//   GetRandomReal FUN_6f1e4420 (165 B): 23-bit mantissa method --
//     (draw & 0x7fffff) | 0x3f800000 builds a [1,2) float, minus 1.0 gives
//     [0,1); scaled by software float mul/add. Interval < 2.0e-07
//     (DAT_6fb59468) returns low directly. Scaling-chain operand mapping
//     UNVERIFIED.
//   SetRandomSeed FUN_6f1f64b0 (36 B): FUN_6f1ae7e0 seed, one throwaway draw,
//     then FUN_6f675fe0 45-round warmup (local_10 = 0x2d loop; each round
//     advances the accumulator by the extract mixing, then re-seeds via
//     FUN_6f1ae7e0 from the evolving accumulator).
//   ChooseRandomCreep FUN_6f1db0a0 (26 B) / ChooseRandomItem FUN_6f1db0c0
//     (24 B) / ChooseRandomItemEx FUN_6f1db0e0 (22 B): thin shells.
//   PlaceRandomUnit FUN_6f1f1d00 / PlaceRandomItem FUN_6f1f1cc0: registered
//     only; bodies NOT decompiled.
//   Internal helpers: FUN_6f199360 (16 B, int range (draw*n)>>32),
//     FUN_6f199380/6f1993d0 (35/40 B, float [0,1) mantissa - 1.0f),
//     FUN_6f19f490 (80 B, float [-1,1)), FUN_6f1b35f0 (35 B, float (0,1]),
//     FUN_6f1b8600 (40 B, float [0,x)), FUN_6f675f30 (43 B, TLS slot read
//     FUN_6f04efb0 then draw then (draw*n)>>32).

// ---- rand checksum (S11 §5) ---------------------------------------------------
// FUN_6f3130c0 (179 B; only caller FUN_6f310c80, the TurnsSync in/out):
//   base = *(p + 0x1c70); if (*(p + 0x2f8) != 0) base = FUN_6f3071d0();
//   s1 = *(setup + 0x40); if (s1 == -1) s1 = *(setup + 0x1c);
//   s2 = *(setup + 0x50);
//   FUN_6f3131a0();  // fold RNG state bytes into the accumulator
//   x = base ^ rotl32(FUN_6f025d00() ^ 0x17ab, 3);
//   x = rotl32(x, 3) ^ s1;  x = rotl32(x, 3) ^ s2;
//   *(p + 0x1c64) = rotl32(x, 6);  // stored in the sync object +0x1c64
// FUN_6f3131a0 (191 B): folds an 8-byte RNG state's bytes with
// acc += b * 0x7a2d ^ b. (fastcall register pass-through: the exact input
// pointers of FUN_6f3131a0 / FUN_6f025d00 in the chain are UNVERIFIED.)
// The seven subsystem checksum print (%srand checksum at 0x6f987e41) has no
// direct code reference; the print function is UNVERIFIED.

// ---- Software float (S11 §6) -------------------------------------------------
// FUN_6f0527e0 __fmul (538 callers), FUN_6f0528b0 __fsub (642 callers),
// FUN_6f0529d0 __fadd (667 callers): pure-integer IEEE float mul/sub/add
// (normalisation helper FUN_6f052b50). All engine float hot paths use them,
// so results do not depend on x87/SSE rounding differences.
// TLS slot read: FUN_6f04efb0 (27 B, 340 callers) = TlsGetValue then slot
// lookup (root FUN_6f158a30).
// ---- Compile-time layout checks -------------------------------------------
// RNG state is 8 bytes; the fixed table is 64 dwords = 256 bytes (S11).
static_assert(sizeof(RandState) == 8, "RandState must be 8 bytes");
static_assert(sizeof(kRandTable) == 256, "kRandTable must be 256 bytes");

#endif // WARCRAF_CRNG_H
