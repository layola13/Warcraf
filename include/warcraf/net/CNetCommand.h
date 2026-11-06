// CNetCommand.h -- turn commands: objects, queue, packet buffer, damage data.
//
// Evidence:
//   - ~/workspace/warcraf_stage_simulation.md (S3): 24-byte command objects
//     (FUN_6f30fbb0), enqueue chain (FUN_6f30b940 -> FUN_6f30b920 ->
//     FUN_6f30ce40 -> FUN_6f30ceb0), CDataStore primitives, WidgetDamageData
//     (12 B/entry), AoE/single damage (FUN_6f6b19e0 / FUN_6f6af530).
//   - ~/workspace/warcraf_stage_mainloop.md (S2): turn apply FUN_6f30bac0
//     command dispatch table (0x10-0x31).
//   - ~/workspace/warcraf_stage_replay.md (S9): replay/save/view-speed
//     command class names (RTTI).
//
// Command-number semantics (which number is move / attack / cast) are
// UNVERIFIED. The command-queue CONSUMER (drain) function was not found (S3
// unvalidated point 1). Cheat/replay/save subclass LAYOUTS are unverified;
// only their RTTI names are evidenced.

#ifndef WARCRAF_CNETCOMMAND_H
#define WARCRAF_CNETCOMMAND_H

#include "warcraf/core/Types.h"

// ---- CNetCommandBase (24 bytes = 0x18, S3) --------------------------------
// Built by FUN_6f30fbb0 (94 B); allocated by FUN_6f30c4a0 (112 B, pool alloc
// FUN_6f78a4a0, 0x18 bytes). First 6 dwords are zeroed, then +0x14 is set to
// 0xff and +0x15 to 0 on creation (*(u16*)(obj+5) = 0xff -- see S3 §2).
struct CNetCommandBase { /* size = 24 */
    _DWORD field_0;    // +0x00: zeroed on creation (meaning UNVERIFIED)
    _DWORD field_4;    // +0x04: zeroed on creation (meaning UNVERIFIED)
    _DWORD value1;     // +0x08: value 1 (FUN_6f30fbb0 param_2)
    _DWORD value2;     // +0x0c: value 2 (param_3)
    _DWORD value3;     // +0x10: value 3 (param_4)
    _BYTE  command;    // +0x14: command number (param_1)
    _BYTE  flag;       // +0x15: flag (param_5; == 2 triggers anchor lookup
                       // in FUN_6f30ce40)
    _BYTE  field_16;
    _BYTE  field_17;
};

// Command numbers handled by the turn apply FUN_6f30bac0 (S2 dispatch table).
// Each number first calls a FUN_6f83exxx reader into a stack CDataStore, then
// a 185-byte FUN_6f2fxxxx unpack shell, then FUN_6f30ba60 -> FUN_6f30b940.
// Per-number SEMANTICS are UNVERIFIED; only the dispatch mapping is known.
//   0x10 -> FUN_6f2fdbd0    0x16 -> FUN_6f2fdf90    0x17 -> FUN_6f2fe050
//   0x19 -> FUN_6f2fdd50    0x1a -> FUN_6f2fdb10    0x1b -> FUN_6f2fde10
//   0x1c -> FUN_6f2fdc90    0x20 -> FUN_6f2fded0    0x21 -> FUN_6f2fe110
//   0x22 -> FUN_6f2fe410    0x23 -> FUN_6f2fe4d0 (on failure: Ordinal_403 log)
//   0x2f -> FUN_6f2fe1d0    0x30 -> FUN_6f2fe290    0x31 -> FUN_6f2fe350
//   0x1e/0x1f batch path: FUN_6f04d420 length, FUN_6f83f8e0 + FUN_6f04d5d0
//     read, FUN_6f30b940 execute; 0x1f sets the end flag.
//   default -> FUN_6f30e9c0(*(this+0x610)) (identity UNVERIFIED).
// Command-number ranges in FUN_6f30b940: < 0x40 network turn path
// (FUN_6f83ff00 release/discard on one branch), 0x40-0xfe local path
// (FUN_6f791c50 assert/log), else enqueue via FUN_6f30b920. Local submit
// entries FUN_6f30b8c0 (caller FUN_6f30e540) and FUN_6f306fd0 (caller
// FUN_6f30d640) check session +0x2288 != 'NONE'; FUN_6f306fd0 adds 0x40 to
// the command byte, so locally issued commands land in >= 0x40.

// ---- Command queue (S3) -----------------------------------------------------
// FUN_6f30ce40 (107 B): finds the anchor node (+0x15 == 2) then calls
// FUN_6f30ceb0; increments the per-command counter at
// (session+0xc + (cmd + flag*0xff)*4) -- one counter slot per command number.
// FUN_6f30ceb0 (160 B): doubly-linked list splice; mode != 1 inserts after
// the anchor, mode == 1 inserts before it. The queue CONSUMER (drain)
// function was NOT FOUND -- who dequeues commands and executes them as unit
// behaviour (move/attack/cast) is UNVERIFIED.

// ---- CDataStore (packet buffer; source path "engine/source/base/CDataStore.h") ----
// Primitives (S3):
//   FUN_6f04e1d0 (81 B): write dword -- grows via vtable +0xc on overflow,
//     then *(base - begin + pos) = val, pos += 4.
//   FUN_6f04dfa0 (80 B): write byte, pos += 1.
//   FUN_6f83f8e0 (118 B): buffer alloc/realloc (Ordinal_401/405;
//     CDataStore.h:0xf9/0x11b).
//   FUN_6f83ff00 / FUN_6f791c50 (57/23 B): release / assert (CDataStore.h:0x10a).
//   FUN_6f30ba60 (90 B): reads three values via CDataStore vtable +0x28,
//     then calls FUN_6f30b940.
// Per-command readers (FUN_6f83exxx): 0x1a/0x1b/0x1c read one dword
// (FUN_6f83ee60/6f83f010/6f83efd0, 14 B each); 0x16 reads 1 byte + call +
// dword at +0x1c (FUN_6f83f060, 40 B); 0x17 reads 1 dword + 1 byte + 2 dwords
// (FUN_6f83f090, 50 B); 0x23 reads 1 dword + 1 byte + variable data from +5
// + count elements at +0x1b (FUN_6f83f380, 91 B; variable-length list).
// The CDataStore object layout itself is UNVERIFIED (only the vtable slots
// +0x0c grow and +0x28 read-three are evidenced).
struct CDataStore { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable; +0x0c grow, +0x28 read-three (S3)
    _DWORD field_4;    // UNVERIFIED
};

// ---- WidgetDamageData (RTTI ?AUWidgetDamageData@@, 0x6fbb1e7c; S3) ---------
// 12 bytes per entry: [0] target widget pointer, [1] float damage amount,
// [2] category (0/1/2 -- meanings UNVERIFIED; three per-category counters at
// param_2+5/+6/+7 in FUN_6f6af6c0).
struct WidgetDamageData { /* size = 12 */
    void*  target;     // [0] target widget pointer
    float  amount;     // [1] float damage amount
    _DWORD category;   // [2] category 0/1/2 (meaning UNVERIFIED)
};
// Vector growth: FUN_6f6b81c0 (153 B, 12-byte stride, Ordinal_405/401,
// type name "s___AUWidgetDamageData___"); release: FUN_6f6aeb00 (23 B).
// Builders: FUN_6f6af6c0 (798 B, appends one entry; picks the max of a float
// array at param_2+0xe, length param_2[0x12]; sentinels 0x10/0x20/0x4000000/
// 0x400000 at param_2[0x14]).

// ---- Damage application (S3) -----------------------------------------------
// AoE: FUN_6f6b19e0 (1099 B, __fastcall, 14 params). Enumerates the region
// (FUN_6f661060, 2957 B, ~130 callers, NOT decompiled -- region enumeration,
// inferred; or FUN_6f633a40, 812 B, widget region query with opcodes
// 0x20-0x2b when param_8 & 0x7e0 != 0), filters each entry through callback
// param_11, then calls FUN_6f6af530 per entry. Radius starts from
// DAT_6fbb81d4 (value UNVERIFIED).
// Single damage/heal: FUN_6f6af530 (378 B; only caller FUN_6f6b19e0).
//   *param_2 >= DAT_6fbb81d4 -> damage branch: FUN_6f685690, then target
//     vtable +0x120 (damage entry -- see sim/CWidget_VTable.h slot 72).
//   else -> heal branch: EtherealHealBonus handling (FUN_6f02a4b0), then
//     target vtable +0x128 (heal entry -- slot 74), or FUN_6f662d60.
//   DAT_6fbb81d4 value, the +0x120/+0x128 target functions' own identities
//   beyond the slot, and the 0/1/2 category meanings are UNVERIFIED.
//   Attacker index: current attacker (FUN_6f667a80) is written via vtable
//   +0xec (slot 59) into *(param_3 + 0x6c + idx*4) ("last damage source"
//   table, inferred).

// ---- Command subclasses (RTTI names only, S0/S9) ---------------------------
// Cheat commands: CNetCommandCheatBase + 20+ subclasses (God/Gold/Lumber/
// FastBuild/ShowMap/Victory/Defeat, ... -- full list UNVERIFIED; names from
// S0). Also: CNetCommandBase family AllyPing / ArrowKey / CAbility*Command.
// Replay/save/view-speed (S9):
//   CNetCommandReplayBase, CNetCommandReplayDataCompressed,
//   CNetCommandReplayDataUncompressed, CNetCommandReplayDataDone,
//   CNetCommandReplayViewSpeedIncrement, CNetCommandReplayViewSpeedDecrement,
//   CNetCommandSave, CNetCommandSaveDone, CNetEventSaveComplete.
// Layouts of all subclasses are UNVERIFIED; only the names are evidenced.
// ---- Compile-time layout checks (verified offsets only) -------------------
#include <cstddef>
static_assert(sizeof(CNetCommandBase) == 24, "CNetCommandBase must be 24 bytes");
static_assert(offsetof(CNetCommandBase, value1) == 0x08, "value1 +0x08");
static_assert(offsetof(CNetCommandBase, command) == 0x14, "command +0x14");
static_assert(offsetof(CNetCommandBase, flag) == 0x15, "flag +0x15");
// WidgetDamageData contains a pointer: 32-bit target only (see Types.h).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(WidgetDamageData) == 12, "WidgetDamageData must be 12 bytes");
#endif

#endif // WARCRAF_CNETCOMMAND_H
