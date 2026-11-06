// CAgentWar3.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CAgentWar3.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_CAGENTWAR3_H
#define WARCRAF_SIM_CAGENTWAR3_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CAgentWar3_VTable.h"  // ADDED: this class's own (new, corrected) vtable header; not in the TinkerWorX original.
#include "warcraf/sim/CAgent.h"

class CAgentWar3 : CAgent { /* size = 36 (32+4) */
public:
    _DWORD field_20;
};

// Compile-time check: verified total size 36 bytes (0x24); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAgentWar3) == 36, "CAgentWar3 must be 36 bytes");
#endif

#endif // WARCRAF_SIM_CAGENTWAR3_H
