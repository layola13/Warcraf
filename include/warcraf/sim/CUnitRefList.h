// CUnitRefList.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CUnitRefList.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_CUNITREFLIST_H
#define WARCRAF_SIM_CUNITREFLIST_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CUnitRefList_VTable.h"  // ADDED: this class's own (new, corrected) vtable header; not in the TinkerWorX original.
#include "warcraf/sim/CAgentWar3.h"

class CUnitRefList : CAgentWar3 { /* size = 48 (32+4+12) */
public:
    _DWORD field_24;
    _DWORD field_28;
    _DWORD field_2C;
};

// Compile-time check: verified total size 48 bytes (0x30); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CUnitRefList) == 48, "CUnitRefList must be 48 bytes");
#endif

#endif // WARCRAF_SIM_CUNITREFLIST_H
