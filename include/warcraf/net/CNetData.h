// CNetData.h -- network session state, events, and turn synchronization.
//
// Evidence:
//   - ~/workspace/warcraf_framesync_analysis.md (S0): event pump FUN_6f30ade0,
//     dispatcher FUN_6f3098d0, TurnsSync model (ClientTurn / ClientTurnsSync /
//     TurnsSyncChunk / CTurnsSyncStore), rolling checksum (rotl3 + 0x7a2d).
//   - ~/workspace/warcraf_stage_network.md (S1): CNetEvent layout, per-player
//     table stride 0x304, TurnsSync in/out FUN_6f310c80, turn-packet parse
//     FUN_6f30b1a0, SetRecordState / SetReplayState, mode tags LOOP/NONE.
//   - ~/workspace/warcraf_stage_mainloop.md (S2): session offsets used by the
//     frame step FUN_6f30ff00, turn apply FUN_6f30bac0, SetGameState.
//   - ~/workspace/warcraf_stage_rng.md (S11): rand checksum stored at +0x1c64.
//   - ~/workspace/warcraf_architecture_complete.md (S12): cross-stage offset
//     consistency (no conflicts across S0/S1/S2/S11).
//
// The full CNetData object layout is UNVERIFIED (S1 unvalidated point 7): only
// the offsets listed below are evidenced. Everything between them is a
// field_XX placeholder. The object is at least 0x2300 bytes (highest verified
// offset +0x22c0 is a counter; the per-player table reaches +0x225c+... --
// total size NOT verified, no /* size */ claim is made here).

#ifndef WARCRAF_CNETDATA_H
#define WARCRAF_CNETDATA_H

#include "warcraf/core/Types.h"

// ---- CNetEvent (24 bytes = 0x18, verified S0/S1) --------------------------
// Event objects are heap-allocated (released by FUN_6f307880 via
// FUN_6f78a5c0(event, 0x18, "...CNetEvent_CNetData...", -2)) and linked into
// a list whose head is the session field at +0xf40 (next at +0xf3c/+4, S1).
// Event types 0x01-0x31 are game events, >= 0x40 are Battle.net events;
// a type > 0x3f is clamped to 0x32 at the big dispatcher entry (S1).
struct CNetEvent { /* size = 24 */
    _DWORD field_0;
    _DWORD field_4;
    void*  payload;    // +0x08: event payload pointer
    _DWORD length;     // +0x0c: payload length
    _DWORD field_10;
    _BYTE  type;       // +0x14: event type byte (0x01-0x31 game, >=0x40 BN)
    _BYTE  flag;       // +0x15: flag byte
    _BYTE  field_16;
    _BYTE  field_17;
};

// Game event types (FUN_6f3098d0 dispatch table, S0/S1 -- verified).
enum CNetEventType {
    NETEVENT_CONNECT            = 0x01,
    NETEVENT_DISCONNECT         = 0x02,
    NETEVENT_GAMELIST_START     = 0x03,
    NETEVENT_GAMELIST_STOP      = 0x04,
    NETEVENT_GAMELIST_ERROR     = 0x05,
    NETEVENT_GAMELIST_ADD       = 0x06,
    NETEVENT_GAMELIST_UPDATE    = 0x07,
    NETEVENT_GAMELIST_DELETE    = 0x08,
    NETEVENT_TEAM_GAMELIST_START  = 0x09,
    NETEVENT_TEAM_GAMELIST_STOP   = 0x0a,
    NETEVENT_TEAM_GAMELIST_ADD    = 0x0b,
    NETEVENT_TEAM_GAMELIST_UPDATE = 0x0c,
    NETEVENT_TEAM_GAMELIST_DELETE = 0x0d,
    NETEVENT_ANON_GAME_FIND     = 0x0e,
    NETEVENT_ANON_GAME_JOIN     = 0x0f,
    NETEVENT_GAME_CREATE        = 0x10,
    NETEVENT_GAME_AD            = 0x11,
    NETEVENT_TEAM_GAME_AD       = 0x12,
    NETEVENT_TEAM_INFO          = 0x13,
    NETEVENT_GAME_FIND          = 0x14,
    NETEVENT_GAME_JOIN          = 0x15,
    NETEVENT_PLAYER_JOIN        = 0x16,
    NETEVENT_PLAYER_LEAVE       = 0x17,
    NETEVENT_PLAYER_READY       = 0x18,
    NETEVENT_GAME_SETUP         = 0x19,
    NETEVENT_GAME_CLOSE         = 0x1a,
    NETEVENT_GAME_START         = 0x1b,  // also starts writing TempReplay.w3g (S9)
    NETEVENT_GAME_READY         = 0x1c,
    NETEVENT_PLAYER_UPDATE      = 0x1d,
    NETEVENT_TURNSYNC_OUT       = 0x1e,  // turn-sync data out -> FUN_6f310c80
    NETEVENT_TURNSYNC_IN        = 0x1f,  // turn-sync data in  -> FUN_6f310c80
    NETEVENT_TURN_PACKET        = 0x20,  // turn packet -> FUN_6f30b1a0
    NETEVENT_SET_TURNS_LATENCY  = 0x21,
    NETEVENT_TURN_APPLY_1       = 0x22,  // turn apply + checksum step (FUN_6f3048c0)
    NETEVENT_TURN_APPLY_2       = 0x23,  // turn apply + checksum step (FUN_6f304680)
    NETEVENT_GAME_SUSPEND       = 0x24,
    NETEVENT_PLAYER_RESUME      = 0x25,
    NETEVENT_ROUTER_HANDOFF_SEARCHING = 0x26,
    NETEVENT_ROUTER_HANDOFF_SYNCING   = 0x27,
    NETEVENT_ROUTER_HANDOFF_DONE      = 0x28,
    NETEVENT_ROUTER_UNRESPONSIVE      = 0x29,
    NETEVENT_ROUTER_RESPONSIVE        = 0x2a,
    NETEVENT_DIST_FILE_START    = 0x2b,
    NETEVENT_DIST_FILE_PROGRESS = 0x2c,
    NETEVENT_DIST_FILE_COMPLETE = 0x2d,
    NETEVENT_OFFICIAL_PLAYERS   = 0x2e,
    NETEVENT_TRIGGER            = 0x2f,
    NETEVENT_TRUSTED_DESYNC     = 0x30,  // only while in-game and +0xabc != 0
    NETEVENT_TRUSTED_RESULT     = 0x31   // only while in-game and +0xabc != 0
};

// ---- Turn synchronization model (RTTI names, S0) --------------------------
//   Net::ClientTurn      -- one player's command set for one turn.
//   Net::ClientTurnsSync -- one player's turn-sync state.
//   Net::TurnsSyncChunk  -- turn data block as sent on the wire.
//   CTurnsSyncStore      -- assembles received chunks by turn number and
//                           releases the simulation only when all are in.
// Per-player state lives in a table with stride 0x304 (S1/S2); the fields
// below are offsets WITHIN one per-player entry, except where noted.
struct ClientTurnsSync { /* size = 0x304 (stride, verified S1) */
    _DWORD field_0[0x9e];      // +0x00-0x277: UNVERIFIED
    _DWORD playerCounter;      // +0x278: per-player state counter (S1/S2)
    _DWORD field_27C;
    _DWORD field_280;
    _DWORD bytesAccum;         // +0x284: accumulated bytes this turn (S1);
                               // also the "ms remainder vs 25" in the frame
                               // step (S2) -- same offset, both evidenced
    _DWORD field_288[0x16];    // +0x288-0x2df: UNVERIFIED
    _DWORD countdown;          // +0x2e0: decrements per TurnsSync; at zero
                               // calls FUN_6f30b460(slot, 0) (S1; exact
                               // semantics of FUN_6f30b460 UNVERIFIED)
    _DWORD field_2E4[0x8];     // +0x2e4-0x303: UNVERIFIED
};

// Sub-record types inside a turn packet (event 0x20, FUN_6f30b1a0, S1).
// Each maps to one constructor call:
//   0x10 -> FUN_6f2fc4b0, 0x11 -> FUN_6f2fc7d0, 0x12 -> FUN_6f2fc590,
//   0x13 -> FUN_6f2fc710, 0x14 -> FUN_6f2fc650, 0x20 -> FUN_6f2fc3d0.
// The meaning of each sub-type is UNVERIFIED.
enum TurnPacketSubType {
    TURN_SUB_10 = 0x10,
    TURN_SUB_11 = 0x11,
    TURN_SUB_12 = 0x12,
    TURN_SUB_13 = 0x13,
    TURN_SUB_14 = 0x14,
    TURN_SUB_20 = 0x20
};

// ---- CNetData / CNetSession (partial layout, verified offsets only) -------
// The frame step (FUN_6f30ff00), turn apply (FUN_6f30bac0), SetGameState
// (FUN_6f312740) and the event pump (FUN_6f30ade0) all operate on the same
// session object (S2: "offsets consistent with the S1 network object").
// Total size UNVERIFIED. Fields below are placed at their verified offsets;
// gaps are field_XX placeholders (size computed from the next known offset).
struct CNetData {
    _DWORD field_0[0x9c];      // +0x00-0x26b: UNVERIFIED
    _DWORD gameState;          // +0x270: game state 0-6 (FUN_6f312740)
    _DWORD field_274;
    _DWORD frameState;         // +0x278: frame-step gate (< 4 returns; 4/5
                               // emit GameTick). Same offset as the
                               // per-player counter -- different objects
                               // (session vs per-player entry), S12 §4.3.
    _DWORD field_27C;
    _DWORD field_280;
    _DWORD turnMsRemainder;    // +0x284: ms remainder within a turn (25 ms
                               // steps; S2). NOTE: S1 reads +0x284 as the
                               // per-player byte accumulator -- the two
                               // readings are at different base objects.
    _DWORD field_288[0x62];    // +0x288-0x40f: UNVERIFIED
    _DWORD field_410[0x80];    // +0x410-0x60f: UNVERIFIED
    _DWORD localSlot;          // +0x610: local player slot (S1/S2)
    _DWORD replayState;        // +0x614: replay state 0-4 (FUN_6f312930);
                               // 2/3 = turn apply runs (FUN_6f30bac0)
    _DWORD field_618;
    _DWORD field_61C;
    _DWORD field_620;
    _DWORD field_624;
    _DWORD turnCursor;         // +0x628: current turn cursor (S2)
    _DWORD turnApplied;        // +0x62c: applied turn cursor (S2)
    _DWORD field_630[0x64];    // +0x630-0x7bf: UNVERIFIED
    _DWORD field_7C0[0x80];    // +0x7c0-0x9bf: UNVERIFIED
    _DWORD field_9C0[0x6c];    // +0x9c0-0xb6f: UNVERIFIED
    _DWORD field_B70[0x14];    // +0xb70-0xbbf: UNVERIFIED
    _DWORD field_BC0[0x9];     // +0xbc0-0xbe3: UNVERIFIED
    _DWORD recordState;        // +0xbe4: record state (FUN_6f3128d0,
                               // SetRecordState); == 1 calls FUN_6f78b670
                               // per turn-apply round (S2)
    _DWORD field_BE8[0x76];    // +0xbe8-0xdbf: UNVERIFIED
    _DWORD field_DC0[0x80];    // +0xdc0-0xfbf: UNVERIFIED
    _DWORD field_FC0[0x80];    // +0xfc0-0x11bf: UNVERIFIED
    _DWORD field_11C0[0x80];   // +0x11c0-0x13bf: UNVERIFIED
    _DWORD field_13C0[0x80];   // +0x13c0-0x15bf: UNVERIFIED
    _DWORD field_15C0[0x80];   // +0x15c0-0x17bf: UNVERIFIED
    _DWORD field_17C0[0x80];   // +0x17c0-0x19bf: UNVERIFIED
    _DWORD field_19C0[0x80];   // +0x19c0-0x1bbf: UNVERIFIED
    _DWORD field_1BC0[0x19];   // +0x1bc0-0x1c23: UNVERIFIED
    _DWORD field_1C24[0x10];   // +0x1c24-0x1c63: UNVERIFIED
    _DWORD randChecksum;       // +0x1c64: rand checksum (FUN_6f3130c0, S11)
    _DWORD field_1C68;
    _DWORD eventStage;         // +0x1c6c: event delivery stage (1 = GameTick,
                               // 3 = IdleTick, S2)
    _DWORD gameTickAccum;      // +0x1c70: GameTick accumulator (steps of 5, S2)
    _DWORD field_1C74[0x53];   // +0x1c74-0x1dbf: UNVERIFIED
    _DWORD field_1DC0[0x80];   // +0x1dc0-0x1fbf: UNVERIFIED
    _DWORD field_1FC0[0x80];   // +0x1fc0-0x21bf: UNVERIFIED
    _DWORD field_21C0[0x22];   // +0x21c0-0x2247: UNVERIFIED
    _DWORD timeResetFlag;      // +0x2248: time-reset flag (S2)
    _DWORD lastTick;           // +0x224c: previous tick (S2)
    _DWORD timeAccum;          // +0x2250: time accumulator, ms, clamped 4000 (S2)
    _DWORD field_2254;
    _DWORD bandwidthAvg;       // +0x2258: bandwidth sliding average
                               // (old*7 + new) >> 3 (S1)
    _DWORD bytesRemaining;     // +0x225c: global bytes remaining (S1); also
                               // += param_5 & 0xffff on commands 0x1e/0x1f (S3)
    _DWORD field_2260[0x7];    // +0x2260-0x227b: UNVERIFIED
    _DWORD eventChecksum;      // +0x227c: rolling event-stream checksum
                               // accumulator (S0/S1)
    _DWORD field_2280;
    _DWORD field_2284;
    _DWORD modeTag;            // +0x2288: 4-char mode tag: 0x4c4f4f50
                               // "LOOP" (loopback/local) or 0x4e4f4e45
                               // "NONE" (no network) (S1/S2)
    _DWORD field_228C[0x9];    // +0x228c-0x22af: UNVERIFIED
    _DWORD speedFactor;        // +0x22b0: speed factor from table
                               // DAT_6f987f14 (index 0-6, S2; table values
                               // UNREAD -- UNVERIFIED)
    _DWORD speedNum;           // +0x22b4: speed scale numerator (S2)
    _DWORD speedDen;           // +0x22b8: speed scale denominator (S2)
    _DWORD field_22BC;         // +0x22bc: flag (== 0 gates time accumulation, S2)
    _DWORD gameTickCount;      // +0x22c0: GameTick count (steps of 5, wraps
                               // past 250, S2)
    // NOTE: highest verified offset in this object is +0x22c0. The object
    // continues past here (per-player table base, event list at +0xf40 and
    // current event at +0x1b40 are BELOW this point -- see below). Total
    // size is UNVERIFIED and not asserted.
};

// Session fields whose offsets fall BELOW +0x22c0 are documented here rather
// than as struct members, because the struct above is laid out linearly and
// the per-player table / event list live at fixed offsets inside it:
//   +0xf40  event list head (event pump traverses it, S1)
//   +0x1b40 current event being processed (S1)
//   +0x1b44 replay-recording flag (== 1 writes NET_COMMAND_SAVE, S1/S9)
//   +0x1c68 global counter (+1 when player slot is 0, S1)
//   +0x2274 / +0x1c68 context flags (S1 -- exact meaning UNVERIFIED)
//   +0xacc  turn-cursor backup (S2); +0xad4 catch-up throttle counter (< 5)
//   +0x58c  if != 0, turnCursor = backup (S2)
//   +0xabc  TrustedDesync/TrustedResult gate (S1 -- meaning UNVERIFIED)
//   +0x1c78 vtable slot used by FUN_6f312490 (+0x1c virtual call, S2 --
//          target UNVERIFIED)
//   +0x1c70 rand-checksum aggregate base (S11); +0x2f8 rand-checksum switch
// Per-player table (stride 0x304, ClientTurnsSync above) base offset within
// the session is UNVERIFIED -- only the stride and in-entry offsets are known.

// ---- Mode tags / magic (verified) -----------------------------------------
#define NET_MODE_LOOP   0x4c4f4f50  // "LOOP": loopback / local game
#define NET_MODE_NONE   0x4e4f4e45  // "NONE": no network
#define NET_MAGIC_WAR3  0x57415233  // "WAR3" (Reign of Chaos, FUN_6f30e2b0)
#define NET_MAGIC_W3XP  0x57335850  // "W3XP" (Frozen Throne, FUN_6f30e2b0)
// Game type selected by global DAT_6fb592f0 (S1).

// ---- Frame events posted by the frame step (S2) ---------------------------
#define NETEVENT_GAME_TICK        0x40090092  // CNetEventGameTick
#define NETEVENT_IDLE_TICK        0x40090093  // CNetEventIdleTick
#define NETEVENT_ASYNC_IDLE_TICK  0x40090094  // CNetEventAsyncIdleTick
// Payload: one float, seconds (tick count x _DAT_6f956678).

// ---- Game states (FUN_6f312740 SetGameState, S2) --------------------------
// States 0-6 with the verified transition table; the MEANING of each state
// (menu / lobby / loading / in-game / paused ...) is UNVERIFIED.
//   new 0: from any (param_2 == old passes through); 1,2: from 0;
//   3: from 1; 4: from 3 or 5; 5: from 4 or 6; 6: from 5.
// Illegal transitions log via FUN_6f3129f0
// ("CNetData::CNetSession::SetGameState (from %s to %s)") and return 0.
enum CNetGameState {
    GAME_STATE_0 = 0,  // meaning UNVERIFIED
    GAME_STATE_1 = 1,  // meaning UNVERIFIED
    GAME_STATE_2 = 2,  // meaning UNVERIFIED
    GAME_STATE_3 = 3,  // meaning UNVERIFIED
    GAME_STATE_4 = 4,  // meaning UNVERIFIED
    GAME_STATE_5 = 5,  // meaning UNVERIFIED
    GAME_STATE_6 = 6   // meaning UNVERIFIED
};

// ---- Checksum format strings (S0/S1) --------------------------------------
// Seven subsystem checksums, format strings at 0x6f987de8-0x6f987e6b:
//   "%scheat / %scombat / %sgame / %snet / %srand / %stempest / %sunit
//    checksum %08x"
// The strings have NO pointer hits in the binary and the print functions
// (FUN_6f3079f0 syncNetEventChecksum, FUN_6f231cd0 fog) have NO static
// callers -- they are reached via callback/vtable registration (S1).
// Their hash functions are UNVERIFIED.
// Rolling event checksum (FUN_6f30ade0, inlined):
//   per dword: acc = rotl(acc ^ word, 3); tail bytes handled per byte;
//   then folded per byte x 0x7a2d into the +0x227c accumulator.
#define CHECKSUM_FMT_BASE  0x6f987de8

// ---- Key functions (verified addresses, S0/S1/S2) -------------------------
//   FUN_6f30ade0  event pump (954 B; callers FUN_6f30ff00 / FUN_6f310f20)
//   FUN_6f3098d0  game event dispatcher (5074 B, 0x01-0x31, 73 callees)
//   FUN_6f307b50  Battle.net event dispatcher (2625 B, types >= 0x40)
//   FUN_6f307980  event pre-dispatch (38 B); FUN_6f307880 post-dispatch (62 B)
//   FUN_6f310c80  TurnsSync in/out (435 B)
//   FUN_6f30b1a0  turn-packet parse, event 0x20 (654 B)
//   FUN_6f2feb10  event send (via FUN_6f311750, CDataStoreCache<1460>)
//   FUN_6f2fd0d0  per-turn command log + dispatch ("War3Net: (INFO): turn
//                 %u: %s from %u"; NET_COMMAND_SAVE path)
//   FUN_6f30e2b0  WAR3/W3XP magic (21 B)
//   FUN_6f3128d0  CNetData::SetRecordState (+0xbe4)
//   FUN_6f312930  CNetData::SetReplayState (+0x614, 0->1->2->3->4)
//   FUN_6f283540  router setup (612 B; "Router is running in lockstep mode")
//   FUN_6f3079f0  syncNetEventChecksum print (59 B, callback-registered)
//   FUN_6f231cd0  fog checksum print (43 B, callback-registered)
//   FUN_6f3048c0  13-byte forward stub (event 0x22 path)
//   FUN_6f304680  conditional free via SMemFree (ord 403) of +0x70 handle
//   FUN_6f30ada0  event delivery double virtual call (61 B)
//   FUN_6f310f20  step inner: pump(1), else FUN_6f30bac0, pump(1) again (47 B)
//   FUN_6f2f20e0  7-byte stub, 250 callers -- identity UNVERIFIED
// ---- Compile-time layout checks (verified offsets only) -------------------
// These assert the pinned sizes/offsets above; they fail the build if a
// later edit moves a field. They check ONLY what the stage docs evidence.
#include <cstddef>
// CNetEvent contains a pointer, so its asserts only fire on a 32-bit target
// (see WARCRAF_32BIT_LAYOUT in core/Types.h). ClientTurnsSync and CNetData
// are all-_DWORD and are checked on any host.
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CNetEvent) == 24, "CNetEvent must be 24 bytes");
static_assert(offsetof(CNetEvent, payload) == 0x08, "CNetEvent payload +0x08");
static_assert(offsetof(CNetEvent, length) == 0x0c, "CNetEvent length +0x0c");
static_assert(offsetof(CNetEvent, type) == 0x14, "CNetEvent type +0x14");
static_assert(offsetof(CNetEvent, flag) == 0x15, "CNetEvent flag +0x15");
#endif
static_assert(sizeof(ClientTurnsSync) == 0x304, "ClientTurnsSync stride 0x304");
static_assert(offsetof(ClientTurnsSync, playerCounter) == 0x278, "playerCounter +0x278");
static_assert(offsetof(ClientTurnsSync, bytesAccum) == 0x284, "bytesAccum +0x284");
static_assert(offsetof(ClientTurnsSync, countdown) == 0x2e0, "countdown +0x2e0");
static_assert(sizeof(CNetData) == 0x22c4, "CNetData must end at 0x22c4");
static_assert(offsetof(CNetData, gameState) == 0x270, "gameState +0x270");
static_assert(offsetof(CNetData, localSlot) == 0x610, "localSlot +0x610");
static_assert(offsetof(CNetData, replayState) == 0x614, "replayState +0x614");
static_assert(offsetof(CNetData, recordState) == 0xbe4, "recordState +0xbe4");
static_assert(offsetof(CNetData, randChecksum) == 0x1c64, "randChecksum +0x1c64");
static_assert(offsetof(CNetData, timeAccum) == 0x2250, "timeAccum +0x2250");
static_assert(offsetof(CNetData, bandwidthAvg) == 0x2258, "bandwidthAvg +0x2258");
static_assert(offsetof(CNetData, bytesRemaining) == 0x225c, "bytesRemaining +0x225c");
static_assert(offsetof(CNetData, eventChecksum) == 0x227c, "eventChecksum +0x227c");
static_assert(offsetof(CNetData, modeTag) == 0x2288, "modeTag +0x2288");
static_assert(offsetof(CNetData, gameTickCount) == 0x22c0, "gameTickCount +0x22c0");

#endif // WARCRAF_CNETDATA_H
