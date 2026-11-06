// NetRouter.h -- Net::NetRouter / Net::NetClient (game + file-info routing).
//
// Evidence:
//   - ~/workspace/warcraf_framesync_analysis.md (S0): Net::NetRouter /
//     Net::NetClient RTTI (收发、主机交接、延迟设置); "Router is running in
//     lockstep mode" (FUN_6f283540).
//   - ~/workspace/warcraf_stage_network.md (S1): router setup FUN_6f283540
//     (612 B, bounded copies via SStrCopy: name 0x20, password 0x10),
//     RouterHandoff events 0x26-0x2a, RouterUnresponsive/Responsive 0x29/0x2a,
//     DistFile events 0x2b-0x2d, SetTurnsLatency event 0x21.
//   - ~/workspace/warcraf_stage_battlenet.md (S10): OsNet transport objects
//     (NETCONN / TCPLISTEN / NETCONNECT / LOOPCONN / NETSELSOCKPTR).
//
// The internal layouts of NetRouter / NetClient are UNVERIFIED (no field
// offsets were recovered for them). Only the behaviour-level facts below are
// evidenced. Do not treat the placeholder structs as real layouts.

#ifndef WARCRAF_NETROUTER_H
#define WARCRAF_NETROUTER_H

#include "warcraf/core/Types.h"
#include "warcraf/net/CNetData.h"

// ---- Router setup (FUN_6f283540, 612 B, S1) --------------------------------
// Assembles connection parameters by flag bits:
//   0x200        reuse existing connection;
//   0x10/0x400000 set -> parameter 6 is fixed to 0xc;
//   0x80000000   controlled by param_14.
// Bounded copies via Storm SStrCopy (ordinal 501): name 0x20, password 0x10.
// References the string "Router is running in lockstep mode".
// Callers (call sites, scenarios UNVERIFIED): FUN_6f2c3160, FUN_6f283320,
// FUN_6f281330.
//   FUN_6f283540 -- NetRouter setup (address only; signature UNVERIFIED).

// ---- Router handoff / responsiveness events (S0/S1) ------------------------
//   0x26 RouterHandoffSearching, 0x27 RouterHandoffSyncing,
//   0x28 RouterHandoffDone, 0x29 RouterUnresponsive, 0x2a RouterResponsive.
// Host handoff and peer-liveness detection. Payload formats UNVERIFIED.

// ---- Map distribution (DistFile, S0/S1) ------------------------------------
//   0x2b DistFileStart, 0x2c DistFileProgress, 0x2d DistFileComplete.
// Guarantees identical starting state (same map on all clients). The file
// transfer itself goes through the Net layer; chunk format UNVERIFIED.

// ---- Turn latency (S1) -----------------------------------------------------
// Event 0x21 CNetEventSetTurnsLatency: turn delay is tunable to absorb
// network jitter. The numeric conversion (SetTurnsLatency) is UNVERIFIED.

// ---- Transport objects (OsNet namespace, S10) ------------------------------
// RTTI names only (no layouts recovered):
//   OsNet::NETCONN, OsNet::TCPLISTEN, OsNet::NETCONNECT, OsNet::LOOPCONN,
//   OsNet::NETSELSOCKPTR.
// LOOPCONN is the loopback connection object (cf. NET_MODE_LOOP in
// CNetData.h). Connection establishment / handshake internals UNVERIFIED.
// SNet itself is NOT in Game.dll's Storm import table (S1 correction);
// transport goes via WS2_32.dll (26 ordinal imports) + internal Net code.

// ---- Placeholder declarations (layouts UNVERIFIED) -------------------------
// These structs exist only so code can name the types; their members are
// placeholders. Do NOT use sizeof() or field offsets from these structs.
struct NetRouter { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct NetClient { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
// Net::ClientTurn / ClientTurnsSync / TurnsSyncChunk / CTurnsSyncStore:
// see warcraf/net/CNetData.h.

#endif // WARCRAF_NETROUTER_H
