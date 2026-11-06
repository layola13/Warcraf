// Warcraf.h -- umbrella header: all Warcraft III 1.27a engine headers.
//
// Includes every header in the project, in module order. See README.md for
// the module map, the evidence behind each header, and what is verified,
// stub, or still unverified.

#ifndef WARCRAF_WARCRAF_H
#define WARCRAF_WARCRAF_H

// core -- base types, Storm system library, RNG
#include "warcraf/core/Types.h"
#include "warcraf/core/Storm.h"
#include "warcraf/core/CRng.h"

// math -- vectors (TinkerWorX copies)
#include "warcraf/math/Vector2.h"
#include "warcraf/math/Vector3.h"

// net -- session state, router, commands, Battle.net
#include "warcraf/net/CNetData.h"
#include "warcraf/net/NetRouter.h"
#include "warcraf/net/CNetCommand.h"
#include "warcraf/net/CBattleNet.h"

// io -- MPQ file layer, replay
#include "warcraf/io/SFile.h"
#include "warcraf/io/CReplay.h"

// engine -- GameMain, engine loop, 25 ms step
#include "warcraf/engine/CMainLoop.h"

// script -- JASS2 VM, native registry, triggers
#include "warcraf/script/Jass.h"

// sim -- simulation classes (TinkerWorX copies + corrected vtables)
#include "warcraf/sim/CharArray.h"
#include "warcraf/sim/StringArray.h"
#include "warcraf/sim/AbilityUIDef.h"
#include "warcraf/sim/AbilityLevelData.h"
#include "warcraf/sim/AbilDataCacheNode.h"
#include "warcraf/sim/CAgent_VTable.h"
#include "warcraf/sim/CAgentWar3_VTable.h"
#include "warcraf/sim/CWar3Image_VTable.h"
#include "warcraf/sim/CWidget_VTable.h"
#include "warcraf/sim/CSelectable_VTable.h"
#include "warcraf/sim/CUnit_VTable.h"
#include "warcraf/sim/CUnitRefList_VTable.h"
#include "warcraf/sim/CAbility_VTable.h"
#include "warcraf/sim/CAgent.h"
#include "warcraf/sim/CAgentWar3.h"
#include "warcraf/sim/CWar3Image.h"
#include "warcraf/sim/CWidget.h"
#include "warcraf/sim/CSelectable.h"
#include "warcraf/sim/CUnit.h"
#include "warcraf/sim/CUnitRefList.h"
#include "warcraf/sim/CAbility.h"
#include "warcraf/sim/main.h"

// render -- Gx renderer abstraction
#include "warcraf/render/CGx.h"

// ui -- Frame/UI system
#include "warcraf/ui/CFrame.h"

// audio -- Miles Sound System
#include "warcraf/audio/CMiles.h"

#endif // WARCRAF_WARCRAF_H
