// AbilityLevelData.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/AbilityLevelData.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_ABILITYLEVELDATA_H
#define WARCRAF_SIM_ABILITYLEVELDATA_H

#include "warcraf/core/Types.h"
class AbilityLevelData { /* size = unknown */
public:
    int statsTargetsAllowed;
    float statsCastingTime;
    float statsDurationNormal;
    float statsDurationHero;
    int statsManaCost;
    float statsCooldown;
    float statsAreaOfEffect;
    float statsCastRange;
    _DWORD dataA;
    _DWORD dataB;
    _DWORD dataC;
    _DWORD dataD;
    _DWORD dataE;
    _DWORD dataF;
    _DWORD dataG;
    _DWORD dataH;
    _DWORD dataI;
    int unitId;
    _DWORD field_48;
    _DWORD field_4C;
    _DWORD field_50;
    int buffId;
    _DWORD field_58;
    _DWORD field_5C;
    _DWORD field_60;
    _DWORD field_64;
};

#endif // WARCRAF_SIM_ABILITYLEVELDATA_H
