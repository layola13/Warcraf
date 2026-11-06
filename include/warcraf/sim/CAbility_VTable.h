// CAbility_VTable.h -- CAbility virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6f9957ec; real slot count: 189 (slots 0-188).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CABILITY_VTABLE_H
#define WARCRAF_CABILITY_VTABLE_H

#include "warcraf/sim/CUnitRefList_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCAbility__Function)(void* self);

// Notable overrides in THIS class (slots inherited from the parent
// vtable, where this class supplies its own function):
//   slot 13: FUN_6f3e9910: thunk to the 1-byte base stub FUN_6f038610 (vfunc_decomp.txt)
struct CAbility_VTable { /* size = 189 */
    CUnitRefList_VTable unitRefList;
    pCAbility__Function func32;  // slot 32: FUN_6f3fd640 (+0x080, size 76) -- UNVERIFIED
    pCAbility__Function func33;  // slot 33: FUN_6f3fd620 (+0x084, size 29) -- UNVERIFIED
    pCAbility__Function func34;  // slot 34: FUN_6f414450 (+0x088, size 3) -- UNVERIFIED
    pCAbility__Function func35;  // slot 35: FUN_6f4150d0 (+0x08c, size 3) -- UNVERIFIED
    pCAbility__Function func36;  // slot 36: FUN_6f3fa570 (+0x090, size 3) -- UNVERIFIED
    pCAbility__Function func37;  // slot 37: FUN_6f414bf0 (+0x094, size 1) -- UNVERIFIED
    pCAbility__Function func38;  // slot 38: FUN_6f3dedf0 (+0x098, size 8) -- UNVERIFIED
    pCAbility__Function func39;  // slot 39: FUN_6f3ec8c0 (+0x09c, size 3) -- UNVERIFIED
    pCAbility__Function func40;  // slot 40: FUN_6f3f68c0 (+0x0a0, size 3) -- UNVERIFIED
    pCAbility__Function func41;  // slot 41: FUN_6f3d7530 (+0x0a4, size 64) -- UNVERIFIED
    pCAbility__Function func42;  // slot 42: FUN_6f4124e0 (+0x0a8, size 84) -- UNVERIFIED
    pCAbility__Function func43;  // slot 43: FUN_6f3ee450 (+0x0ac, size 3) -- UNVERIFIED
    pCAbility__Function func44;  // slot 44: FUN_6f3ee440 (+0x0b0, size 3) -- UNVERIFIED
    pCAbility__Function func45;  // slot 45: FUN_6f4187c0 (+0x0b4, size 3) -- UNVERIFIED
    pCAbility__Function func46;  // slot 46: FUN_6f4187b0 (+0x0b8, size 3) -- UNVERIFIED
    pCAbility__Function func47;  // slot 47: FUN_6f409ed0 (+0x0bc, size 34) -- UNVERIFIED
    pCAbility__Function func48;  // slot 48: FUN_6f41d9a0 (+0x0c0, size 34) -- UNVERIFIED
    pCAbility__Function func49;  // slot 49: FUN_6f3e9f70 (+0x0c4, size 1) -- UNVERIFIED
    pCAbility__Function func50;  // slot 50: FUN_6f3ee390 (+0x0c8, size 1) -- UNVERIFIED
    pCAbility__Function func51;  // slot 51: FUN_6f3dcd40 (+0x0cc, size 6) -- UNVERIFIED
    pCAbility__Function func52;  // slot 52: FUN_6f3dcdd0 (+0x0d0, size 86) -- UNVERIFIED
    pCAbility__Function func53;  // slot 53: FUN_6f3dcdb0 (+0x0d4, size 6) -- UNVERIFIED
    pCAbility__Function func54;  // slot 54: FUN_6f3fd2c0 (+0x0d8, size 3) -- UNVERIFIED
    pCAbility__Function func55;  // slot 55: FUN_6f3fd2e0 (+0x0dc, size 3) -- UNVERIFIED
    pCAbility__Function func56;  // slot 56: FUN_6f3e9bb0 (+0x0e0, size 1) -- UNVERIFIED
    pCAbility__Function func57;  // slot 57: FUN_6f3ede90 (+0x0e4, size 1) -- UNVERIFIED
    pCAbility__Function func58;  // slot 58: FUN_6f409e40 (+0x0e8, size 1) -- UNVERIFIED
    pCAbility__Function func59;  // slot 59: FUN_6f41d8f0 (+0x0ec, size 1) -- UNVERIFIED
    pCAbility__Function func60;  // slot 60: FUN_6f3fc730 (+0x0f0, size 3) -- UNVERIFIED
    pCAbility__Function func61;  // slot 61: FUN_6f414b30 (+0x0f4, size 1) -- UNVERIFIED
    pCAbility__Function func62;  // slot 62: FUN_6f419c40 (+0x0f8, size 3) -- UNVERIFIED
    pCAbility__Function func63;  // slot 63: FUN_6f3d7fc0 (+0x0fc, size 5) -- UNVERIFIED
    pCAbility__Function func64;  // slot 64: FUN_6f40ee20 (+0x100, size 3) -- UNVERIFIED
    pCAbility__Function func65;  // slot 65: FUN_6f418490 (+0x104, size 6) -- UNVERIFIED
    pCAbility__Function func66;  // slot 66: FUN_6f3d7fb0 (+0x108, size 5) -- UNVERIFIED
    pCAbility__Function func67;  // slot 67: FUN_6f3f1400 (+0x10c, size 6) -- UNVERIFIED
    pCAbility__Function func68;  // slot 68: FUN_6f3d7d90 (+0x110, size 5) -- UNVERIFIED
    pCAbility__Function func69;  // slot 69: FUN_6f3f1410 (+0x114, size 6) -- UNVERIFIED
    pCAbility__Function func70;  // slot 70: FUN_6f3d7da0 (+0x118, size 5) -- UNVERIFIED
    pCAbility__Function func71;  // slot 71: FUN_6f400790 (+0x11c, size 18) -- UNVERIFIED
    pCAbility__Function func72;  // slot 72: FUN_6f404d80 (+0x120, size 18) -- UNVERIFIED
    pCAbility__Function func73;  // slot 73: FUN_6f404d70 (+0x124, size 3) -- UNVERIFIED
    pCAbility__Function func74;  // slot 74: FUN_6f3eee50 (+0x128, size 3) -- UNVERIFIED
    pCAbility__Function func75;  // slot 75: FUN_6f40a120 (+0x12c, size 3) -- UNVERIFIED
    pCAbility__Function func76;  // slot 76: FUN_6f405130 (+0x130, size 6) -- UNVERIFIED
    pCAbility__Function func77;  // slot 77: FUN_6f405120 (+0x134, size 5) -- UNVERIFIED
    pCAbility__Function func78;  // slot 78: FUN_6f3f0d50 (+0x138, size 6) -- UNVERIFIED
    pCAbility__Function func79;  // slot 79: FUN_6f3f0cf0 (+0x13c, size 5) -- UNVERIFIED
    pCAbility__Function func80;  // slot 80: FUN_6f3f0d00 (+0x140, size 5) -- UNVERIFIED
    pCAbility__Function func81;  // slot 81: FUN_6f3f0d20 (+0x144, size 5) -- UNVERIFIED
    pCAbility__Function func82;  // slot 82: FUN_6f3f0d10 (+0x148, size 5) -- UNVERIFIED
    pCAbility__Function func83;  // slot 83: FUN_6f3f0d30 (+0x14c, size 5) -- UNVERIFIED
    pCAbility__Function func84;  // slot 84: FUN_6f3f0d40 (+0x150, size 5) -- UNVERIFIED
    pCAbility__Function func85;  // slot 85: FUN_6f3f0ce0 (+0x154, size 5) -- UNVERIFIED
    pCAbility__Function func86;  // slot 86: FUN_6f3ef370 (+0x158, size 18) -- UNVERIFIED
    pCAbility__Function func87;  // slot 87: FUN_6f3e7980 (+0x15c, size 18) -- UNVERIFIED
    pCAbility__Function func88;  // slot 88: FUN_6f3e8290 (+0x160, size 18) -- UNVERIFIED
    pCAbility__Function func89;  // slot 89: FUN_6f3ef360 (+0x164, size 5) -- UNVERIFIED
    pCAbility__Function func90;  // slot 90: FUN_6f3e7970 (+0x168, size 5) -- UNVERIFIED
    pCAbility__Function func91;  // slot 91: FUN_6f3e8280 (+0x16c, size 5) -- UNVERIFIED
    pCAbility__Function func92;  // slot 92: FUN_6f3daa90 (+0x170, size 3) -- UNVERIFIED
    pCAbility__Function func93;  // slot 93: FUN_6f3de5c0 (+0x174, size 3) -- UNVERIFIED
    pCAbility__Function func94;  // slot 94: FUN_6f3de110 (+0x178, size 3) -- UNVERIFIED
    pCAbility__Function func95;  // slot 95: FUN_6f3dbd50 (+0x17c, size 6) -- UNVERIFIED
    pCAbility__Function func96;  // slot 96: FUN_6f3dbd60 (+0x180, size 5) -- UNVERIFIED
    pCAbility__Function func97;  // slot 97: FUN_6f418020 (+0x184, size 18) -- UNVERIFIED
    pCAbility__Function func98;  // slot 98: FUN_6f400290 (+0x188, size 3) -- UNVERIFIED
    pCAbility__Function func99;  // slot 99: FUN_6f400270 (+0x18c, size 3) -- UNVERIFIED
    pCAbility__Function func100;  // slot 100: FUN_6f3ffee0 (+0x190, size 3) -- UNVERIFIED
    pCAbility__Function func101;  // slot 101: FUN_6f3ffed0 (+0x194, size 3) -- UNVERIFIED
    pCAbility__Function func102;  // slot 102: FUN_6f3fff00 (+0x198, size 3) -- UNVERIFIED
    pCAbility__Function func103;  // slot 103: FUN_6f400170 (+0x19c, size 3) -- UNVERIFIED
    pCAbility__Function func104;  // slot 104: FUN_6f3fffe0 (+0x1a0, size 3) -- UNVERIFIED
    pCAbility__Function func105;  // slot 105: FUN_6f3d9510 (+0x1a4, size 3) -- UNVERIFIED
    pCAbility__Function func106;  // slot 106: FUN_6f40ecf0 (+0x1a8, size 3) -- UNVERIFIED
    pCAbility__Function func107;  // slot 107: FUN_6f3d8760 (+0x1ac, size 1) -- UNVERIFIED
    pCAbility__Function func108;  // slot 108: FUN_6f40c9c0 (+0x1b0, size 1) -- UNVERIFIED
    pCAbility__Function func109;  // slot 109: FUN_6f40c980 (+0x1b4, size 1) -- UNVERIFIED
    pCAbility__Function func110;  // slot 110: FUN_6f3dbfa0 (+0x1b8, size 3) -- UNVERIFIED
    pCAbility__Function func111;  // slot 111: FUN_6f41c8f0 (+0x1bc, size 24) -- UNVERIFIED
    pCAbility__Function func112;  // slot 112: FUN_6f412ce0 (+0x1c0, size 3) -- UNVERIFIED
    pCAbility__Function func113;  // slot 113: FUN_6f413d20 (+0x1c4, size 3) -- UNVERIFIED
    pCAbility__Function func114;  // slot 114: FUN_6f4126b0 (+0x1c8, size 1) -- UNVERIFIED
    pCAbility__Function func115;  // slot 115: FUN_6f417420 (+0x1cc, size 1) -- UNVERIFIED
    pCAbility__Function func116;  // slot 116: FUN_6f4143c0 (+0x1d0, size 1) -- UNVERIFIED
    pCAbility__Function func117;  // slot 117: FUN_6f414c40 (+0x1d4, size 1) -- UNVERIFIED
    pCAbility__Function func118;  // slot 118: FUN_6f3ffc80 (+0x1d8, size 3) -- UNVERIFIED
    pCAbility__Function func119;  // slot 119: FUN_6f3ffe90 (+0x1dc, size 3) -- UNVERIFIED
    pCAbility__Function func120;  // slot 120: FUN_6f3ffdd0 (+0x1e0, size 3) -- UNVERIFIED
    pCAbility__Function func121;  // slot 121: FUN_6f3fff10 (+0x1e4, size 3) -- UNVERIFIED
    pCAbility__Function func122;  // slot 122: FUN_6f3ff800 (+0x1e8, size 3) -- UNVERIFIED
    pCAbility__Function func123;  // slot 123: FUN_6f3ffe10 (+0x1ec, size 3) -- UNVERIFIED
    pCAbility__Function func124;  // slot 124: FUN_6f3ff910 (+0x1f0, size 3) -- UNVERIFIED
    pCAbility__Function func125;  // slot 125: FUN_6f3ffe50 (+0x1f4, size 3) -- UNVERIFIED
    pCAbility__Function func126;  // slot 126: FUN_6f3ffe30 (+0x1f8, size 3) -- UNVERIFIED
    pCAbility__Function func127;  // slot 127: FUN_6f3ff9f0 (+0x1fc, size 3) -- UNVERIFIED
    pCAbility__Function func128;  // slot 128: FUN_6f400280 (+0x200, size 3) -- UNVERIFIED
    pCAbility__Function func129;  // slot 129: FUN_6f3fffa0 (+0x204, size 3) -- UNVERIFIED
    pCAbility__Function func130;  // slot 130: FUN_6f3ffef0 (+0x208, size 3) -- UNVERIFIED
    pCAbility__Function func131;  // slot 131: FUN_6f400420 (+0x20c, size 17) -- UNVERIFIED
    pCAbility__Function func132;  // slot 132: FUN_6f3ff7c0 (+0x210, size 3) -- UNVERIFIED
    pCAbility__Function func133;  // slot 133: FUN_6f3ff7d0 (+0x214, size 3) -- UNVERIFIED
    pCAbility__Function func134;  // slot 134: FUN_6f3ff7e0 (+0x218, size 3) -- UNVERIFIED
    pCAbility__Function func135;  // slot 135: FUN_6f3fba10 (+0x21c, size 5) -- UNVERIFIED
    pCAbility__Function func136;  // slot 136: FUN_6f3fb560 (+0x220, size 6) -- UNVERIFIED
    pCAbility__Function func137;  // slot 137: FUN_6f3fb570 (+0x224, size 6) -- UNVERIFIED
    pCAbility__Function func138;  // slot 138: FUN_6f3fb550 (+0x228, size 6) -- UNVERIFIED
    pCAbility__Function func139;  // slot 139: FUN_6f419a60 (+0x22c, size 5) -- UNVERIFIED
    pCAbility__Function func140;  // slot 140: FUN_6f3ddd00 (+0x230, size 6) -- UNVERIFIED
    pCAbility__Function func141;  // slot 141: FUN_6f40a070 (+0x234, size 18) -- UNVERIFIED
    pCAbility__Function func142;  // slot 142: FUN_6f40a090 (+0x238, size 3) -- UNVERIFIED
    pCAbility__Function func143;  // slot 143: FUN_6f4179b0 (+0x23c, size 5) -- UNVERIFIED
    pCAbility__Function func144;  // slot 144: FUN_6f40f0a0 (+0x240, size 3) -- UNVERIFIED
    pCAbility__Function func145;  // slot 145: FUN_6f41dc60 (+0x244, size 8) -- UNVERIFIED
    pCAbility__Function func146;  // slot 146: FUN_6f4095f0 (+0x248, size 5) -- UNVERIFIED
    pCAbility__Function func147;  // slot 147: FUN_6f41dce0 (+0x24c, size 8) -- UNVERIFIED
    pCAbility__Function func148;  // slot 148: FUN_6f41dcd0 (+0x250, size 8) -- UNVERIFIED
    pCAbility__Function func149;  // slot 149: FUN_6f41dc90 (+0x254, size 8) -- UNVERIFIED
    pCAbility__Function func150;  // slot 150: FUN_6f41dcb0 (+0x258, size 8) -- UNVERIFIED
    pCAbility__Function func151;  // slot 151: FUN_6f41dc80 (+0x25c, size 8) -- UNVERIFIED
    pCAbility__Function func152;  // slot 152: FUN_6f41dcc0 (+0x260, size 8) -- UNVERIFIED
    pCAbility__Function func153;  // slot 153: FUN_6f41dca0 (+0x264, size 8) -- UNVERIFIED
    pCAbility__Function func154;  // slot 154: FUN_6f3fc090 (+0x268, size 289) strings=[DepCheckAlias, Misc] -- UNVERIFIED
    pCAbility__Function func155;  // slot 155: FUN_6f3ded10 (+0x26c, size 5) -- UNVERIFIED
    pCAbility__Function func156;  // slot 156: FUN_6f404af0 (+0x270, size 5) -- UNVERIFIED
    pCAbility__Function func157;  // slot 157: FUN_6f3d7f70 (+0x274, size 3) -- UNVERIFIED
    pCAbility__Function func158;  // slot 158: FUN_6f3d7db0 (+0x278, size 3) -- UNVERIFIED
    pCAbility__Function func159;  // slot 159: FUN_6f3e7ff0 (+0x27c, size 3) -- UNVERIFIED
    pCAbility__Function func160;  // slot 160: FUN_6f4006c0 (+0x280, size 3) -- UNVERIFIED
    pCAbility__Function func161;  // slot 161: FUN_6f400680 (+0x284, size 1) -- UNVERIFIED
    pCAbility__Function func162;  // slot 162: FUN_6f3e7f20 (+0x288, size 1) -- UNVERIFIED
    pCAbility__Function func163;  // slot 163: FUN_6f3dbed0 (+0x28c, size 10) -- UNVERIFIED
    pCAbility__Function func164;  // slot 164: FUN_6f3ee430 (+0x290, size 14) -- UNVERIFIED
    pCAbility__Function func165;  // slot 165: FUN_6f3d84b0 (+0x294, size 127) -- UNVERIFIED
    pCAbility__Function func166;  // slot 166: FUN_6f415460 (+0x298, size 3) -- UNVERIFIED
    pCAbility__Function getEffectArt;  // slot 167: FUN_6f3f7980 (+0x29c, size 69) strings=[CAbility::GetEffectArt] -- CONFIRMED: string "CAbility::GetEffectArt" (vfunc_decomp.txt); reads global table +0xd4 count / +0xd8 array, clamps index (S3)
    pCAbility__Function getAreaEffectArt;  // slot 168: FUN_6f3f69b0 (+0x2a0, size 69) strings=[CAbility::GetAreaEffectArt] -- CONFIRMED: string "CAbility::GetAreaEffectArt" (vfunc_decomp.txt)
    pCAbility__Function getTargetArt;  // slot 169: FUN_6f3fa640 (+0x2a4, size 69) strings=[CAbility::GetTargetArt] -- CONFIRMED: string "CAbility::GetTargetArt" (vfunc_decomp.txt)
    pCAbility__Function getEffectSound;  // slot 170: FUN_6f3f7a40 (+0x2a8, size 30) strings=[CAbility::GetEffectSound] -- CONFIRMED: string "CAbility::GetEffectSound" (vfunc_decomp.txt)
    pCAbility__Function getEffectSoundLooped;  // slot 171: FUN_6f3f7a70 (+0x2ac, size 30) strings=[CAbility::GetEffectSoundLooped] -- CONFIRMED: string "CAbility::GetEffectSoundLooped" (vfunc_decomp.txt)
    pCAbility__Function func172;  // slot 172: FUN_6f3f94a0 (+0x2b0, size 8) -- UNVERIFIED
    pCAbility__Function func173;  // slot 173: FUN_6f3f9540 (+0x2b4, size 45) strings=[PlacementCursor, ReplaceableTextures\Selection\SpellAreaOfEffect.blp] -- UNVERIFIED
    pCAbility__Function func174;  // slot 174: FUN_6f3fa550 (+0x2b8, size 18) -- UNVERIFIED
    pCAbility__Function func175;  // slot 175: FUN_6f3f94c0 (+0x2bc, size 23) -- UNVERIFIED
    pCAbility__Function func176;  // slot 176: FUN_6f3f9480 (+0x2c0, size 30) strings=[TargetingColors, TargetLightColor] -- UNVERIFIED
    pCAbility__Function getMissileArc;  // slot 177: FUN_6f3f8840 (+0x2c4, size 53) strings=[CAbility::GetMissileArc] -- CONFIRMED: string "CAbility::GetMissileArc" (vfunc_decomp.txt)
    pCAbility__Function getMissileSpeed;  // slot 178: FUN_6f3f8980 (+0x2c8, size 53) strings=[CAbility::GetMissileSpeed] -- CONFIRMED: string "CAbility::GetMissileSpeed" (vfunc_decomp.txt)
    pCAbility__Function getMissileHoming;  // slot 179: FUN_6f3f8910 (+0x2cc, size 30) strings=[CAbility::GetMissileHoming] -- CONFIRMED: string "CAbility::GetMissileHoming" (vfunc_decomp.txt)
    pCAbility__Function func180;  // slot 180: FUN_6f3fae20 (+0x2d0, size 3) -- UNVERIFIED
    pCAbility__Function func181;  // slot 181: FUN_6f3fa600 (+0x2d4, size 3) -- UNVERIFIED
    pCAbility__Function func182;  // slot 182: FUN_6f3fad50 (+0x2d8, size 3) -- UNVERIFIED
    pCAbility__Function func183;  // slot 183: FUN_6f3f71d0 (+0x2dc, size 18) -- UNVERIFIED
    pCAbility__Function func184;  // slot 184: FUN_6f3f7030 (+0x2e0, size 18) -- UNVERIFIED
    pCAbility__Function func185;  // slot 185: FUN_6f3fd550 (+0x2e4, size 19) -- UNVERIFIED
    pCAbility__Function func186;  // slot 186: FUN_6f3e7f30 (+0x2e8, size 19) -- UNVERIFIED
    pCAbility__Function func187;  // slot 187: FUN_6f3f6fd0 (+0x2ec, size 89) -- UNVERIFIED
    pCAbility__Function func188;  // slot 188: FUN_6f40a500 (+0x2f0, size 1) -- UNVERIFIED
};

// Compile-time check: 189 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CAbility_VTable) == 756, "CAbility_VTable must be 189 slots");
#endif

#endif // WARCRAF_CABILITY_VTABLE_H
