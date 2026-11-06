// CFrame.h -- UI Frame system (FrameDef templates, layout, controls).
//
// Evidence: ~/workspace/warcraf_stage_ui.md (S6).
//   - Inheritance: CLayer (FUN_6f097050, 395 B) -> CFrame (FUN_6f0a0b80,
//     355 B, 56 callers) -> CSpriteFrame (FUN_6f0a2c20) -> CControl
//     (FUN_6f0bddc0) -> CTextFrame (FUN_6f0a9000, 0x240 B) / CButtonFrame
//     (FUN_6f0aaac0, 0x1e4 B) / CEditBox (FUN_6f0ae290, 1480 B) /
//     CChatDisplay (FUN_6f0b4350, 0x224 B); CMessageFrame (FUN_6f0a71b0),
//     CScrollFrame (0x180 B), CFloatingFrame (0x174 B), CCursorFrame
//     (0x1b0 B), CBackdropFrame (render FUN_6f0a4cd0, 5417 B, nine-slice),
//     CScreenFrame (lazy children FUN_6f0ca0f0 / FUN_6f0ca020).
//   - CFrame has TWO vtables ([0] and [0x2d] / +0xb4); FrameDef templates
//     (.fdf, UI\FrameDef\FrameDef.toc, FramedefErrors.log); anchors
//     CFramePointAbsolute / CFramePointRelative; layout via FUN_6f0bd630
//     (dirty list, sentinel PTR_LOOP_6fb6641c) and CLayoutFrame::FRAMENODE;
//     CTextFrame::SetText FUN_6f0aa130 (148 callers); UI init FUN_6f0280e0
//     (948 B).
//
// Field-level layouts of the Frame classes are UNVERIFIED (only the total
// sizes above are evidenced). The CFrame vtable method table, the Win32
// message-pump -> CButtonFrame click dispatch, the .fdf handler names, the
// DAT_6f95d420 constant table and the FrameDef numeric IDs (0x400b0064 /
// 0x40100064 / 0x40060064) are UNVERIFIED.

#ifndef WARCRAF_CFRAME_H
#define WARCRAF_CFRAME_H

#include "warcraf/core/Types.h"

// ---- Frame class sizes (verified totals, S6) -------------------------------
//   CTextFrame     0x240 B    CButtonFrame  0x1e4 B    CChatDisplay 0x224 B
//   CScrollFrame   0x180 B    CFloatingFrame 0x174 B   CCursorFrame 0x1b0 B
// (CFrame / CLayer / CSpriteFrame / CControl / CMessageFrame / CEditBox /
// CBackdropFrame / CScreenFrame total sizes were not recovered.)
// The structs below pin ONLY the total size (remainder arrays); internal
// field layout is UNVERIFIED. Base subobjects are declared as the 4-byte
// vtable pointer + one placeholder dword each, so the remainder counts are
// pure arithmetic -- they are NOT evidence about the real fields.
struct CFrame { /* size = UNVERIFIED */
    void* vtable;      // +0x00: first vtable (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
// Second vtable: S6 reports CFrame has two vtables, the second at [0x2d] /
// +0xb4. Whether +0xb4 is an object offset or a vtable-slot index is
// UNVERIFIED; it is recorded here as text only, NOT as a struct member.
struct CLayer { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct CSpriteFrame { /* size = UNVERIFIED */
    CFrame base;       // base subobject (layout UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct CControl { /* size = UNVERIFIED */
    CSpriteFrame base; // base subobject (layout UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct CTextFrame { /* size = 0x240 */
    CControl base;     // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x8b];  // remainder to 0x240 (contents UNVERIFIED)
};
struct CButtonFrame { /* size = 0x1e4 */
    CControl base;     // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x75];  // remainder to 0x1e4 (contents UNVERIFIED)
};
struct CChatDisplay { /* size = 0x224 */
    CControl base;     // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x85];  // remainder to 0x224 (contents UNVERIFIED)
};
struct CScrollFrame { /* size = 0x180 */
    CFrame base;       // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x5e];  // remainder to 0x180 (contents UNVERIFIED)
};
struct CFloatingFrame { /* size = 0x174 */
    CFrame base;       // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x5b];  // remainder to 0x174 (contents UNVERIFIED)
};
struct CCursorFrame { /* size = 0x1b0 */
    CFrame base;       // base subobject (layout UNVERIFIED)
    _DWORD field_4[0x6a];  // remainder to 0x1b0 (contents UNVERIFIED)
};

// ---- FrameDef templates (S6) -----------------------------------------------
// Templates live in .fdf files, indexed by UI\FrameDef\FrameDef.toc;
// load errors go to FramedefErrors.log. FrameDef lookup by name reads the
// [0x1c] text field and copies it (FUN_6f090b00 -- also referenced by S7).
// Anchors: CFramePointAbsolute / CFramePointRelative (RTTI names; layouts
// UNVERIFIED). Layout invalidation: FUN_6f0bd630 walks a dirty list with
// sentinel PTR_LOOP_6fb6641c; nodes are CLayoutFrame::FRAMENODE.
// Nine-slice rendering: CBackdropFrame render FUN_6f0a4cd0 (5417 B);
// slice flag at +0x6c (S12 summary -- the containing object is UNVERIFIED).

// ---- Key functions (verified addresses, S6) --------------------------------
//   FUN_6f0280e0  UI/engine init entry (948 B, 46 callees, 0 callers)
//   FUN_6f097050  CLayer constructor (395 B)
//   FUN_6f0a0b80  CFrame constructor (355 B, 56 callers)
//   FUN_6f0a2c20  CSpriteFrame constructor
//   FUN_6f0bddc0  CControl constructor
//   FUN_6f0a9000  CTextFrame constructor
//   FUN_6f0aaac0  CButtonFrame constructor
//   FUN_6f0ae290  CEditBox constructor (1480 B)
//   FUN_6f0b4350  CChatDisplay constructor
//   FUN_6f0a71b0  CMessageFrame constructor
//   FUN_6f0a4cd0  CBackdropFrame render (5417 B, nine-slice)
//   FUN_6f0ca0f0 / FUN_6f0ca020  CScreenFrame lazy child creation
//   FUN_6f0aa130  CTextFrame::SetText (148 callers)
//   FUN_6f0bd630  layout (dirty list)
//   FUN_6f090b00  FrameDef lookup by name ([0x1c] text field)
//   FUN_6f12a6a0  CBackdropFrame primitive submit into CGxDevice batch (S12)
// ---- Compile-time layout checks (verified sizes only) ---------------------
// All Frame structs contain a vtable pointer: 32-bit target only (Types.h).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CTextFrame) == 0x240, "CTextFrame must be 0x240 bytes");
static_assert(sizeof(CButtonFrame) == 0x1e4, "CButtonFrame must be 0x1e4 bytes");
static_assert(sizeof(CChatDisplay) == 0x224, "CChatDisplay must be 0x224 bytes");
static_assert(sizeof(CScrollFrame) == 0x180, "CScrollFrame must be 0x180 bytes");
static_assert(sizeof(CFloatingFrame) == 0x174, "CFloatingFrame must be 0x174 bytes");
static_assert(sizeof(CCursorFrame) == 0x1b0, "CCursorFrame must be 0x1b0 bytes");
#endif

#endif // WARCRAF_CFRAME_H
