// CMainLoop.h -- GameMain entry, engine loop, 25 ms frame step, EvtSched.
//
// Evidence: ~/workspace/warcraf_stage_mainloop.md (S2).
//   - GameMain @ 0x6f0297d0 (155 B, only export); engine loop FUN_6f05f710
//     (564 B, while(true), "Engine %x"); frame step FUN_6f30ff00 (1148 B,
//     25 ms fixed step, accumulator clamp 4000, watchdog 200 ms); turn apply
//     FUN_6f30bac0 (1530 B); EvtSched (FUN_6f05e410 init, "EvtSched#%d"
//     threads); message pump FUN_6f1532b0; SetGameState FUN_6f312740.
//   - Session offsets used by the frame step (see net/CNetData.h).
//
// UNVERIFIED: the engine-loop -> frame-step "last hop" (the only direct code
// reference to FUN_6f30ff00 is a ~20 B wrapper near 0x6f30c8d0 whose exact
// start and caller are unverified; FUN_6f05fb10's virtual call (**param_1)(1)
// is a candidate dispatch point with an unverified target); which thread
// FUN_6f05f710 runs on; the speed table DAT_6f987f14 values (7 entries,
// unread); the 0-6 game-state meanings; the 8-slot callback registry
// contents; the DAT_6f951e28 string in GameMain.

#ifndef WARCRAF_CMAINLOOP_H
#define WARCRAF_CMAINLOOP_H

#include "warcraf/core/Types.h"

// ---- Entry points (verified addresses/sizes, S2) ----------------------------
//   0x6f0297d0  GameMain (155 B, ONLY export; RVA 0x297d0, image base
//               0x6f000000). DLL entry (AddressOfEntryPoint RVA 0x77190a,
//               35 B) is only ___security_init_cookie + ___DllMainCRTStartup.
//   0x6f02bee0  process hardening (347 B; advapi32 deny-ACE 0xf01ffffe)
//   0x6f027d40  error reporting (47 B; ClassicErrorReporter)
//   0x6f027fb0  init chain (292 B; single-instance -> Storm -> subsystems
//               -> EvtSched; any failure -> FUN_6f0277c0 error box)
//   0x6f027660  shutdown cleanup (155 B)
//   0x6f05e710  engine-loop thin shell (28 B) -> FUN_6f05f710(1)
//   0x6f05f710  ENGINE LOOP (564 B, while(true), thread named "Engine %x")
//   0x6f30ff00  frame step (1148 B, __thiscall, 25 ms fixed step)
//   0x6f310f20  step inner (47 B): pump(1), else FUN_6f30bac0, pump(1)
//   0x6f30bac0  turn apply (1530 B, commands 0x10-0x31)
//   0x6f312a40  turn catch-up (445 B; max 1021 turns, max 5 calls/frame)
//   0x6f312740  SetGameState (133 B; state field +0x270, states 0-6)
//   0x6f312490  POOL/NONE branch (47 B) -> FUN_6f310f50 or virtual call
//   0x6f30ada0  event delivery double virtual call (61 B)
//   0x6f05e410  EvtSched init (740 B; EvtThread entries 0x18 B each)
//   0x6f05ae90  event-queue trigger (493 B; virtual (**entry+8))())
//   0x6f0603b0  timer-queue dispatch (326 B; GetTickCount deadlines)
//   0x6f05b080  event-list processing (349 B)
//   0x6f05fc10  frame-delta dt (136 B; dt = (tick - *(obj+0x40)) * _DAT_6f956678)
//   0x6f059b00  message-pump driver (147 B) / 0x6f1532b0 Win32 pump (462 B)
//   0x6f05fb10  frame callback incl. virtual call (**param_1)(1) (251 B)
//   0x6f154d30  GetTickCount wrapper (6 B; used by GameMain, frame step,
//               engine loop, timer queue, dt calc, ...)
//   0x6f154fb0  20 ms busy-wait (91 B; Sleep(0) loop)
//   0x6f154c30  CpuTicksPerSecond calibration (223 B; QPC)
//   0x6f155010  time subsystem init (146 B; "W32\Time.cpp" line 0x273)

// ---- GameMain call order (S2 §2, verified) ----------------------------------
//   1. FUN_6f02bee0() -- process hardening (deny ACE).
//   2. GetTickCount() via FUN_6f154d30.
//   3. FUN_6f154940() / FUN_6f154630() / FUN_6f153f60() -- small init;
//      FUN_6f154630 is an 8-slot (key, callback) registry at globals
//      DAT_6fbdab98 / DAT_6fbdab9c; FUN_6f153e30 unregisters by key.
//      WHAT callbacks are registered is UNVERIFIED.
//   4. Ordinal_503(buf, &DAT_6f951e28, 0x104) + Ordinal_585(buf) -- string
//      copy; DAT_6f951e28 contents UNVERIFIED.
//   5. FUN_6f027d40() -- error reporting; SErrSetBlizzardErrorFunction
//      (FUN_6f01a310).
//   6. FUN_6f027fb0() -- init; on success thunk_FUN_6f05e710() ->
//      FUN_6f05e710() -> FUN_6f05f710(1) (engine loop), then FUN_6f027660()
//      shutdown + FUN_6f153e30() unregister.
//   7. FUN_6f771125() epilogue.

// ---- Frame-step constants (verified, S2 §7) ---------------------------------
#define FRAME_STEP_MS        25     // fixed step (0x19)
#define FRAME_ACCUM_MAX_MS   4000   // time-accumulator clamp
#define FRAME_WATCHDOG_MS    200    // per-round wall-clock watchdog
#define FRAME_CATCHUP_MAX    1021   // max turns read per catch-up (0x3fd)
#define FRAME_CATCHUP_CALLS  5      // max catch-up calls per frame
#define FRAME_SPEED_SLOTS    7      // speed table DAT_6f987f14 index 0-6
// Speed-table VALUES are UNREAD (UNVERIFIED). GameTick pacing: state 4/5
// emits CNetEventGameTick (0x40090092) with iVar4*5 deliveries per step
// (FUN_6f2877b0 / FUN_6f2877c0 multipliers, identities UNVERIFIED);
// state 5 also calls FUN_6f312740(6).

// ---- Engine-loop object fields (FUN_6f05f710 timing object, S2 §4) ----------
// Offsets evidenced; the OBJECT IDENTITY is UNVERIFIED.
//   +0x28 last tick; +0x2c mode (== 1 -> FUN_6f05dce0 + FUN_6f05fb10);
//   +0x3c deadline; +0x40 frame-start tick; +0x44 flags (bit1 started,
//   bit2 message pump on, bit4 frame-delta pending); +0x48/+0x4c wait
//   budget; +0x50/+0x54/+0x58 scheduler params; +0x1f8/+0x1fc timer queue;
//   +0x1b8/+0x1bc event list.

// ---- EvtSched (S2 §6) ---------------------------------------------------------
// Source clues: "EvtSched.cpp" (FUN_6f05e410, Ordinal_401(4, "EvtSched.cpp",
// 0x311, 0)), "s___AUEvtThread___", "s__PAVCOsThread___", "EvtSched#%d".
//   FUN_6f05e410 (740 B): thread-slot count rounded up to a power of 2
//     (DAT_6fbb8930); EvtThread array entries 0x18 B (FUN_6f771ab4 init,
//     callbacks FUN_6f157040 / FUN_6f1571e0); per-thread pointer table
//     DAT_6fbb896c; DAT_6fbb8978 = FUN_6f05e790(); threads created via
//     FUN_6f157530 (_beginthreadex wrapper, 45 B) named "EvtSched#%d".
//   FUN_6f05e790 (436 B): get/build an EvtThread (Ordinal_401(0x38,
//     "s___AUEvtThread___", ...)); field +0xd = 0x30; global list
//     PTR_LOOP_6fb598ec; refcount +0xc.
//   FUN_6f05ee90 (482 B): schedule wait/reorder (heap reorder on
//     +0x3c/+0x54/+0x58 comparisons).
//   FUN_6f05dce0 (166 B): thread accounting.
//   FUN_6f05dac0 (527 B): unregister (refcount +0xc; release via
//     FUN_6f05f0b0 / FUN_6f05d100 / Ordinal_403 "s___AUEvtThread___").
//   FUN_6f05f290 (287 B): priority-queue (heap) delete.
//   FUN_6f05f140 (129 B): COsThread* array growth (Ordinal_405 realloc).
//   FUN_6f159b00 (341 B): thread registration (TLS; "W32\OsCall.cpp" 0xe5).

// ---- Timing (S2 §5) -----------------------------------------------------------
// The loop times with GetTickCount (FUN_6f154d30 wrapper), NOT timeGetTime:
// WINMM imports only timeGetTime and its only xref is the IAT entry
// (0x6f94e904) -- NO verified call site in code (UNVERIFIED whether it is
// called at all). QPC calibration via FUN_6f154c30; lazy QPC path
// FUN_6f154d40; timestamp FUN_6f155130 (rdtsc wrapper FUN_6f154d20, 3 B).

#endif // WARCRAF_CMAINLOOP_H
