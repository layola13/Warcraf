// CAbility.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CAbility.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_CABILITY_H
#define WARCRAF_SIM_CABILITY_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CAbility_VTable.h"  // ADDED: this class's own (new, corrected) vtable header; not in the TinkerWorX original.
#include "warcraf/sim/CUnitRefList.h"

#include "warcraf/sim/CUnit.h"
#include "warcraf/sim/AbilDataCacheNode.h"

class CAbility : CUnitRefList { /* size = 108 (32+4+12+60) */
public:
    CUnit* owner; // The CUnit that has the ability.
    int abilityId; // The ability id.
    _DWORD field_38;
    _DWORD field_3C;
    _DWORD field_40;
    _DWORD field_44;
    _DWORD field_48;
    _DWORD field_4C;
    _DWORD field_50;
    AbilDataCacheNode* data; // The ability data cache.
    _DWORD field_58;
    _DWORD field_5C;
    _DWORD field_60;
    _DWORD field_64;
    _DWORD field_68;
};

// Compile-time check: verified total size 108 bytes (0x6c); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAbility) == 108, "CAbility must be 108 bytes");
#endif

#endif // WARCRAF_SIM_CABILITY_H
