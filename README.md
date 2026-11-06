# Warcraf — Warcraft III 1.27a engine: header set

A C++ header description of the Warcraft III 1.27a (1.27.0.52240, build
f7ba2a3, 2016-02-12) engine's memory layout: classes, vtables, network
session state, Storm ordinals, JASS2 natives, renderer, UI, audio, replay,
Battle.net, RNG. It describes the engine; it does **not** reimplement it.
The real code lives in `Game.dll` / `Storm.dll` / `Mss32.dll` (see `dll/`).

Evidence: the S0–S12 analysis in `docs/` (frame sync, network, main loop,
simulation, JASS2, render, UI, MPQ, audio, replay, Battle.net, RNG,
cross-validation), Ghidra dumps in `~/workspace/warcraf_ghidra/`, and the
TinkerWorX RE headers + IDC scripts in `~/workspace/warcraftIII/`.
Every header cites its evidence source at the top. Anything not backed by
one of those sources is a `field_XX` / `funcN` placeholder marked
**UNVERIFIED** — never a guess.

## Directory tree

```
Warcraf/
  CMakeLists.txt            static lib target `warcraf`, C++17
  README.md                 this file
  docs/                     S0–S12 analysis docs (already pushed, commit 4bde85f6)
  dll/                      the analysed 32-bit DLLs
  include/warcraf/
    Warcraf.h               umbrella (includes all headers, module order)
    core/   Types.h         _DWORD/_WORD/_BYTE/BOOL, image bases, 32-bit guard
            Storm.h         SMem/SReg/SErr/SCmd/SLog/SStr/SComp/SBig + ordinals
            CRng.h          S11 RNG: state, 64-dword table, seeding, checksum
    math/   Vector2.h       TinkerWorX copy
            Vector3.h       TinkerWorX copy
    net/    CNetData.h      CNetEvent, CNetData session, TurnsSync
            NetRouter.h     NetRouter / NetClient / OsNet
            CNetCommand.h   turn commands, CDataStore, WidgetDamageData
            CBattleNet.h    CONNSTATE, Battle.net events
    io/     SFile.h         Storm SFile/MPQ part (split out of Storm.h)
            CReplay.h       replay recording/playback, save-game classes
    engine/ CMainLoop.h     GameMain, engine loop, 25 ms step, EvtSched
    script/ Jass.h          JASS2 VM, native table, triggers, handles
    sim/    CAgent.h, CAgentWar3.h, CWar3Image.h, CWidget.h, CSelectable.h,
            CUnit.h, CUnitRefList.h, CAbility.h, CharArray.h, StringArray.h,
            AbilityUIDef.h, AbilityLevelData.h, AbilDataCacheNode.h, main.h
                            TinkerWorX copies (MIT, see below)
            CAgent_VTable.h, CAgentWar3_VTable.h, CWar3Image_VTable.h,
            CWidget_VTable.h, CSelectable_VTable.h, CUnit_VTable.h,
            CUnitRefList_VTable.h, CAbility_VTable.h
                            CORRECTED vtable headers (see below)
    render/ CGx.h           Gx renderer abstraction (D3D9 / OpenGL)
    ui/     CFrame.h        Frame/UI system
    audio/  CMiles.h        Miles Sound System 6.1d audio
  src/warcraf/              one .cpp placeholder stub per header, mirroring
                            include/; only core/Storm.cpp has function bodies
                            (empty default-returning stubs for the 7 Storm
                            functions Storm.h declares)
  build/                    cmake build output (libwarcraf.a)
```

## Module -> analysis stage / evidence

| Module | Stage | Evidence |
|---|---|---|
| core/Types.h | — | TinkerWorX convention (`_DWORD` undefined there; defined here) |
| core/Storm.h | S1/S7 | Map Storm Imports.idc (ordinal table + 7 IDC signatures); S1 VA ranges |
| core/CRng.h | S11 | `warcraf_stage_rng.md` + appendix A table; `rng_dump*.txt` |
| math/ | — | TinkerWorX copies |
| net/CNetData.h | S0/S1/S2/S11 | `warcraf_framesync_analysis.md`, `warcraf_stage_network.md`, `warcraf_stage_mainloop.md` |
| net/NetRouter.h | S1/S10 | S1 router setup; S10 OsNet RTTI |
| net/CNetCommand.h | S2/S3/S9 | S3 command objects; S2 dispatch table; S9 class names |
| net/CBattleNet.h | S10 | `warcraf_stage_battlenet.md` (CONNSTATE table, dispatcher) |
| io/SFile.h | S1/S7 | S7 MPQ layout; S1 ordinal ranges; IDC ordinal names |
| io/CReplay.h | S9 | `warcraf_stage_replay.md` |
| engine/CMainLoop.h | S2 | `warcraf_stage_mainloop.md` |
| script/Jass.h | S3/S4/S11 | S4 interpreter/registries; Map Natives.idc; S3 native signatures |
| sim/ (copies) | S3 | TinkerWorX headers; S3 field spot-check (see below) |
| sim/ (vtables) | S3 | Ghidra `vtable_dump.txt` + `vfunc_decomp.txt` (see below) |
| render/CGx.h | S5 | `warcraf_stage_render.md` |
| ui/CFrame.h | S6 | `warcraf_stage_ui.md` |
| audio/CMiles.h | S8 | `warcraf_stage_audio.md` |

## Build

```sh
cmake -B build -S .
cmake --build build        # produces build/libwarcraf.a
```

C++17, include dir `include/`, explicit source list in `CMakeLists.txt`
(grouped by module). Verified 2026-11-06 on this host (g++ 13.3.0,
cmake 3.28.3, 64-bit Linux): configure OK, build OK, `libwarcraf.a`
40,152 bytes. Every header also passes `g++ -fsyntax-only` standalone.

**32-bit caveat (read this):** the layout claims are 32-bit claims
(pointers are 4 bytes). `WARCRAF_32BIT_LAYOUT` (in `core/Types.h`) is 1
only on a 32-bit target; on a 64-bit host the pointer-dependent
`static_assert`s are skipped and only the pointer-free subset is checked
(CNetData, ClientTurnsSync, CNetCommandBase, MilesSound, RandState,
kRandTable). Full verification needs a 32-bit target (MSVC x86, or
`g++ -m32` with multilib — **not installed on this host**: `-m32` fails
to link, no 32-bit C runtime/headers, so the 32-bit asserts are
untested here). The pointer-free asserts *did* fire on this host — and
caught one real bug during this work (see correction list).

## What is verified / stub / unverified

- **Verified:** all offsets/sizes/addresses named in the headers, each
  with a cited source; `static_assert`s pin the checked layouts at
  compile time (pointer-free ones on any host, all of them on a 32-bit
  target). Vtable slot counts and per-slot function addresses come from
  the Ghidra dump; renamed slots carry their evidence in the comment.
- **Stub:** every `.cpp` under `src/` is a placeholder (`占位桩，仅用于占位与
  编译通过，不含引擎实现`). The 7 Storm functions get empty bodies returning
  defaults; everything else is include-only. No engine behaviour is
  implemented here.
- **Unverified:** every `field_XX` / `funcN` / `UNVERIFIED` marker in the
  headers, plus the per-header notes. Largest gaps: most vtable slot
  identities (only 3–11 slots per vtable are named, the rest are funcN);
  all CFrame/CGxDevice/CAbility-subclass internal layouts; the
  command-queue drain function; the Battle.net handshake packet format;
  the `.w3g` file format; the three RNG states' individual purposes;
  the S1 unmapped Storm ordinals (151, 428, 471, 524, 525, 534, 537, 568,
  589, 595, 596, 597) and unknown-semantics SErr470 / SMem496 / SLog543.

## Correction list (TinkerWorX originals -> this project)

Vtable corrections — method: Ghidra RTTI type-name string -> TypeDescriptor
(string−8) -> Complete Object Locator -> vtable (COL pointer) -> per-slot
function address -> string-xref / decompile identity. Dump:
`~/workspace/warcraf_ghidra/vtable_dump.txt` (635 slot lines);
decompiles: `~/workspace/warcraf_ghidra/vfunc_decomp.txt` (17 functions).

| Class | Vtable VA | Real slots | TinkerWorX | Change |
|---|---|---|---|---|
| CAgent | 0x6f950bd0 | 24 | 24 | slot 1 renamed `deletingDestructor` (behavior-derived: body references "delete"; MSVC scalar-deleting-destructor slot by convention — NOT a recovered symbol); slots 7/22 CONFIRMED `getClassId` (returns 0x2b616761, FUN_6f038b90) / `getClassName` (returns "CAgent", FUN_6f0219b0) |
| CAgentWar3 | 0x6f950c4c | 30 | — (no header) | NEW header; slots 7/22 same confirmed names (values not decompiled for this class) |
| CWar3Image | 0x6f970694 | 39 | — (no header) | NEW header; slot 7 `getClassId` returns 0x2b773369 (FUN_6f6a0d70) |
| CWidget | 0x6fa49f4c | 96 | 76 claimed / 77 listed | **slots 77–95 were missing — added**; slot 72 `applyDamage` (behavior-derived, CONFIRMED: S3 FUN_6f6af530 calls +0x120), slot 73 `setLife` (name KEPT from TinkerWorX; base FUN_6f636020 is a 3-byte empty stub), slot 74 `applyHeal` (behavior-derived, CONFIRMED: S3 heal branch calls +0x128) |
| CSelectable | 0x6fa4a118 | 107 | — (no header) | NEW header; slots 72–74 same as CWidget |
| CUnit | 0x6fa4a704 | 118 | adds nothing | **CUnit adds slots 107–117**; slot 7 `getClassId` returns 0x2b773375 (FUN_6f66adb0); notable overrides recorded in the header (slot 3 FUN_6f672d60 id-dispatch, slot 23 thunk to FUN_6f0385d0, slot 59 FUN_6f668670 returns *(this+0x58), slots 72/73/74 overrides FUN_6f67dc40 / FUN_6f67b310 / FUN_6f652e30) |
| CUnitRefList | 0x6f995758 | 32 | — (no header) | NEW header |
| CAbility | 0x6f9957ec | 189 | — (no header) | NEW header; slot 7 `getClassId` returns 0x6162696c (FUN_6f3fccd0); slots 167–171, 177–179 string-CONFIRMED: `getEffectArt` / `getAreaEffectArt` / `getTargetArt` / `getEffectSound` / `getEffectSoundLooped` / `getMissileArc` / `getMissileSpeed` / `getMissileHoming` ("CAbility::GetXxx" strings); slot 13 is a thunk to the 1-byte base stub FUN_6f038610 |
| AbilityUIDef / AbilDataCacheNode / AbilityLevelData | — | no vtable | — | no RTTI found: value types, no vtable (confirmed by the dump run) |

Slot 0 in all eight vtables is the shared FUN_6f0218c0 (calls slot-7
getClassId, then dispatches via FUN_6f0219f0 registry) — identity
UNVERIFIED, kept as `func0` with a behavior note. Slot 23 (CAgent
FUN_6f0385d0, class-id-gated update) is UNVERIFIED, kept as `func23`.
Slot 59 is kept as `func59` with a behavior note (CWidget base returns
−1; CUnit override returns *(this+0x58); exact meaning UNVERIFIED).
String hints that are annotated but NOT renamed: CAgent slot 2
(`.?AUObserverEventReg@@`), CWidget slot 15 (`CWidget.cpp`), CWidget /
CSelectable slot 66 and CUnit slots 100/101/104/113 (`ScaleFactor` /
`SelectionCircle` / `ColorNeutral` / `ColorFriend` / `AllyOffset` /
`ImageWalkableZOffset`), CUnit slot 107
(`BuildingPlacementNotifyRadius` / `BuildingsOcclude` / `Occlusion` /
`CmdCancel` / `Ubertip`), CUnit slot 115 (`.../CUnitUI.cpp`), CAbility
slot 154 (`DepCheckAlias` / `Misc`), slot 173 (`PlacementCursor` /
`ReplaceableTextures\Selection\SpellAreaOfEffect.blp`), slot 176
(`TargetingColors` / `TargetLightColor`), CAgentWar3 slots 26/28
(`.?AVCAgentFieldMini@@`).

Field-offset spot-check (1.27a evidence, S3): **no field offsets were
found wrong.** Checked and kept: CAgent field_C (+0x0c, type-id check),
field_14/field_18 (flag bytes +0x17/+0x19 inside them); CUnit field_54
(+0x56 inside it), field_248 (+0x248, revive-timer candidate), position
Vector3 @0x284. All other copied files' fields were NOT spot-checked and
stay UNVERIFIED as in the original.

Layout bug found by the new `static_assert`s (2026-11-06):
`ClientTurnsSync::field_288` was `[0x12]` in the draft, which put
`countdown` at +0x2d0 and the struct at 0x2f4; corrected to `[0x16]`
(countdown +0x2e0, size 0x304). The assert caught it — this is why the
asserts exist.

Other corrections: `CNetCommandBase` starts at +0x00 (field_0/field_4
zeroed dwords, values +0x8/+0xc/+0x10, cmd +0x14, flag +0x15);
`CNetData` placeholder counts fixed during the S1 work (field_630
[0x62]→[0x64], field_9C0 [0x6b]→[0x6c], field_B70 [0x13]→[0x14],
field_BE8 [0x3e]→[0x76], field_1C74 [0x5a]→[0x53]; total 0x22c4,
verified by script, 0 mismatches). Storm ordinal 403 = SMemFree
(corrects the earlier S1 "conditional log" reading; FUN_6f304680 is a
debug-tagged conditional free) — the corrected S1 doc is in `docs/`.

## Conventions and deviations from TinkerWorX

- Style follows the TinkerWorX originals: plain C++ classes, inheritance
  chains, `field_XX` hex-offset fields, `_DWORD`/`int`/`float`/`void*`/
  `char*`, no `#pragma pack`, `/* size = N */` comments, vtable structs
  with `pClass__Function` typedefs.
- **Include guards added** (originals have none) — required because
  `Warcraf.h` includes the headers together.
- **`Types.h` defines `_DWORD` etc.** (the originals relied on the IDA
  prelude) and no-ops `__stdcall`/`__fastcall`/`__thiscall` on non-MSVC
  so the headers check with g++ on Linux.
- **Intra-repo includes retargeted** to `warcraf/sim/...` /
  `warcraf/math/...` (include dir is `include/`).
- **MIT attribution kept**: every copied file names its TinkerWorX
  source (MIT License, Copyright (c) 2017 Nikolaj Mariager) and states
  the original repository was not modified. `~/workspace/warcraftIII`
  is read-only source for this project.
- `CUnit.h` gains a `/* size = 0x310 */` note (ends at field_30C; the
  original has no size comment there). `CAgent.h`'s vtable union gains
  views through the five new vtable headers. Class headers include their
  own (new) vtable header.
- One known warning: `AbilityUIDef.h` has a field named `requires`,
  which is a C++20 keyword — harmless in C++17 (the project's standard),
  a `-Wc++20-compat` warning with `-Wall`; the name is kept as in the
  original.
- `CRng.h` places the S11 RNG in `core/` (determinism core) — the module
  layout does not name it elsewhere; recorded here.
- `SFile.h` holds the SFile/MPQ part split out of `Storm.h`; together
  they cover the whole Storm import table.

## Generator scripts (reproducibility)

In `~/workspace/warcraf_tools/` (outside this repo):
- `gen_vtables.py` — regenerates the 8 `sim/*_VTable.h` headers from
  `~/workspace/warcraf_ghidra/vtable_dump.txt` (rename map + evidence
  notes are in the script).
- `copy_tinkerworx.py` — copies the 16 TinkerWorX headers into `sim/` /
  `math/` with the attribution, guards, include retargeting, and
  `static_assert`s.
- `gen_stubs.py` — generates the 38 `src/` stubs and `CMakeLists.txt`.

## What is still unverified (short list)

See each header's `UNVERIFIED` markers for the full list. The biggest
items: most vtable slot identities; CFrame/CGxDevice/CAbility-subclass
internal layouts; command-queue drain; Battle.net handshake; `.w3g`
format; RNG state purposes; S1 unmapped ordinals and SErr470 / SMem496 /
SLog543 semantics; `CNetData` total size beyond +0x22c0; the engine-loop
→ frame-step "last hop" dispatch.
