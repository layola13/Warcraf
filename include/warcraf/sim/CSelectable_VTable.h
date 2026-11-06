// CSelectable_VTable.h -- CSelectable virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6fa4a118; real slot count: 107 (slots 0-106).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CSELECTABLE_VTABLE_H
#define WARCRAF_CSELECTABLE_VTABLE_H

#include "warcraf/sim/CWidget_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCSelectable__Function)(void* self);

typedef void (__thiscall* pCWidget__SetLife)(void* widget, float* life);

struct CSelectable_VTable { /* size = 107 */
    CWidget_VTable widget;
    pCSelectable__Function func96;  // slot 96: FUN_6f637630 (+0x180, size 58) -- UNVERIFIED
    pCSelectable__Function func97;  // slot 97: FUN_6f637170 (+0x184, size 24) -- UNVERIFIED
    pCSelectable__Function func98;  // slot 98: FUN_6f637300 (+0x188, size 3) -- UNVERIFIED
    pCSelectable__Function func99;  // slot 99: FUN_6f637310 (+0x18c, size 3) -- UNVERIFIED
    pCSelectable__Function func100;  // slot 100: FUN_6f637360 (+0x190, size 271) strings=[ColorNeutral, SelectionCircle] -- UNVERIFIED
    pCSelectable__Function func101;  // slot 101: FUN_6f637540 (+0x194, size 230) strings=[ColorNeutral, SelectionCircle] -- UNVERIFIED
    pCSelectable__Function func102;  // slot 102: FUN_6f6374d0 (+0x198, size 1) -- UNVERIFIED
    pCSelectable__Function func103;  // slot 103: FUN_6f637350 (+0x19c, size 5) -- UNVERIFIED
    pCSelectable__Function func104;  // slot 104: FUN_6f637730 (+0x1a0, size 205) strings=[ImageWalkableZOffset, SelectionCircle] -- UNVERIFIED
    pCSelectable__Function func105;  // slot 105: FUN_6f6376a0 (+0x1a4, size 71) -- UNVERIFIED
    pCSelectable__Function func106;  // slot 106: FUN_6f637670 (+0x1a8, size 41) -- UNVERIFIED
};

// Compile-time check: 107 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CSelectable_VTable) == 428, "CSelectable_VTable must be 107 slots");
#endif

#endif // WARCRAF_CSELECTABLE_VTABLE_H
