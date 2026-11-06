// CWidget.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CWidget.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_CWIDGET_H
#define WARCRAF_SIM_CWIDGET_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CWar3Image.h"

class CWidget : CWar3Image { /* size = 68 (32+4+8+24) */
public:
    _DWORD field_2C;
    _DWORD field_30;
    _DWORD field_34;
    _DWORD field_38;
    _DWORD field_3C;
    _DWORD field_40;
};

// Compile-time check: verified total size 68 bytes (0x44); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CWidget) == 68, "CWidget must be 68 bytes");
#endif

#endif // WARCRAF_SIM_CWIDGET_H
