// CWar3Image_VTable.h -- CWar3Image virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6f970694; real slot count: 39 (slots 0-38).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CWAR3IMAGE_VTABLE_H
#define WARCRAF_CWAR3IMAGE_VTABLE_H

#include "warcraf/sim/CAgentWar3_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCWar3Image__Function)(void* self);

struct CWar3Image_VTable { /* size = 39 */
    CAgentWar3_VTable agentWar3;
    pCWar3Image__Function func30;  // slot 30: FUN_6f6a0d80 (+0x078, size 468) -- UNVERIFIED
    pCWar3Image__Function func31;  // slot 31: FUN_6f6a0f60 (+0x07c, size 186) -- UNVERIFIED
    pCWar3Image__Function func32;  // slot 32: FUN_6f6a1250 (+0x080, size 24) -- UNVERIFIED
    pCWar3Image__Function func33;  // slot 33: FUN_6f2197b0 (+0x084, size 5) -- UNVERIFIED
    pCWar3Image__Function func34;  // slot 34: FUN_6f6a0b10 (+0x088, size 142) -- UNVERIFIED
    pCWar3Image__Function func35;  // slot 35: FUN_6f6a11c0 (+0x08c, size 41) -- UNVERIFIED
    pCWar3Image__Function func36;  // slot 36: FUN_6f6a1120 (+0x090, size 45) -- UNVERIFIED
    pCWar3Image__Function func37;  // slot 37: FUN_6f6a1090 (+0x094, size 54) -- UNVERIFIED
    pCWar3Image__Function func38;  // slot 38: FUN_6f2227b0 (+0x098, size 1) -- UNVERIFIED
};

// Compile-time check: 39 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CWar3Image_VTable) == 156, "CWar3Image_VTable must be 39 slots");
#endif

#endif // WARCRAF_CWAR3IMAGE_VTABLE_H
