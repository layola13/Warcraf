// TurnsSync.h -- implementation.
//
// Accounting mirrors FUN_6f310c80; the turn-packet parser mirrors the
// evidenced skeleton of FUN_6f30b1a0; the log line is FUN_6f2fd0d0's
// format string. Behaviour is transcribed; nothing is copied.

#include "warcraf/net/TurnsSync.h"

#include <cstdio>

namespace warcraf::net {

// ---------------------------------------------------------------------------
// TurnsSyncStore
// ---------------------------------------------------------------------------

TurnsSyncStore::TurnsSyncStore(std::vector<uint8_t> expectedSlots)
    : expectedSlots_(std::move(expectedSlots)) {}

bool TurnsSyncStore::SubmitTurn(uint8_t slot, uint32_t turn,
                                DataStore chunk) {
    auto& bySlot = turns_[turn];
    if (bySlot.find(slot) != bySlot.end()) {
        return false;  // duplicate submission: refused, kept nothing
    }
    ClientTurn clientTurn;
    clientTurn.turn = turn;
    clientTurn.slot = slot;
    clientTurn.chunk = std::move(chunk);
    bySlot.emplace(slot, std::move(clientTurn));
    return true;
}

bool TurnsSyncStore::IsTurnComplete(uint32_t turn) const {
    auto it = turns_.find(turn);
    if (it == turns_.end()) {
        return false;
    }
    for (uint8_t slot : expectedSlots_) {
        if (it->second.find(slot) == it->second.end()) {
            return false;
        }
    }
    return true;
}

std::vector<ClientTurn> TurnsSyncStore::TakeTurn(uint32_t turn) {
    std::vector<ClientTurn> result;
    if (!IsTurnComplete(turn)) {
        return result;
    }
    auto it = turns_.find(turn);
    for (uint8_t slot : expectedSlots_) {
        result.push_back(std::move(it->second.at(slot)));
    }
    turns_.erase(it);
    return result;
}

uint32_t TurnsSyncStore::LowestPendingTurn() const {
    if (turns_.empty()) {
        return kNoPendingTurn;
    }
    return turns_.begin()->first;
}

// ---------------------------------------------------------------------------
// TurnsSyncAccounting (FUN_6f310c80)
// ---------------------------------------------------------------------------

void TurnsSyncAccounting::AccountTurnData(NetSession& session, uint8_t slot,
                                          uint32_t byteCount, bool flag) {
    // A zero byte count forces the flag off (the original reads the flag
    // from FUN_6f3086a0's result: `if (uVar6 == 0) param_4 = 0`).
    if (byteCount == 0) {
        flag = false;
    }

    // Slot 0 bumps the global counter (+0x1c68).
    if (slot == 0) {
        session.IncrementGlobalCounter();
    }

    PerPlayerAccounting& player = session.Player(slot);
    player.bytesAccumulated += byteCount;   // +0x284
    session.SubtractBytesRemaining(byteCount);  // +0x225c

    if (flag) {
        // Bandwidth average (+0x2258): (old * 7 + bytes) >> 3.
        session.SetBandwidthAverage(
            (session.BandwidthAverage() * 7 + byteCount) >> 3);

        // Countdown (+0x2e0): max(countdown, bytes) - bytes; at zero,
        // FUN_6f30b460(slot, 0) (target identity UNVERIFIED).
        uint32_t countdown = player.countdown;
        if (countdown != 0) {
            uint32_t next = (byteCount < countdown ? countdown : byteCount) -
                            byteCount;
            player.countdown = next;
            if (next == 0 && onCountdownZero) {
                onCountdownZero(slot);
            }
        }

        // Advance turn (FUN_6f3130c0, inferred role) when the chunk is
        // from slot 0 and (the record state is 1, or the local slot is
        // ready, or this is a networked game where the local player is
        // slot 0). The record-state field is +0xbe4; the "local slot
        // ready" test is +0x2270 == 0 && +0x226c == 0 && local slot == 1
        // (those two context flags' meanings are UNVERIFIED).
        bool localSlotReady =
            session.ContextFlag2270() == 0 &&
            session.ContextFlag226c() == 0 && session.LocalSlot() == 1;
        bool networkedSlotZero =
            session.ModeTag() != NET_MODE_LOOP &&
            session.ModeTag() != NET_MODE_NONE && session.LocalSlot() == 0;
        if (slot == 0 &&
            (session.RecordState() == 1 || localSlotReady ||
             networkedSlotZero) &&
            onAdvanceTurn) {
            onAdvanceTurn();
        }

        // Local-player data-ready actions (FUN_6f3023c0, FUN_6f304920,
        // FUN_6f840bd0, FUN_6f3041c0, in that order): four unidentified
        // functions, one callback here.
        if (slot == session.LocalSlot() && onLocalTurnData) {
            onLocalTurnData();
        }
    }
}

// ---------------------------------------------------------------------------
// Turn-packet parsing (FUN_6f30b1a0 skeleton)
// ---------------------------------------------------------------------------

uint32_t TurnSubRecordConstructorAddress(uint8_t subType) {
    switch (subType) {
        case 0x10:
            return 0x6f2fc4b0;
        case 0x11:
            return 0x6f2fc7d0;
        case 0x12:
            return 0x6f2fc590;
        case 0x13:
            return 0x6f2fc710;
        case 0x14:
            return 0x6f2fc650;
        case 0x20:
            return 0x6f2fc3d0;
        default:
            return 0;
    }
}

TurnPacketParseResult ParseTurnPacket(
    const uint8_t* data, size_t length,
    const std::function<size_t(uint8_t subType)>& subRecordHandler) {
    TurnPacketParseResult result;
    if (length < 3) {
        result.status = TurnPacketStatus::kTruncated;
        return result;
    }
    // Header: byte 0 (meaning UNVERIFIED), then the sub-record section
    // length as a little-endian 16-bit value (local_118 >> 8 & 0xffff).
    result.headerByte = data[0];
    result.declaredLength = static_cast<uint16_t>(
        data[1] | (static_cast<uint16_t>(data[2]) << 8));

    size_t position = 3;  // sub-records start after the 3-byte header
    size_t sectionEnd = 3 + result.declaredLength;
    if (sectionEnd > length) {
        result.status = TurnPacketStatus::kTruncated;
        return result;
    }
    while (position < sectionEnd) {
        uint8_t subType = data[position];
        ++position;
        uint32_t constructor = TurnSubRecordConstructorAddress(subType);
        if (constructor == 0) {
            result.status = TurnPacketStatus::kUnknownSubType;
            return result;
        }
        ParsedTurnSubRecord record;
        record.type = subType;
        record.constructorAddress = constructor;
        if (subRecordHandler) {
            // The handler states how many payload bytes this sub-record
            // consumes (the original's per-type constructors read their
            // own payloads; their sizes are not evidenced).
            size_t payloadBytes = subRecordHandler(subType);
            if (position + payloadBytes > sectionEnd) {
                result.status = TurnPacketStatus::kCountMismatch;
                return result;
            }
            record.payloadBytes = payloadBytes;
            position += payloadBytes;
        } else {
            // No handler: the payload layout is unknown, so no further
            // bytes can be consumed faithfully. Stop here and report
            // the gap (the record is still reported).
            result.subRecords.push_back(record);
            result.status = TurnPacketStatus::kPayloadLayoutGap;
            return result;
        }
        result.subRecords.push_back(record);
    }
    if (position != sectionEnd) {
        result.status = TurnPacketStatus::kCountMismatch;
        return result;
    }
    result.status = TurnPacketStatus::kComplete;
    return result;
}

// ---------------------------------------------------------------------------
// Per-turn command log (FUN_6f2fd0d0)
// ---------------------------------------------------------------------------

void LogTurnCommand(uint32_t turn, const std::string& text,
                    uint8_t fromSlot) {
    // The original's format string, verbatim: "War3Net: (INFO): turn %u:
    // %s from %u\n". Its "from" value is the event's flag byte (+0x15);
    // this function takes the source slot explicitly (see the header).
    std::printf("War3Net: (INFO): turn %u: %s from %u\n", turn, text.c_str(),
                static_cast<unsigned>(fromSlot));
}

}  // namespace warcraf::net
