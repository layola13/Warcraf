// CBattleNet.h -- Battle.net client: connection states, events, lobby.
//
// Evidence: ~/workspace/warcraf_stage_battlenet.md (S10).
//   - Three layers: OsNet + BNCS, two dispatchers, Glue UI.
//   - CONNSTATE table: 13 state names at 0x6fa5c7f0 (order-inferred, NO code
//     references -- the reading function is UNVERIFIED).
//   - OsNet RTTI: NETCONN / TCPLISTEN / NETCONNECT / LOOPCONN / NETSELSOCKPTR.
//   - Battle.net dispatcher FUN_6f307b50 (types 0x41-0x80, full table in S10).
//   - Login screen FUN_6f2b9d60 (3132 B); main menu FUN_6f2b7560 (4146 B,
//     15 buttons); CAdManager FUN_6f2cf9b0; ladder URL.
//   - Gateway domain NOT in Game.dll (registry Software\Battle.net\ in
//     Storm.dll).
//
// The CONNSTATE driver function, the BNCS handshake packet format, the lobby
// event-callback bodies, the matchmaking algorithm, and all class layouts
// are UNVERIFIED.

#ifndef WARCRAF_CBATTLE_NET_H
#define WARCRAF_CBATTLE_NET_H

#include "warcraf/core/Types.h"

// ---- CONNSTATE (13 states, S10) --------------------------------------------
// Names stored consecutively from 0x6fa5c7f0; the index <-> state mapping is
// ORDER-INFERRED (no code references to the table exist -- the names are
// presumably read via pointer-table / computed addressing). Treat the
// numbering as an inference, not a verified mapping. "Unknown CONNSTATE" at
// 0x6fa5c948.
enum ConnState {
    CONNSTATE_NONE                      = 0,   // not connected
    CONNSTATE_DISCONNECTING             = 1,   // disconnecting
    CONNSTATE_CHAT_CONNECTING           = 2,   // chat server connecting
    CONNSTATE_AUTH_INFO                 = 3,   // auth info exchange
    CONNSTATE_AUTH_CHECK                = 4,   // auth check
    CONNSTATE_CONNECTED                 = 5,   // connected (not logged in)
    CONNSTATE_UPGRADE_REQUIRED          = 6,   // client upgrade required
    CONNSTATE_AUTH_ACCOUNTLOGON         = 7,   // account logon request
    CONNSTATE_AUTH_ACCOUNTLOGONPROOF    = 8,   // logon proof (SRP exchange)
    CONNSTATE_AUTH_ACCOUNTCHANGE        = 9,   // password change request
    CONNSTATE_AUTH_ACCOUNTCHANGEPROOF   = 10,  // change proof
    CONNSTATE_AUTH_ACCOUNTCREATE        = 11,  // account creation request
    CONNSTATE_LOGGEDON                  = 12   // logged in
};
#define CONNSTATE_TABLE_BASE  0x6fa5c7f0
#define CONNSTATE_UNKNOWN_STR 0x6fa5c948  // "Unknown CONNSTATE"

// ---- Battle.net event types (FUN_6f307b50 dispatch, S1/S10) -----------------
// Types >= 0x40 (minus 0x40 for the table index), all producing
// CBattleNetEvent* objects: Connect, Disconnect, Logon, Create, Change,
// UpgradeUpdate, DownloadUpgradeFile, CheckAd, QueryAdUrl, NewsStart/Stop/Add,
// FriendsStart/Stop/Add/Remove/Position/Update, ReadUserData, Profile,
// ClanProfile, CheckDataFile, EnterChat, ChannelList, ChatEvent, MessageBox,
// TeamGetPotentials, AnonProfile (+Clan), ClanGetPotentials, ClanCreate,
// ClanFounder, ClanDisband, ClanDesignate, ClanAdded, ClanRemoved,
// ClanMemberAdd/Remove/Invite/Rank, ClanInfoGet, ClanListStart/Stop/Remove/
// Update, ClanChangeRank, TournamentInfo, ScheduledGame, MatchmakingInfo,
// IconList. Special case 0x6b (table index 0x2b): FUN_6f2ffe50(0x400900e2)
// then FUN_6f30ada0 with the data payload. Event 0x43, 0x5c-0x61, 0x63-0x66,
// 0x69-0x6a have no handling branch (S10). Payload formats UNVERIFIED.
enum CBattleNetEventType {
    BNETEVENT_BASE = 0x40  // first Battle.net event type (table index 0)
};
//   FUN_6f307b50 -- Battle.net event dispatcher (2625 B, 56 callees)
//   FUN_6f3098d0 -- lobby/game event dispatcher (0x01-0x31; shared with S0)

// ---- UI entry points (verified addresses/sizes, S10) ------------------------
//   FUN_6f2b9d60  login screen (3132 B; 9 buttons -- count per S10)
//   FUN_6f2b7560  main menu (4146 B; 15 buttons)
//   FUN_6f2cf9b0  CAdManager
//   Ladder URL: http://www.battle.net/war3/ladder/war3-ladder-info-clans.aspx
//   Gateway domain: NOT in Game.dll; Storm.dll holds only the registry path
//   "Software\Battle.net\" -- connection parameters come from the registry /
//   config. BNCS handshake packet format UNVERIFIED.

#endif // WARCRAF_CBATTLE_NET_H
