// Jass.h -- JASS2 virtual machine, native registry, triggers, handles.
//
// Evidence:
//   - ~/workspace/warcraf_stage_jass.md (S4): interpreter FUN_6f7f9070
//     (6160 B), opcode tables, two native registries (FUN_6f1e9a50 with 1167
//     BindNative calls; FUN_6f6f5d00), BindNative FUN_6f7e3710, type codes,
//     RTTI names, common.j references.
//   - ~/workspace/warcraf_stage_simulation.md (S3): native signature table
//     (UnitDamageTarget / UnitDamagePoint / GetUnitState / SetUnitState /
//     IssueOrder / ...).
//   - ~/workspace/warcraf_stage_rng.md (S11): BindNative registration form
//     (ECX = implementation, EDX = name, stack = type signature).
//   - ~/workspace/warcraftIII/IDA/Map Natives.idc (TinkerWorX): native table
//     entry layout (0x14 bytes), BindNative __fastcall (func, name, args).
//
// Interpreter internals (opcode semantics, value stack layout, the second
// registry's role) are largely UNVERIFIED. The native-name <-> signature
// pairing inside the registries is UNVERIFIED (BindNative takes only the
// signature; names live in a separate table).

#ifndef WARCRAF_JASS_H
#define WARCRAF_JASS_H

#include "warcraf/core/Types.h"

// ---- Interpreter (S4) -------------------------------------------------------
// FUN_6f7f9070 (6160 B): JASS2 interpreter main loop. Opcode tables at
// DAT_6fb6bc08 / DAT_6fb6bf20 / DAT_6fb6ba48 / 0x6fb6c4b0; opcode ranges
// < 0x9c / < 0x36 (table-dependent). Source clue: the interpreter cites
// "Jass2/Nodes.h". Opcode semantics UNVERIFIED; the interpreter's only
// caller identity is UNVERIFIED.
// JASS state pointers are passed through ECX directly to natives (S11).
//   FUN_6f7f9070 -- JASS2 interpreter (address only; signature UNVERIFIED).

// ---- Native registry (S3/S4/S11) -------------------------------------------
// FUN_6f1e9a50 (23,346 B): registry A -- 1167 BindNative calls (common.j
// table, inferred). FUN_6f6f5d00 (25,844 B): registry B (Blizzard.j table,
// inferred; NOT decompiled). Both register the same name set in the RNG
// stage's sample (GetRandomInt/Real/SetRandomSeed/...).
// BindNative: FUN_6f7e3710, __fastcall (func, name, args) per Map Natives.idc;
// per S11: ECX = implementation address, EDX = name string, stack = type
// signature string. InitGameNatives: FUN_6f1e9a50 (the registry itself is
// the init function -- "InitGameNatives" is the TinkerWorX/IDC name for it;
// the identity FUN_6f1e9a50 = InitGameNatives comes from Map Natives.idc
// via the "Deg2Rad" string anchor).
//   FUN_6f1e9a50 -- InitGameNatives / registry A
//   FUN_6f6f5d00 -- registry B (Blizzard.j, inferred)
//   FUN_6f7e3710 -- BindNative (native registration helper)

// Native table entry layout (Map Natives.idc, 0x14 = 20 bytes):
//   +0x1: args-string pointer (type signature, e.g. "(Hunit;R)V")
//   +0x6: name pointer (e.g. "GetRandomInt")
//   +0xB: function pointer (implementation)
// (Offsets are relative to the entry as the IDC script reads them; the exact
// entry base alignment is UNVERIFIED -- see the IDC script.)
struct JassNativeEntry { /* size = 0x14 */
    _BYTE  field_0;
    char*  args;       // +0x01: type-signature string pointer
    _BYTE  field_5;
    char*  name;       // +0x06: native name string pointer
    _BYTE  field_A;
    void*  func;       // +0x0b: implementation function pointer
    _DWORD field_F;
    _DWORD field_13;   // last byte at +0x13 (size 0x14 total)
};

// Type codes in native signatures (S3/S4, verified):
//   R = real, I = integer, S = string, B = boolean, H = handle,
//   C = code, V = void (no return).
// Examples (S3, signatures evidenced; name pairing by common.j convention,
// inferred):
//   (Hunit;Hwidget;RBBHattacktype;Hdamagetype;Hweapontype;)B -- UnitDamageTarget
//   (Hunit;RRRRRBBHattacktype;Hdamagetype;Hweapontype;)B    -- UnitDamagePoint
//   (Hunit;Hunitstate;)R / (Hunit;Hunitstate;R)V            -- Get/SetUnitState
//   (Hunit;S)B / (Hunit;SRR)B / (Hunit;SHlocation;)B         -- IssueOrder family
//   (Hunit;Hitem;)B / (Hunit;I)Hitem;                        -- inventory
//   (Htrigger;Hunit;Hunitevent;)Hevent;                      -- unit event reg

// ---- JASS runtime types (RTTI names, S4) -----------------------------------
//   JassHandle  -- base class of all JASS handles (units, triggers, timers,
//                  dialogs, ...). Layout UNVERIFIED.
//   JassString  -- script string object. Layout UNVERIFIED.
//   JassFrame   -- call-stack frame. Layout UNVERIFIED (S6 notes a possible
//                  use, also UNVERIFIED).
//   NATIVETOKEN -- native token. Layout UNVERIFIED.
//   NativeFunc  -- native function object. Layout UNVERIFIED.
//   ScriptData / ScriptError -- script data / script error objects.
//   Script2HandleReg -- script-to-handle registry.
//   TriggerActionNode -- trigger action list node.
//   TriggerConditionNode -- trigger condition list node.
//   TriggerString@CTriggerStrings -- trigger string table.
//   GameHashTableBooleanReg / HandleReg / IntegerReg / RealReg / StringReg --
//     game hash tables split by value type (backs SaveXxxHandle/LoadXxxHandle).
// All layouts are UNVERIFIED; only the RTTI names are evidenced.
struct JassHandle { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct JassString { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct JassFrame { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct NativeFunc { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct TriggerActionNode { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct TriggerConditionNode { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};

// ---- Script loading (S4) -----------------------------------------------------
// "common.j": 2 string references in Game.dll. "war3map.j" / "Blizzard.j" /
// "InitGlobals": 0 hits -- the map-script loading path is UNVERIFIED.
// TriggerSleepAction's interaction with turn advancement: UNVERIFIED.
// ---- Compile-time layout checks -------------------------------------------
// The native entry is 0x14 bytes (Map Natives.idc); it contains pointers,
// so the asserts only fire on a 32-bit target (see Types.h).
#include <cstddef>
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(JassNativeEntry) == 0x14, "JassNativeEntry must be 0x14 bytes");
static_assert(offsetof(JassNativeEntry, args) == 0x01, "args +0x01");
static_assert(offsetof(JassNativeEntry, name) == 0x06, "name +0x06");
static_assert(offsetof(JassNativeEntry, func) == 0x0b, "func +0x0b");
#endif

#endif // WARCRAF_JASS_H
