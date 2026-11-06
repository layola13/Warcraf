// CSelectable.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CSelectable.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_CSELECTABLE_H
#define WARCRAF_SIM_CSELECTABLE_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CSelectable_VTable.h"  // ADDED: this class's own (new, corrected) vtable header; not in the TinkerWorX original.
#include "warcraf/sim/CWidget.h"

class CSelectable : CWidget { /* size = 84 (32+4+8+24) */
public:
    _DWORD field_44;
    _DWORD field_48;
    _DWORD field_4C;
    _DWORD field_50;
};

// Compile-time check: verified total size 84 bytes (0x54); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CSelectable) == 84, "CSelectable must be 84 bytes");
#endif

#endif // WARCRAF_SIM_CSELECTABLE_H
