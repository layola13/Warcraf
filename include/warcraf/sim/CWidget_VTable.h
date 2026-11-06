// CWidget_VTable.h -- CWidget virtual function table, Warcraft III 1.27a.
//
// CORRECTED against 1.27a (Ghidra RTTI -> Complete Object Locator ->
// vtable; dump: ~/workspace/warcraf_ghidra/vtable_dump.txt).
// Vtable address: 0x6fa49f4c; real slot count: 96 (slots 0-95).
// CORRECTION vs TinkerWorX: the original header claims size 76 but lists
// 77 entries (0-76); the real 1.27a vtable has 96 slots -- slots 77-95
// were missing. Slots 72-74 renamed on behavior evidence (see below).
// Slot comments give the real 1.27a function address (+0xNNN byte offset),
// size, and referenced strings from the dump. Renamed slots use the generic
// p<Class>__Function typedef (no invented signatures), except getClassId /
// getClassName / setLife, which keep the TinkerWorX typedefs. Unrenamed
// slots keep funcN and are marked UNVERIFIED -- do not guess their names.

#ifndef WARCRAF_CWIDGET_VTABLE_H
#define WARCRAF_CWIDGET_VTABLE_H

#include "warcraf/sim/CAgent_VTable.h"

#include "warcraf/core/Types.h"

typedef _DWORD (__thiscall* pCWidget__Function)(void* self);

typedef void (__thiscall* pCWidget__SetLife)(void* widget, float* life);

struct CWidget_VTable { /* size = 96 */
    CAgent_VTable agent;
    pCWidget__Function func24;  // slot 24: FUN_6f021a90 (+0x060, size 13) -- UNVERIFIED
    pCWidget__Function func25;  // slot 25: FUN_6f0219d0 (+0x064, size 4) -- UNVERIFIED
    pCWidget__Function func26;  // slot 26: FUN_6f021760 (+0x068, size 45) -- UNVERIFIED
    pCWidget__Function func27;  // slot 27: FUN_6f021740 (+0x06c, size 11) -- UNVERIFIED
    pCWidget__Function func28;  // slot 28: FUN_6f0217e0 (+0x070, size 29) -- UNVERIFIED
    pCWidget__Function func29;  // slot 29: FUN_6f0217c0 (+0x074, size 3) -- UNVERIFIED
    pCWidget__Function func30;  // slot 30: FUN_6f6a0d80 (+0x078, size 468) -- UNVERIFIED
    pCWidget__Function func31;  // slot 31: FUN_6f6a0f60 (+0x07c, size 186) -- UNVERIFIED
    pCWidget__Function func32;  // slot 32: FUN_6f6a1250 (+0x080, size 24) -- UNVERIFIED
    pCWidget__Function func33;  // slot 33: FUN_6f2197b0 (+0x084, size 5) -- UNVERIFIED
    pCWidget__Function func34;  // slot 34: FUN_6f6a0b10 (+0x088, size 142) -- UNVERIFIED
    pCWidget__Function func35;  // slot 35: FUN_6f6a11c0 (+0x08c, size 41) -- UNVERIFIED
    pCWidget__Function func36;  // slot 36: FUN_6f6a1120 (+0x090, size 45) -- UNVERIFIED
    pCWidget__Function func37;  // slot 37: FUN_6f635780 (+0x094, size 49) -- UNVERIFIED
    pCWidget__Function func38;  // slot 38: FUN_6f6356e0 (+0x098, size 151) -- UNVERIFIED
    pCWidget__Function func39;  // slot 39: FUN_6f634ef0 (+0x09c, size 552) -- UNVERIFIED
    pCWidget__Function func40;  // slot 40: FUN_6f635dc0 (+0x0a0, size 25) -- UNVERIFIED
    pCWidget__Function func41;  // slot 41: FUN_6f6360b0 (+0x0a4, size 25) -- UNVERIFIED
    pCWidget__Function func42;  // slot 42: FUN_6f471a20 (+0x0a8, size 25) -- UNVERIFIED
    pCWidget__Function func43;  // slot 43: FUN_6f6356d0 (+0x0ac, size 6) -- UNVERIFIED
    pCWidget__Function func44;  // slot 44: FUN_6f6367e0 (+0x0b0, size 6) -- UNVERIFIED
    pCWidget__Function func45;  // slot 45: FUN_6f6367d0 (+0x0b4, size 8) -- UNVERIFIED
    pCWidget__Function func46;  // slot 46: FUN_6f634e90 (+0x0b8, size 3) -- UNVERIFIED
    pCWidget__Function func47;  // slot 47: FUN_6f634e50 (+0x0bc, size 7) -- UNVERIFIED
    pCWidget__Function func48;  // slot 48: FUN_6f6360a0 (+0x0c0, size 3) -- UNVERIFIED
    pCWidget__Function func49;  // slot 49: FUN_6f636080 (+0x0c4, size 3) -- UNVERIFIED
    pCWidget__Function func50;  // slot 50: FUN_6f636050 (+0x0c8, size 3) -- UNVERIFIED
    pCWidget__Function func51;  // slot 51: FUN_6f636010 (+0x0cc, size 3) -- UNVERIFIED
    pCWidget__Function func52;  // slot 52: FUN_6f633e60 (+0x0d0, size 82) -- UNVERIFIED
    pCWidget__Function func53;  // slot 53: FUN_6f634c70 (+0x0d4, size 114) -- UNVERIFIED
    pCWidget__Function func54;  // slot 54: FUN_6f635de0 (+0x0d8, size 235) -- UNVERIFIED
    pCWidget__Function func55;  // slot 55: FUN_6f633ec0 (+0x0dc, size 565) -- UNVERIFIED
    pCWidget__Function func56;  // slot 56: FUN_6f634bd0 (+0x0e0, size 145) -- UNVERIFIED
    pCWidget__Function func57;  // slot 57: FUN_6f632eb0 (+0x0e4, size 68) -- UNVERIFIED
    pCWidget__Function func58;  // slot 58: FUN_6f635490 (+0x0e8, size 3) -- UNVERIFIED
    pCWidget__Function func59;  // slot 59: FUN_6f634e60 (+0x0ec, size 4) -- behavior: CWidget base FUN_6f634e60 returns -1; CUnit override FUN_6f668670 returns *(this+0x58); result used as index into +0x6c table (FUN_6f6af530) and as unit id/level (FUN_6f678380); exact meaning UNVERIFIED (S3)
    pCWidget__Function func60;  // slot 60: FUN_6f6351c0 (+0x0f0, size 5) -- UNVERIFIED
    pCWidget__Function func61;  // slot 61: FUN_6f6347f0 (+0x0f4, size 101) -- UNVERIFIED
    pCWidget__Function func62;  // slot 62: FUN_6f6348d0 (+0x0f8, size 38) -- UNVERIFIED
    pCWidget__Function func63;  // slot 63: FUN_6f632f50 (+0x0fc, size 132) -- UNVERIFIED
    pCWidget__Function func64;  // slot 64: FUN_6f632f00 (+0x100, size 76) -- UNVERIFIED
    pCWidget__Function func65;  // slot 65: FUN_6f636700 (+0x104, size 172) -- UNVERIFIED
    pCWidget__Function func66;  // slot 66: FUN_6f635d00 (+0x108, size 41) strings=[ScaleFactor, SelectionCircle] -- UNVERIFIED
    pCWidget__Function func67;  // slot 67: FUN_6f634b30 (+0x10c, size 16) -- UNVERIFIED
    pCWidget__Function func68;  // slot 68: FUN_6f634900 (+0x110, size 272) -- UNVERIFIED
    pCWidget__Function func69;  // slot 69: FUN_6f634ea0 (+0x114, size 3) -- UNVERIFIED
    pCWidget__Function func70;  // slot 70: FUN_6f634ed0 (+0x118, size 18) -- UNVERIFIED
    pCWidget__Function func71;  // slot 71: FUN_6f635ba0 (+0x11c, size 3) -- UNVERIFIED
    pCWidget__Function applyDamage;  // slot 72: FUN_6f6364f0 (+0x120, size 53) -- BEHAVIOR-DERIVED, CONFIRMED (S3): FUN_6f6af530 calls +0x120 on the damage branch; CUnit override FUN_6f67dc40 references MagicImmunesResistDamage / EtherealDamageBonusAlly and calls +0x128 on the negative-damage path
    pCWidget__SetLife setLife;  // slot 73: FUN_6f636020 (+0x124, size 3) -- name KEPT from TinkerWorX (not independently recovered); CWidget base FUN_6f636020 is a 3-byte empty stub; CUnit override FUN_6f67b310 clamps against DAT_6fbb81d4; signature (float*) consistent with TinkerWorX (S3, vfunc_decomp.txt)
    pCWidget__Function applyHeal;  // slot 74: FUN_6f632fe0 (+0x128, size 18) -- BEHAVIOR-DERIVED, CONFIRMED (S3): heal branch calls +0x128; CWidget base FUN_6f632fe0 writes DAT_6fbb81d4 into the out-param; CUnit override FUN_6f652e30 does float add/sub
    pCWidget__Function func75;  // slot 75: FUN_6f634de0 (+0x12c, size 18) -- UNVERIFIED
    pCWidget__Function func76;  // slot 76: FUN_6f634e00 (+0x130, size 18) -- UNVERIFIED
    pCWidget__Function func77;  // slot 77: FUN_6f634e20 (+0x134, size 18) -- UNVERIFIED
    pCWidget__Function func78;  // slot 78: FUN_6f632e00 (+0x138, size 21) -- UNVERIFIED
    pCWidget__Function func79;  // slot 79: FUN_6f635550 (+0x13c, size 38) -- UNVERIFIED
    pCWidget__Function func80;  // slot 80: FUN_6f636040 (+0x140, size 3) -- UNVERIFIED
    pCWidget__Function func81;  // slot 81: FUN_6f636030 (+0x144, size 3) -- UNVERIFIED
    pCWidget__Function func82;  // slot 82: FUN_6f632a70 (+0x148, size 524) strings=[CWidget.cpp] -- UNVERIFIED
    pCWidget__Function func83;  // slot 83: FUN_6f6334d0 (+0x14c, size 226) strings=[delete] -- UNVERIFIED
    pCWidget__Function func84;  // slot 84: FUN_6f633da0 (+0x150, size 183) -- UNVERIFIED
    pCWidget__Function func85;  // slot 85: FUN_6f636dc0 (+0x154, size 403) -- UNVERIFIED
    pCWidget__Function func86;  // slot 86: FUN_6f634e70 (+0x158, size 3) -- UNVERIFIED
    pCWidget__Function func87;  // slot 87: FUN_6f636060 (+0x15c, size 25) -- UNVERIFIED
    pCWidget__Function func88;  // slot 88: FUN_6f6335e0 (+0x160, size 6) -- UNVERIFIED
    pCWidget__Function func89;  // slot 89: FUN_6f6335d0 (+0x164, size 6) -- UNVERIFIED
    pCWidget__Function func90;  // slot 90: FUN_6f6333a0 (+0x168, size 18) -- UNVERIFIED
    pCWidget__Function func91;  // slot 91: FUN_6f635a40 (+0x16c, size 1) -- UNVERIFIED
    pCWidget__Function func92;  // slot 92: FUN_6f6364b0 (+0x170, size 4) -- UNVERIFIED
    pCWidget__Function func93;  // slot 93: FUN_6f636090 (+0x174, size 3) -- UNVERIFIED
    pCWidget__Function func94;  // slot 94: FUN_6f6347e0 (+0x178, size 15) -- UNVERIFIED
    pCWidget__Function func95;  // slot 95: FUN_6f632df0 (+0x17c, size 3) -- UNVERIFIED
};

// Compile-time check: 96 slots x 4 bytes (32-bit target only).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CWidget_VTable) == 384, "CWidget_VTable must be 96 slots");
#endif

#endif // WARCRAF_CWIDGET_VTABLE_H
