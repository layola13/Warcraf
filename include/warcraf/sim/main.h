// main.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/main.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_SIM_MAIN_H
#define WARCRAF_SIM_MAIN_H
// basic stuff
#include "warcraf/sim/CharArray.h"
#include "warcraf/sim/StringArray.h"
#include "warcraf/math/Vector2.h"
#include "warcraf/math/Vector3.h"

// advanced stuff
#include "warcraf/sim/AbilDataCacheNode.h"
#include "warcraf/sim/AbilityLevelData.h"
#include "warcraf/sim/AbilityUIDef.h"

// advanced game stuff
#include "warcraf/sim/CAgent.h"
#include "warcraf/sim/CAgentWar3.h" // : CAgent

#include "warcraf/sim/CWar3Image.h" // : CAgentWar3
#include "warcraf/sim/CWidget.h" // : CWar3Image
#include "warcraf/sim/CUnit.h" // : CWidget

#include "warcraf/sim/CUnitRefList.h" // : CAgentWar3
#include "warcraf/sim/CAbility.h" // : CUnitRefList

#endif // WARCRAF_SIM_MAIN_H
