// CAgent.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/CAgent.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field-offset spot-check vs 1.27a evidence (S3): field_C (+0x0c, type-id check), field_14/field_18 (flag bytes +0x17/+0x19 lie inside them) -- confirmed present; NO field offsets found wrong in this file.

#ifndef WARCRAF_SIM_CAGENT_H
#define WARCRAF_SIM_CAGENT_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/CAgent_VTable.h"
#include "warcraf/sim/CAgentWar3_VTable.h"
#include "warcraf/sim/CWar3Image_VTable.h"
#include "warcraf/sim/CWidget_VTable.h"
#include "warcraf/sim/CSelectable_VTable.h"
#include "warcraf/sim/CUnit_VTable.h"
#include "warcraf/sim/CUnitRefList_VTable.h"
#include "warcraf/sim/CAbility_VTable.h"

class CAgent { /* size = 32 */
public:
    union {
      CAgent_VTable* cAgent_vtable;
      CWidget_VTable* cWidget_vtable;
      CUnit_VTable* cUnit_vtable;
      // ADDED: views of the same pointer through the new (corrected)
      // vtable headers; not in the TinkerWorX original.
      CAgentWar3_VTable* cAgentWar3_vtable;
      CWar3Image_VTable* cWar3Image_vtable;
      CSelectable_VTable* cSelectable_vtable;
      CUnitRefList_VTable* cUnitRefList_vtable;
      CAbility_VTable* cAbility_vtable;
    };
    _DWORD field_4;
    _DWORD field_8;
    int field_C;
    int field_10;
    _DWORD field_14;
    _DWORD field_18;
    _DWORD field_1C;
};

// Compile-time check: verified total size 32 bytes (0x20); 32-bit target only (vtable/pointer members).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAgent) == 32, "CAgent must be 32 bytes");
#endif

#endif // WARCRAF_SIM_CAGENT_H
