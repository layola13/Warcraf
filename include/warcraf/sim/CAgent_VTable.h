// CAgent_VTable.h -- CAgent virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6f950bd0; real slot count: 24 (slots 0-23).
// CORRECTION vs TinkerWorX: slot 1 renamed deletingDestructor on behavior
// evidence; slots 7/22 confirmed getClassId/getClassName by decompile.
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CAGENT_VTABLE_H
#define WARCRAF_CAGENT_VTABLE_H

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCAgent__Function)(void* self);

typedef int (__thiscall* pCAgent__GetClassId)(void* agent);
typedef char* (__thiscall* pCAgent__GetClassName)(void* agent);

struct CAgent_VTable { /* size = 24 */
    pCAgent__Function func0;  // slot 0: FUN_6f0218c0 (+0x000, size 26) -- identity UNVERIFIED; body calls slot-7 getClassId then dispatches via FUN_6f0219f0 registry -- likely event/observer dispatch (vfunc_decomp.txt)
    pCAgent__Function deletingDestructor;  // slot 1: FUN_6f021630 (+0x004, size 117) strings=[delete] -- BEHAVIOR-DERIVED name: body references "delete" string; MSVC scalar-deleting-destructor slot by convention; NOT a recovered symbol (vfunc_decomp.txt)
    pCAgent__Function func2;  // slot 2: FUN_6f056af0 (+0x008, size 354) strings=[.?AUObserverEventReg@@] -- UNVERIFIED
    pCAgent__Function func3;  // slot 3: FUN_6f021bb0 (+0x00c, size 5) -- UNVERIFIED
    pCAgent__Function func4;  // slot 4: FUN_6f0562e0 (+0x010, size 19) -- UNVERIFIED
    pCAgent__Function func5;  // slot 5: FUN_6f056300 (+0x014, size 56) -- UNVERIFIED
    pCAgent__Function func6;  // slot 6: FUN_6f038eb0 (+0x018, size 319) -- UNVERIFIED
    pCAgent__GetClassId getClassId;  // slot 7: FUN_6f038b90 (+0x01c, size 6) -- CONFIRMED: returns class-id constant 0x2b616761 (FUN_6f038b90, vfunc_decomp.txt)
    pCAgent__Function func8;  // slot 8: FUN_6f0385c0 (+0x020, size 5) -- UNVERIFIED
    pCAgent__Function func9;  // slot 9: FUN_6f0384d0 (+0x024, size 17) -- UNVERIFIED
    pCAgent__Function func10;  // slot 10: FUN_6f0384f0 (+0x028, size 5) -- UNVERIFIED
    pCAgent__Function func11;  // slot 11: FUN_6f038bb0 (+0x02c, size 3) -- UNVERIFIED
    pCAgent__Function func12;  // slot 12: FUN_6f038ba0 (+0x030, size 3) -- UNVERIFIED
    pCAgent__Function func13;  // slot 13: FUN_6f038610 (+0x034, size 1) -- UNVERIFIED
    pCAgent__Function func14;  // slot 14: FUN_6f0390d0 (+0x038, size 279) -- UNVERIFIED
    pCAgent__Function func15;  // slot 15: FUN_6f038c60 (+0x03c, size 310) -- UNVERIFIED
    pCAgent__Function func16;  // slot 16: FUN_6f021bc0 (+0x040, size 1) -- UNVERIFIED
    pCAgent__Function func17;  // slot 17: FUN_6f0217a0 (+0x044, size 3) -- UNVERIFIED
    pCAgent__Function func18;  // slot 18: FUN_6f021ce0 (+0x048, size 3) -- UNVERIFIED
    pCAgent__Function func19;  // slot 19: FUN_6f021b20 (+0x04c, size 3) -- UNVERIFIED
    pCAgent__Function func20;  // slot 20: FUN_6f039300 (+0x050, size 107) -- UNVERIFIED
    pCAgent__Function func21;  // slot 21: FUN_6f038da0 (+0x054, size 171) -- UNVERIFIED
    pCAgent__GetClassName getClassName;  // slot 22: FUN_6f0219b0 (+0x058, size 6) strings=[CAgent] -- CONFIRMED: returns "CAgent" literally (vfunc_decomp.txt)
    pCAgent__Function func23;  // slot 23: FUN_6f0385d0 (+0x05c, size 36) -- UNVERIFIED identity; CAgent body is a class-id-gated update (*(obj+0xc) == 0x2b61676c), touches DAT_6fbb81d4 (vfunc_decomp.txt)
};

// Compile-time check: 24 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAgent_VTable) == 96, "CAgent_VTable must be 24 slots");
#endif

#endif // WARCRAF_CAGENT_VTABLE_H
