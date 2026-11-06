// CAgentWar3_VTable.h -- CAgentWar3 virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6f950c4c; real slot count: 30 (slots 0-29).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CAGENTWAR3_VTABLE_H
#define WARCRAF_CAGENTWAR3_VTABLE_H

#include "warcraf/sim/CAgent_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCAgentWar3__Function)(void* self);

struct CAgentWar3_VTable { /* size = 30 */
    CAgent_VTable agent;
    pCAgentWar3__Function func24;  // slot 24: FUN_6f021a90 (+0x060, size 13) -- UNVERIFIED
    pCAgentWar3__Function func25;  // slot 25: FUN_6f0219d0 (+0x064, size 4) -- UNVERIFIED
    pCAgentWar3__Function func26;  // slot 26: FUN_6f021760 (+0x068, size 45) strings=[.?AVCAgentFieldMini@@] -- UNVERIFIED
    pCAgentWar3__Function func27;  // slot 27: FUN_6f021740 (+0x06c, size 11) -- UNVERIFIED
    pCAgentWar3__Function func28;  // slot 28: FUN_6f0217e0 (+0x070, size 29) strings=[.?AVCAgentFieldMini@@] -- UNVERIFIED
    pCAgentWar3__Function func29;  // slot 29: FUN_6f0217c0 (+0x074, size 3) -- UNVERIFIED
};

// Compile-time check: 30 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAgentWar3_VTable) == 120, "CAgentWar3_VTable must be 30 slots");
#endif

#endif // WARCRAF_CAGENTWAR3_VTABLE_H
