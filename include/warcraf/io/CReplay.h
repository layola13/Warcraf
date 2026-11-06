// CReplay.h -- replay recording / playback (.w3g) and save-game classes.
//
// Evidence: ~/workspace/warcraf_stage_replay.md (S9).
//   - Recording = input log. TempReplay.w3g written from GameStart (event
//     0x1b); flag +0x1b44 (in the session object; see net/CNetData.h).
//   - Loader FUN_6f2da7f0 (547 B, path Replay\LastReplay.w3g, version check
//     constants 0x1b and 0x17ab -- field meanings UNVERIFIED).
//   - Filename builder FUN_6f30b530 ("%s_%s_replay" + ".w3g"); save dialog
//     FUN_6f2beba0; autosave FUN_6f2902c0 (AUTOSAVE_REPLAY_INFO).
//   - TReplayUnit + TSExplicitList / TSHashTable<TReplayUnit, HASHKEY_DWORD>.
//   - Command classes (RTTI names, S9). Save classes (RTTI names, S9).
//   - JASS native DoNotSaveReplay.
//
// The .w3g file format details, the replay writer identity (FUN_6f28b570 /
// FUN_6f2c4c30 candidates, UNVERIFIED), and all class layouts are UNVERIFIED.

#ifndef WARCRAF_CREPLAY_H
#define WARCRAF_CREPLAY_H

#include "warcraf/core/Types.h"

// ---- Version constants (S9 -- meanings UNVERIFIED) --------------------------
// The loader FUN_6f2da7f0 checks two version constants, 0x1b and 0x17ab.
// Which field each belongs to (header version vs data version) is
// UNVERIFIED. Note 0x17ab is also the S11 rand-checksum XOR constant --
// same value, different use; no link proven.
#define REPLAY_VERSION_A  0x1b
#define REPLAY_VERSION_B  0x17ab

// ---- File names (verified, S9) ----------------------------------------------
//   TempReplay.w3g -- recorded from GameStart (event 0x1b).
//   Replay\LastReplay.w3g -- loader input path (FUN_6f2da7f0).
//   "%s_%s_replay.w3g" -- saved-replay filename pattern (FUN_6f30b530).
//   AUTOSAVE_REPLAY_INFO -- autosave marker (FUN_6f2902c0).

// ---- Replay unit tracking (RTTI names, S9) ----------------------------------
//   TReplayUnit -- per-unit replay record (layout UNVERIFIED).
//   TSExplicitList / TSHashTable<TReplayUnit, HASHKEY_DWORD> -- containers
//   holding TReplayUnit entries (layouts UNVERIFIED).
struct TReplayUnit { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};

// ---- Replay / save command classes (RTTI names only, S9) --------------------
//   CNetCommandReplayBase, CNetCommandReplayDataCompressed,
//   CNetCommandReplayDataUncompressed, CNetCommandReplayDataDone,
//   CNetCommandReplayViewSpeedIncrement, CNetCommandReplayViewSpeedDecrement,
//   CNetCommandSave, CNetCommandSaveDone, CNetEventSaveComplete.
// ---- Save-game classes (RTTI names only, S9) -------------------------------
//   CBaseSaveLoad, CSave, CSaveGame, CGameSaveEventData.
// Layouts of all the above are UNVERIFIED.

// ---- Key functions (verified addresses, S9) ---------------------------------
//   FUN_6f2da7f0  replay loader (547 B)
//   FUN_6f30b530  replay filename builder ("%s_%s_replay" + ".w3g")
//   FUN_6f2beba0  save dialog
//   FUN_6f2902c0  autosave (AUTOSAVE_REPLAY_INFO)
//   FUN_6f28b570 / FUN_6f2c4c30  replay-writer candidates -- UNVERIFIED

#endif // WARCRAF_CREPLAY_H
