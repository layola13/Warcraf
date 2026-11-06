// TurnsSync.h -- lockstep turn synchronization.
//
// Evidence:
//   - FUN_6f310c80 (TurnsSync in/out, 435 B), FUN_6f30b1a0 (turn-packet
//     parse, event 0x20, 654 B), FUN_6f2fd0d0 (per-turn command log,
//     59 B): all decompiled in ~/workspace/warcraf_ghidra/net_decomp.txt.
//   - ~/workspace/warcraf_stage_network.md (S1) section 4 (TurnsSync
//     pipeline) and section 3 (event types 0x1e/0x1f/0x20).
//   - RTTI names Net::ClientTurn / Net::ClientTurnsSync /
//     Net::TurnsSyncChunk / CTurnsSyncStore (S0): a ClientTurn is one
//     player's command set for one turn; the store assembles received
//     chunks by turn number and releases the simulation only when all
//     are in.
//   - The 25 ms fixed step: S2 (frame step FUN_6f30ff00); the demo steps
//     the frame every 25 ms.
//
// What is implemented here, and what is a gap:
//   - TurnsSyncAccounting mirrors FUN_6f310c80 exactly, including the
//     arithmetic: slot 0 bumps the global counter (+0x1c68); the byte
//     count (from FUN_6f3086a0 -- internals NOT decompiled, see below)
//     is added to the per-player accumulator (+0x284) and subtracted
//     from the global budget (+0x225c); when the flag is set, the
//     bandwidth average (+0x2258) becomes (old*7 + bytes) >> 3 and the
//     countdown (+0x2e0) becomes max(countdown, bytes) - bytes, calling
//     back at zero; the advance and local-player actions run under the
//     decompile's conditions.
//   - The byte count source FUN_6f3086a0(data, slot) is an EVIDENCE GAP:
//     the function is not in our decompiles. AccountTurnData therefore
//     takes the byte count as a parameter (the caller measures the
//     chunk it received). A zero byte count forces the flag off, as in
//     the original.
//   - FUN_6f30b460(slot, 0) (countdown-zero), FUN_6f3130c0 (advance),
//     FUN_6f3023c0 / FUN_6f304920 / FUN_6f840bd0 / FUN_6f3041c0 (local
//     advance actions): addresses known, identities unknown. They are
//     callbacks here, named by their evidenced role; the demo logs them.
//   - ParseTurnPacket implements the evidenced skeleton of FUN_6f30b1a0:
//     a 3-byte header (first byte: meaning unknown; next two bytes: the
//     sub-record section length, little-endian), then a stream of
//     sub-records each starting with a type byte (0x10-0x14, 0x20).
//     The six per-type constructors (FUN_6f2fc4b0/6f2fc7d0/6f2fc590/
//     6f2fc710/6f2fc650/6f2fc3d0) are NAMING GAPS: their identities and
//     their per-type payload layouts are unknown, so the parser cannot
//     consume payload bytes for them. A sub-record is reported with its
//     type and constructor address, and parsing stops with status
//     kPayloadLayoutGap unless the caller registered a handler that
//     states how many bytes that sub-record consumes (the demo does
//     this for its bare type-byte packets -- see apps/net_demo).
//   - LogTurnCommand mirrors FUN_6f2fd0d0's log line exactly:
//     "War3Net: (INFO): turn %u: %s from %u\n". The original prints the
//     event's flag byte (+0x15) as the "from" value; this function takes
//     the source slot explicitly and notes the difference.

#ifndef WARCRAF_NET_TURNS_SYNC_H
#define WARCRAF_NET_TURNS_SYNC_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "warcraf/net/NetDataStore.h"
#include "warcraf/net/NetSession.h"

namespace warcraf::net {

// One player's command set for one turn (Net::ClientTurn, RTTI name).
// The commands travel as an encoded turn chunk (see NetCommandCodec.h).
struct ClientTurn {
    uint32_t turn = 0;
    uint8_t slot = 0;
    DataStore chunk;  // encoded commands for this turn
};

// Assembles received turn chunks by turn number (CTurnsSyncStore, RTTI
// name). A turn is released only when every expected slot has submitted
// it -- the lockstep rule (S0).
class TurnsSyncStore {
public:
    explicit TurnsSyncStore(std::vector<uint8_t> expectedSlots);

    // Submits one player's chunk for one turn. Returns false (and keeps
    // nothing) on a duplicate submission -- duplicate handling is NOT
    // evidenced in the original; refusing the duplicate is the safe
    // behaviour and is documented as a reconstruction choice.
    bool SubmitTurn(uint8_t slot, uint32_t turn, DataStore chunk);

    // True when every expected slot has submitted this turn.
    bool IsTurnComplete(uint32_t turn) const;

    // Takes a complete turn out of the store, in slot order. The caller
    // must check IsTurnComplete first; taking an incomplete turn returns
    // an empty vector.
    std::vector<ClientTurn> TakeTurn(uint32_t turn);

    // Lowest turn number still waiting for at least one slot, or
    // kNoPendingTurn when nothing is buffered.
    static constexpr uint32_t kNoPendingTurn = 0xffffffff;
    uint32_t LowestPendingTurn() const;
    size_t BufferedTurnCount() const { return turns_.size(); }

private:
    std::vector<uint8_t> expectedSlots_;
    std::map<uint32_t, std::map<uint8_t, ClientTurn>> turns_;
};

// Per-turn accounting, mirroring FUN_6f310c80 (TurnsSync in/out).
class TurnsSyncAccounting {
public:
    // Countdown-zero callback: FUN_6f30b460(slot, 0). The original's
    // target behaviour (player ready / timeout handling) is UNVERIFIED
    // (S1 unvalidated point 4).
    std::function<void(uint8_t slot)> onCountdownZero;
    // Advance-turn callback: FUN_6f3130c0. Inferred role: advance the
    // turn (S1: "推进回合，推断").
    std::function<void()> onAdvanceTurn;
    // Local-player data-ready callback: FUN_6f3023c0, FUN_6f304920,
    // FUN_6f840bd0, FUN_6f3041c0, run in that order when the chunk's
    // slot is the local slot and the flag is set. Four unidentified
    // functions; the callback stands in for all four.
    std::function<void()> onLocalTurnData;

    // Accounts one received (or sent) turn chunk, exactly as FUN_6f310c80
    // does. `byteCount` is the chunk's byte count -- in the original it
    // comes from FUN_6f3086a0(data, slot), whose internals are an evidence
    // gap (see the header comment); the caller supplies the measured size.
    // A zero byte count forces `flag` off, as in the original.
    void AccountTurnData(NetSession& session, uint8_t slot,
                         uint32_t byteCount, bool flag);
};

// ---- Turn-packet parsing (FUN_6f30b1a0, event 0x20) ------------------------

// Sub-record types inside a turn packet, with the constructor each maps
// to (S1). The meaning of each sub-type is UNVERIFIED; the constructor
// identities are NAMING GAPS (see TurnsSync.h header comment).
enum class TurnSubRecordType : uint8_t {
    kSub10 = 0x10,  // constructor FUN_6f2fc4b0
    kSub11 = 0x11,  // constructor FUN_6f2fc7d0
    kSub12 = 0x12,  // constructor FUN_6f2fc590
    kSub13 = 0x13,  // constructor FUN_6f2fc710
    kSub14 = 0x14,  // constructor FUN_6f2fc650
    kSub20 = 0x20,  // constructor FUN_6f2fc3d0
};

// Returns the (unidentified) constructor address for a sub-record type,
// or 0 for an unknown type.
uint32_t TurnSubRecordConstructorAddress(uint8_t subType);

struct ParsedTurnSubRecord {
    uint8_t type = 0;                // sub-record type byte
    uint32_t constructorAddress = 0; // FUN_6f2fcxxx (identity unknown)
    size_t payloadBytes = 0;         // bytes the handler consumed (0 if none)
};

enum class TurnPacketStatus {
    kComplete,         // all sub-records parsed, position == declared length
    kPayloadLayoutGap, // stopped at a sub-record whose payload layout is
                       // unknown and has no registered handler
    kTruncated,        // packet ends before the declared length
    kCountMismatch,    // parsed bytes ran past the declared length
    kUnknownSubType,   // a sub-record type byte is not 0x10-0x14/0x20
};

struct TurnPacketParseResult {
    TurnPacketStatus status = TurnPacketStatus::kTruncated;
    uint8_t headerByte = 0;      // first header byte; meaning UNVERIFIED
    uint16_t declaredLength = 0; // sub-record section length (header +1..+2)
    std::vector<ParsedTurnSubRecord> subRecords;
};

// Parses a turn packet per the FUN_6f30b1a0 skeleton (see header comment).
// `subRecordHandler`, when set, is called for each sub-record type and
// must return how many payload bytes that sub-record consumes (the
// original's per-type constructors read their own payloads from the
// store; their sizes are not evidenced). With no handler, parsing stops
// at the first sub-record with kPayloadLayoutGap.
TurnPacketParseResult ParseTurnPacket(
    const uint8_t* data, size_t length,
    const std::function<size_t(uint8_t subType)>& subRecordHandler = nullptr);

// Per-turn command log (FUN_6f2fd0d0, 59 B, net_decomp.txt):
// prints exactly "War3Net: (INFO): turn %u: %s from %u\n". The original
// prints the event's flag byte (+0x15) as "from"; this function takes the
// source slot explicitly (noted here, not hidden). The original then
// delivers the event via FUN_6f30ada0 unless the flag is 0xff -- delivery
// stays with the caller.
void LogTurnCommand(uint32_t turn, const std::string& text, uint8_t fromSlot);

}  // namespace warcraf::net

#endif  // WARCRAF_NET_TURNS_SYNC_H
