// CUnitRefList_VTable.h -- CUnitRefList virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6f995758; real slot count: 32 (slots 0-31).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CUNITREFLIST_VTABLE_H
#define WARCRAF_CUNITREFLIST_VTABLE_H

#include "warcraf/sim/CAgentWar3_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCUnitRefList__Function)(void* self);

struct CUnitRefList_VTable { /* size = 32 */
    CAgentWar3_VTable agentWar3;
    pCUnitRefList__Function func30;  // slot 30: FUN_6f3ff280 (+0x078, size 88) -- UNVERIFIED
    pCUnitRefList__Function func31;  // slot 31: FUN_6f3f9570 (+0x07c, size 15) -- UNVERIFIED
};

// Compile-time check: 32 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CUnitRefList_VTable) == 128, "CUnitRefList_VTable must be 32 slots");
#endif

#endif // WARCRAF_CUNITREFLIST_VTABLE_H
