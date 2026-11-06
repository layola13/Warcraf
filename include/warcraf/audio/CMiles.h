// CMiles.h -- Miles Sound System 6.1d audio (Mss32.dll) + Game-side sound.
//
// Evidence: ~/workspace/warcraf_stage_audio.md (S8).
//   - Game side: CSoundWar3 / CSoundManager; init FUN_6f339df0 (1087 B).
//   - Miles 6.1d: Mss32.dll base 0x21100000, 319 AIL_* exports, Game imports
//     110 AIL_* (2 dead imports: _AIL_digital_CPU_percent@4,
//     _AIL_set_3D_speaker_type@8, 0 callers).
//   - Sound object HLOOPINGSOUND 0x1d4 B (offsets below); MIDI variant
//     0x14c B; handle pools 3x16 records of 0x10 B; max 24 concurrent
//     sounds (0x18); file callbacks via Storm 267/269/271/253; 3D listener /
//     provider handles DAT_6fbda508 / DAT_6fbda95c.
//
// The Miles API function list (which of the 110 imports is used where), the
// driver / XMIDI / DLS open chain details, the preemption and 3D-panning
// internals beyond the listed functions, and the CSoundWar3 / CSoundManager
// object layouts are UNVERIFIED.

#ifndef WARCRAF_CMILES_H
#define WARCRAF_CMILES_H

#include "warcraf/core/Types.h"

// ---- Miles facts (verified, S8) --------------------------------------------
//   Mss32.dll base 0x21100000; 319 AIL_* exports; Game imports 110 AIL_*.
//   Dead imports (0 callers): _AIL_digital_CPU_percent@4,
//   _AIL_set_3D_speaker_type@8.
//   File callbacks route through Storm ordinals 267/269/271/253 (SFile
//   OpenFile / ReadFile / SetFilePointer / CloseFile -- see io/SFile.h).
//   3D listener handle DAT_6fbda508; 3D provider handle DAT_6fbda95c.
//   22050 Hz warning string exists (details UNVERIFIED).

// ---- HLOOPINGSOUND sound object (0x1d4 bytes, S8) --------------------------
// Field meanings below are as evidenced in S8; gaps are placeholders.
// Total size 0x1d4 is evidenced.
struct MilesSound { /* size = 0x1d4 */
    _DWORD field_0[0xd];   // +0x00-0x33: UNVERIFIED
    char   filename[0x104]; // +0x34: file name (0x104 bytes)
    _DWORD field_138;      // +0x138: UNVERIFIED
    _DWORD flags;          // +0x13c
    _DWORD priority;       // +0x140
    _DWORD volume;         // +0x144
    _DWORD username;       // +0x148
    _DWORD type;           // +0x14c: 1 = 2D, 2 = 3D
    _DWORD field_150;
    _DWORD field_154;
    _DWORD field_158;
    _DWORD handle3D;       // +0x15c: 3D handle
    _DWORD handle2D;       // +0x160: 2D handle
    _DWORD handleStream;   // +0x164: stream handle
    _DWORD handleSequence; // +0x168: sequence handle
    _DWORD field_16C;
    _DWORD field_170;
    _DWORD field_174;
    _DWORD channel;        // +0x178
    _DWORD field_17C[0xb]; // +0x17c-0x1a7: UNVERIFIED
    _DWORD field_1A8;      // +0x1a8: UNVERIFIED
    _DWORD state;          // +0x1ac: 2 = delayed/queued, 3 = playing,
                           // 5 = stopped
    _DWORD field_1B0;
    _DWORD field_1B4;
    _DWORD params3D;       // +0x1b8: 3D parameters (exact layout UNVERIFIED)
    _DWORD cutoff;         // +0x1bc: cutoff
    _DWORD field_1C0;
    float  posX;           // +0x1c4: 3D position X
    float  posY;           // +0x1c8: 3D position Y
    float  posZ;           // +0x1cc: 3D position Z
    _DWORD field_1D0;      // +0x1d0: last dword (to 0x1d4)
};
// MIDI sound object variant: 0x14c bytes total (layout UNVERIFIED).

// ---- Handle pools (S8) -------------------------------------------------------
// Three pools of 16 records, each record 0x10 bytes (pool identities and
// record layouts UNVERIFIED). Max 24 concurrent sounds (0x18) -- preemption
// in FUN_6f171380.

// ---- Key functions (verified addresses, S8) ---------------------------------
//   FUN_6f339df0  audio init (1087 B)
//   FUN_6f171ba0  playback dispatcher (stream / sequence / 2D / 3D)
//   FUN_6f171380  preemption (24-sound cap)
//   FUN_6f16cbe0  3D listener change detection + per-sound parameter
//                 delivery (EAX2)
//   FUN_6f173cb0  unified volume (distance attenuation sqrt/pow)
//   FUN_6f144a00  file load + info cache (304 B; via Storm 267/269/253;
//                 shared with S7)
//   FUN_6f145e30 / FUN_6f1462c0  periodic audio sweep -- CALLERS UNVERIFIED
//                 (S12 residual point 1: the sweep's caller is unknown)
// ---- Compile-time layout checks (verified offsets only) -------------------
#include <cstddef>
static_assert(sizeof(MilesSound) == 0x1d4, "MilesSound must be 0x1d4 bytes");
static_assert(offsetof(MilesSound, filename) == 0x34, "filename +0x34");
static_assert(offsetof(MilesSound, flags) == 0x13c, "flags +0x13c");
static_assert(offsetof(MilesSound, priority) == 0x140, "priority +0x140");
static_assert(offsetof(MilesSound, volume) == 0x144, "volume +0x144");
static_assert(offsetof(MilesSound, username) == 0x148, "username +0x148");
static_assert(offsetof(MilesSound, type) == 0x14c, "type +0x14c");
static_assert(offsetof(MilesSound, handle3D) == 0x15c, "handle3D +0x15c");
static_assert(offsetof(MilesSound, handle2D) == 0x160, "handle2D +0x160");
static_assert(offsetof(MilesSound, handleStream) == 0x164, "handleStream +0x164");
static_assert(offsetof(MilesSound, handleSequence) == 0x168, "handleSequence +0x168");
static_assert(offsetof(MilesSound, channel) == 0x178, "channel +0x178");
static_assert(offsetof(MilesSound, state) == 0x1ac, "state +0x1ac");
static_assert(offsetof(MilesSound, params3D) == 0x1b8, "params3D +0x1b8");
static_assert(offsetof(MilesSound, cutoff) == 0x1bc, "cutoff +0x1bc");
static_assert(offsetof(MilesSound, posX) == 0x1c4, "posX +0x1c4");
static_assert(offsetof(MilesSound, posY) == 0x1c8, "posY +0x1c8");
static_assert(offsetof(MilesSound, posZ) == 0x1cc, "posZ +0x1cc");

#endif // WARCRAF_CMILES_H
