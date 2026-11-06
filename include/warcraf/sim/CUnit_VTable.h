// CUnit_VTable.h -- CUnit virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6fa4a704; real slot count: 118 (slots 0-117).
// CORRECTION vs TinkerWorX: the original adds nothing to CWidget_VTable;
// the real 1.27a CUnit vtable has 118 slots -- CUnit adds slots 107-117.
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CUNIT_VTABLE_H
#define WARCRAF_CUNIT_VTABLE_H

#include "warcraf/sim/CSelectable_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCUnit__Function)(void* self);

typedef void (__thiscall* pCWidget__SetLife)(void* widget, float* life);

// Notable overrides in THIS class (slots inherited from the parent
// vtable, where this class supplies its own function):
//   slot 3: FUN_6f672d60 (648 B): id-dispatch switch on *(param_2+8) (ids 0xd0005, 0xd0006, 0xd000e, 0xd0145-0xd0162, 0xd019d, 0xd019f, 0xd01a1, 0xd01a4, 0xd01b3, 0xd01d4, 0x80264, 0x80265), returns 1 if handled; likely order/event handling -- UNVERIFIED identity (vfunc_decomp.txt)
//   slot 23: FUN_6f65c270: thunk to the base FUN_6f0385d0 (vfunc_decomp.txt)
//   slot 59: FUN_6f668670: returns *(this+0x58) (base CWidget FUN_6f634e60 returns -1); exact meaning UNVERIFIED (S3)
//   slot 72: FUN_6f67dc40 (1498 B): references MagicImmunesResistDamage / EtherealDamageBonusAlly, calls +0x128 on the negative-damage path -- applyDamage override (S3)
//   slot 73: FUN_6f67b310 (52 B): takes float*, clamps against DAT_6fbb81d4, applies via FUN_6f03afb0 -- setLife override (S3)
//   slot 74: FUN_6f652e30 (125 B): float add/sub via software float -- applyHeal override (S3)
struct CUnit_VTable { /* size = 118 */
    CSelectable_VTable selectable;
    pCUnit__Function func107;  // slot 107: FUN_6f66c930 (+0x1ac, size 4164) strings=[Misc, BuildingPlacementNotifyRadius, BuildingsOcclude] -- UNVERIFIED
    pCUnit__Function func108;  // slot 108: FUN_6f66a820 (+0x1b0, size 12) -- UNVERIFIED
    pCUnit__Function func109;  // slot 109: FUN_6f671c20 (+0x1b4, size 49) -- UNVERIFIED
    pCUnit__Function func110;  // slot 110: FUN_6f650260 (+0x1b8, size 2207) -- UNVERIFIED
    pCUnit__Function func111;  // slot 111: FUN_6f661d50 (+0x1bc, size 10) -- UNVERIFIED
    pCUnit__Function func112;  // slot 112: FUN_6f677b60 (+0x1c0, size 21) -- UNVERIFIED
    pCUnit__Function func113;  // slot 113: FUN_6f67a1e0 (+0x1c4, size 245) strings=[ScaleFactor, SelectionCircle] -- UNVERIFIED
    pCUnit__Function func114;  // slot 114: FUN_6f66e930 (+0x1c8, size 130) -- UNVERIFIED
    pCUnit__Function func115;  // slot 115: FUN_6f6771a0 (+0x1cc, size 288) strings=[d:\buildserver\3\work-git\warcraft3-repository\war3\source\unit\CUnitUI.cpp] -- UNVERIFIED
    pCUnit__Function func116;  // slot 116: FUN_6f65d800 (+0x1d0, size 270) -- UNVERIFIED
    pCUnit__Function func117;  // slot 117: FUN_6f65c740 (+0x1d4, size 270) -- UNVERIFIED
};

// Compile-time check: 118 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CUnit_VTable) == 472, "CUnit_VTable must be 118 slots");
#endif

#endif // WARCRAF_CUNIT_VTABLE_H
