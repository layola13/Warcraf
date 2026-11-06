# Warcraf — Warcraft III 1.27a engine: header set + network-layer reconstruction

A C++ header description of the Warcraft III 1.27a (1.27.0.52240, build
f7ba2a3, 2016-02-12) engine's memory layout: classes, vtables, network
session state, Storm ordinals, JASS2 natives, renderer, UI, audio, replay,
Battle.net, RNG. It describes the engine; it does **not** reimplement it —
except for the network layer (see below), which is a runnable
reconstruction from the Ghidra decompiles. The real code lives in
`Game.dll` / `Storm.dll` / `Mss32.dll` (see `dll/`).

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
            NetChecksum.h   event checksum (FUN_6f30ade0) -- RECONSTRUCTION
            NetDataStore.h  CDataStore buffer primitives -- RECONSTRUCTION
            NetCommandCodec.h  CNetCommand wire codec -- RECONSTRUCTION
            NetSession.h    runtime session state -- RECONSTRUCTION
            NetEventPump.h  event pump + dispatchers -- RECONSTRUCTION
            TurnsSync.h     lockstep core -- RECONSTRUCTION
            NetTransport.h  router setup + UDP transport -- RECONSTRUCTION
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
                            functions Storm.h declares) -- EXCEPT net/:
                            the seven Net*.cpp files are the real network
                            reconstruction (CBattleNet.cpp is still a stub)
  apps/net_demo/            warcraf_net_demo: two loopback peers, lockstep
  tests/                    warcraf_net_tests: 96 unit checks
  build/                    cmake build output (libwarcraf.a, demo, tests)
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
| net/NetChecksum.h + NetDataStore.h + NetCommandCodec.h + NetSession.h + NetEventPump.h + TurnsSync.h + NetTransport.h | S1/S2/S3 | `net_decomp.txt` (FUN_6f310c80/6f30b1a0/6f2feb10/6f2fd0d0/6f307980/6f307880/6f283540/6f30e2b0) + `sync_decomp.txt` (FUN_6f30ade0) — see "Network layer reconstruction" below |
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
cmake --build build        # libwarcraf.a + warcraf_net_demo + warcraf_net_tests
```

C++17, include dir `include/`, explicit source list in `CMakeLists.txt`
(grouped by module). Verified 2026-11-06 on this host (g++ 13.3.0,
cmake 3.28.3, 64-bit Linux): configure OK, build OK, `libwarcraf.a`
97,454 bytes (was 40,152 before the network reconstruction).
Every header also passes `g++ -fsyntax-only` standalone.

### Demo and tests (network layer)

```sh
./build/warcraf_net_tests            # 94 passed, 0 failed, 2 skipped (see below)
./build/warcraf_net_demo --turns 20 # two loopback peers, 25 ms steps
./build/warcraf_net_demo --turns 50 --step-ms 10   # longer run, faster step
```

Demo output (20 turns, 2026-11-06): every turn applied by both peers,
`RESULT: OK -- 20 turns in lockstep, checksums consistent, no lost
steps`. Per peer: `applied 20/20 turns | checksum mismatches 0 | lost
steps 0 | bytes accumulated slot0=480 slot1=480 | bandwidth avg
0x00000011 | global counter 20`. The two peers' session checksums
differ (each folds its own received stream) — that is expected; the
per-chunk checksums are what must match, and they do (0 mismatches).

**Host note (UDP):** this sandbox rejects UDP `sendto` with EPERM
(bind and TCP work; only datagram sends are blocked). The demo probes
for that at startup and, when UDP is blocked, carries the identical
datagram bytes over an in-process loopback channel instead — the
checksum, pump, and lockstep logic are exercised exactly the same way,
and the demo prints which channel it used. On an unrestricted host the
real UDP path is used. The two UDP round-trip unit tests are likewise
SKIPPED (not passed) on this host; the other 94 checks run and pass.
`NetTransport` itself stays UDP in both cases.

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

## Network layer reconstruction (2026-11-06)

Scope (user-confirmed): **basic network, frame sync, demo only** —
Battle.net, replay, rendering, UI are untouched. The reconstruction is
faithful to the decompiles: function division, constants, offsets,
algorithms, and event flow follow `~/workspace/warcraf_ghidra/
net_decomp.txt` + `sync_decomp.txt` and `warcraf_stage_network.md` (S1).
Each implemented function cites its original address + evidence source
in a comment. Nothing is pasted from the decompiles: the logic,
constants, and flow are re-expressed in this project's own code.
Names are meaningful (confirmed Blizzard names where confirmed, e.g.
`getClassId`, `SMemFree`; otherwise behavior-descriptive names with
evidence + confidence in comments). `FUN_6fxxxxxx` / `funcN` are never
used as implementation names.

### What is implemented (evidence per part)

| Part | Original function(s) | Evidence |
|---|---|---|
| Event checksum: payload roll `rotl(acc ^ word, 3)`, 0x7a2d fold, session accumulator +0x227c | FUN_6f30ade0 | `sync_decomp.txt` lines ~830–910; S1 §3.2/§6 |
| DataStore buffer: write dword/byte, sequential reads, read-three, 1460-byte cache | FUN_6f04e1d0 / 6f04dfa0 / 6f83f8e0 / 6f04d3b0 | CNetCommand.h (S3); `net_decomp.txt` FUN_6f2feb10 (CDataStoreCache<1460>) |
| Command codec: 0x1a/0x1b/0x1c (1 dword), 0x17 (dword+byte+2 dwords), 0x16 (byte+dword, partial) | FUN_6f83ee60 / 6f83f010 / 6f83efd0 / 6f83f090 / 6f83f060 | CNetCommand.h reader table; S3 |
| Session state: mode tag +0x2288, local slot +0x610, frame state +0x278, checksum +0x227c, budget +0x225c, bandwidth +0x2258, counter +0x1c68, record state +0xbe4, per-player +0x284/+0x2e0 | — (fields) | CNetData.h; S1 |
| Game magic "WAR3"/"W3XP" | FUN_6f30e2b0 | `net_decomp.txt` |
| Event pump: outer gate (+0x173c/+0x1744), checksum gate, 0x17/0x21/0x2f handling, pre/post dispatch, 0x1e/0x1f break, param_2==0 break, BN stub for ≥0x40 | FUN_6f30ade0 / 6f307980 / 6f307880 / 6f3098d0 / 6f307b50 | `sync_decomp.txt`; `net_decomp.txt` |
| TurnsSync accounting: slot-0 counter, +0x284 accum, +0x225c budget, (old*7+bytes)>>3 average, countdown max−bytes, advance + local actions | FUN_6f310c80 | `net_decomp.txt`; S1 §4 |
| Turn store: submit / complete-when-all-in / take in slot order | CTurnsSyncStore (RTTI) | S0 |
| Turn-packet parse skeleton: 3-byte header, count, type-byte dispatch 0x10–0x14/0x20 | FUN_6f30b1a0 | `net_decomp.txt`; S1 |
| Per-turn log line `War3Net: (INFO): turn %u: %s from %u` | FUN_6f2fd0d0 | `net_decomp.txt` |
| Router setup: flag rules (0x10/0x400000→param6=0xc; 0x200 reuse; 0x80000000), SStrCopy bounds (name 0x20, password 0x10, map/path 0x36) | FUN_6f283540 | `net_decomp.txt`; S1 §5 |
| Event send entry (type + store) | FUN_6f2feb10 | `net_decomp.txt` |
| UDP transport (POSIX sockets; Winsock on Windows, untested here) | WS2_32 + internal Net functions | S1 §2 (SNet is NOT in Storm's import table) |

Two corrections the decompiles forced during this work (the S1 text was
less precise): **0x1e shares the 0x1f checksum block** (both set
`local_38`, so both stop the pump after dispatch — S1 mentions 0x1f
only), and **param_2 == 0 breaks before the fold and before dispatch**
(the event stays queued; it is not "dispatched then stopped"). The
implementation follows the decompile in both cases; the headers note
the discrepancy.

### What is still stub / unverified in the network layer

- **Battle.net**: the pump counts ≥0x40 events in a stub only
  (FUN_6f307b50 not implemented; no handshake). Out of scope.
- **Wire envelope**: the datagram carries exactly the CDataStore
  bytes; no envelope is added. The event type that FUN_6f2feb10
  passes to FUN_6f311750 is not placed on the wire — FUN_6f311750's
  internals are not in the decompiles, so its wire effect is an
  evidence gap. The demo's receiving side is told the event type by
  the harness (0x1f), not by the wire.
- **Turn-chunk framing** (`[u32 length][commands][0x1f]`): a
  reconstruction from the 0x1e/0x1f batch-path description (a length
  is read first, 0x1f sets the end flag) — marked as such in
  `NetCommandCodec.h`, not claimed as the original's.
- **Command layouts**: only 0x1a/0x1b/0x1c/0x17/0x16 are evidenced.
  0x23's list layout cannot be reconstructed (its count lives at a
  reader-frame offset, not a wire offset); every other command number
  has no evidenced reader. The codec refuses those instead of
  inventing a layout. Command-number semantics (move/attack/cast) are
  UNVERIFIED everywhere.
- **Turn-packet sub-record payloads**: the six constructors
  (FUN_6f2fc4b0/6f2fc7d0/6f2fc590/6f2fc710/6f2fc650/6f2fc3d0) are
  naming gaps; the parser reports them and stops unless the caller
  states the payload size (the demo's bare type-byte packets do).
- **Byte count source** FUN_6f3086a0, **transmit** FUN_6f311750,
  **store fill** FUN_6f83f350, **countdown-zero** FUN_6f30b460,
  **advance** FUN_6f3130c0, **local actions** FUN_6f3023c0/6f304920/
  6f840bd0/6f3041c0, **retain** FUN_6f312dc0, **post-dispatch**
  FUN_6f3039f0, **delivery** FUN_6f30ada0, **preprocess**
  FUN_6f02d2d0, **0x21/0x2f follow-ups** FUN_6f83e540/6f83e670,
  **0x17 store path** FUN_6f0fb2c0, **router internals**
  FUN_6f1c0680/6f1cccc0/6f1bb730/6f76e060/6f0210a0/6f287160/6f2821f0/
  6f842600/6f18cb00: addresses known, identities/behaviour
  unidentified — they appear as named callbacks/hooks, never as
  invented implementations.
- **Player-table size**: the per-player stride (0x304) is evidenced;
  how many entries the table has is not — the session takes the
  player count as a parameter (the demo uses 2).
- **DataStore growth factor**: the original grows via its vtable
  (+0x0c) through SMemAlloc/ReAlloc; the factor is not evidenced —
  this implementation doubles (marked as reconstruction).
- **0x1e out / 0x1f in direction**: S1 lists them together without a
  direction; the demo's assignment is an interpretation (both go
  through the same checksum/accounting path).
- **UDP vs stream**: the original's turn path is carried here over
  UDP datagrams (per the task brief); whether the original used
  datagrams or a stream for turns is not evidenced (OsNet RTTI
  includes TCPLISTEN).

## What is verified / stub / unverified

- **Verified:** all offsets/sizes/addresses named in the headers, each
  with a cited source; `static_assert`s pin the checked layouts at
  compile time (pointer-free ones on any host, all of them on a 32-bit
  target). Vtable slot counts and per-slot function addresses come from
  the Ghidra dump; renamed slots carry their evidence in the comment.
  The network reconstruction is verified by `warcraf_net_tests`
  (94 checks; checksum vectors cross-checked against an independent
  Python transcription of FUN_6f30ade0) and by `warcraf_net_demo`
  (20 and 50 turns, lockstep, 0 checksum mismatches, 0 lost steps).
- **Stub:** every `.cpp` under `src/` is a placeholder (`占位桩，仅用于占位与
  编译通过，不含引擎实现`) — **except** the seven `src/warcraf/net/
  Net*.cpp` files, which are the real network reconstruction, and
  `core/Storm.cpp` (empty bodies for the 7 Storm functions).
  `CBattleNet.cpp` is still a stub (Battle.net out of scope).
  No engine behaviour is implemented here beyond the network layer.
- **Unverified:** every `field_XX` / `funcN` / `UNVERIFIED` marker in the
  headers, plus the per-header notes. Largest gaps: most vtable slot
  identities (only 3–11 slots per vtable are named, the rest are funcN);
  all CFrame/CGxDevice/CAbility-subclass internal layouts; the
  command-queue drain function; the Battle.net handshake packet format;
  the `.w3g` file format; the three RNG states' individual purposes;
  the S1 unmapped Storm ordinals (151, 428, 471, 524, 525, 534, 537, 568,
  589, 595, 596, 597) and unknown-semantics SErr470 / SMem496 / SLog543;
  the network-layer gaps listed in the section above.

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
  Do NOT re-run it over `src/warcraf/net/`: the CNetData / NetRouter /
  CNetCommand stubs were replaced by hand-written implementations
  (NetSession / NetTransport / NetCommandCodec etc.) in 2026-11-06.

## What is still unverified (short list)

See each header's `UNVERIFIED` markers for the full list. The biggest
items: most vtable slot identities; CFrame/CGxDevice/CAbility-subclass
internal layouts; command-queue drain; Battle.net handshake; `.w3g`
format; RNG state purposes; S1 unmapped ordinals and SErr470 / SMem496 /
SLog543 semantics; `CNetData` total size beyond +0x22c0; the engine-loop
→ frame-step "last hop" dispatch.
