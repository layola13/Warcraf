// AbilDataCacheNode.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/AbilDataCacheNode.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_ABILDATACACHENODE_H
#define WARCRAF_SIM_ABILDATACACHENODE_H

#include "warcraf/core/Types.h"
#include "warcraf/sim/AbilityUIDef.h"
#include "warcraf/sim/AbilityLevelData.h"

class AbilDataCacheNode { /* size = unknown */
public:
    _DWORD field_0;
    _DWORD field_4;
    _DWORD field_8;
    _DWORD field_C;
    _DWORD field_10;
    _DWORD abilityId_0;
    _DWORD abilityId_1;
    _DWORD field_1C;
    _DWORD field_20;
    _DWORD field_24;
    AbilityUIDef* uiDef;
    int hasUiDef;
    int code; // the ability it is based on
    int alias; // the custom ability id from the editor
    int checkDependencies;
    int levels_0;
    int requiredLevel;
    int levelSkip;
    int priority;
    _DWORD field_4C;
    int levels_1;
    AbilityLevelData* level; // array[levels_1]
};

#endif // WARCRAF_SIM_ABILDATACACHENODE_H
