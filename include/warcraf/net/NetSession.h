// NetSession.h -- runtime network session state.
//
// This is the *runtime* counterpart of the CNetData layout header
// (include/warcraf/net/CNetData.h). It holds the session fields the
// evidence pins, at their verified offsets, as ordinary 32-bit values --
// no pointers, so it behaves identically on 64-bit hosts. Each field's
// comment cites its offset and evidence source. Fields whose meaning is
// unverified are named as such and default to 0.
//
// Evidence: S1 (warcraf_stage_network.md), CNetData.h (offsets +0x610,
// +0x278, +0x227c, +0x2258, +0x225c, +0x2288, +0x1c68, +0xbe4, +0x173c,
// +0x1744, +0x1b40, +0x226c, +0x2270), FUN_6f30e2b0 (game magic,
// net_decomp.txt).
//
// Per-player accounting entries mirror ClientTurnsSync in CNetData.h
// (stride 0x304; +0x278 playerCounter, +0x284 bytesAccum, +0x2e0 countdown).

#ifndef WARCRAF_NET_SESSION_H
#define WARCRAF_NET_SESSION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "warcraf/net/CNetData.h"

namespace warcraf::net {

// Demo player-capacity parameter. The original's per-player table stride
// (0x304) is evidenced; how many entries the table has is NOT evidenced.
// The session is constructed with the game's player count (the demo uses 2).
inline constexpr size_t kDefaultMaxPlayers = 12;

// One per-player accounting entry (ClientTurnsSync fields, S1).
// Only the three evidenced offsets are modelled; the rest of the 0x304
// entry is not part of this runtime state.
struct PerPlayerAccounting {
    uint32_t playerCounter = 0;    // +0x278: per-player state counter
    uint32_t bytesAccumulated = 0; // +0x284: accumulated bytes this turn
    uint32_t countdown = 0;        // +0x2e0: byte-budget countdown; at zero
                                   // the original calls FUN_6f30b460(slot, 0)
                                   // (target identity UNVERIFIED)
};

class NetSession {
public:
    explicit NetSession(size_t playerCount = kDefaultMaxPlayers);

    // ---- session fields (offsets in CNetData, S1/S2) ---------------------
    uint32_t ModeTag() const { return modeTag_; }
    void SetModeTag(uint32_t tag) { modeTag_ = tag; }  // +0x2288: LOOP/NONE

    uint32_t LocalSlot() const { return localSlot_; }
    void SetLocalSlot(uint32_t slot) { localSlot_ = slot; }  // +0x610

    uint32_t FrameState() const { return frameState_; }
    void SetFrameState(uint32_t state) { frameState_ = state; }  // +0x278
    // The pump's checksum gate needs frameState > 3 (FUN_6f30ade0).

    uint32_t EventChecksum() const { return eventChecksum_; }  // +0x227c
    uint32_t& EventChecksumRef() { return eventChecksum_; }
    void SetEventChecksum(uint32_t value) { eventChecksum_ = value; }
    void AddToEventChecksum(uint32_t value) { eventChecksum_ += value; }

    uint32_t BytesRemaining() const { return bytesRemaining_; }  // +0x225c
    void SetBytesRemaining(uint32_t value) { bytesRemaining_ = value; }
    void SubtractBytesRemaining(uint32_t bytes) { bytesRemaining_ -= bytes; }

    uint32_t BandwidthAverage() const { return bandwidthAvg_; }  // +0x2258
    void SetBandwidthAverage(uint32_t value) { bandwidthAvg_ = value; }

    uint32_t GlobalCounter() const { return globalCounter_; }  // +0x1c68
    void IncrementGlobalCounter() { ++globalCounter_; }

    uint32_t RecordState() const { return recordState_; }  // +0xbe4
    void SetRecordState(uint32_t state) { recordState_ = state; }

    // Context flags at +0x2270 / +0x226c (FUN_6f310c80 advance condition).
    // Exact meaning UNVERIFIED; they only gate the "local slot ready" test.
    uint32_t ContextFlag2270() const { return contextFlag2270_; }
    uint32_t ContextFlag226c() const { return contextFlag226c_; }
    void SetContextFlag2270(uint32_t value) { contextFlag2270_ = value; }
    void SetContextFlag226c(uint32_t value) { contextFlag226c_ = value; }

    // Event-dispatch gates (FUN_6f30ade0 outer check):
    // an event is dispatched only when masterBypass is true (+0x173c == 0)
    // or the per-type table entry (+0x1744 + type*4) is non-zero.
    bool EventDispatchMasterBypass() const { return masterBypass_; }
    void SetEventDispatchMasterBypass(bool value) { masterBypass_ = value; }
    bool IsEventTypeEnabled(uint8_t type) const;
    void SetEventTypeEnabled(uint8_t type, bool enabled);

    // Current event being processed (+0x1b40), for observability.
    uint8_t CurrentEventType() const { return currentEventType_; }
    void SetCurrentEventType(uint8_t type) { currentEventType_ = type; }

    // ---- per-player accounting -------------------------------------------
    size_t PlayerCount() const { return players_.size(); }
    PerPlayerAccounting& Player(size_t slot) { return players_.at(slot); }
    const PerPlayerAccounting& Player(size_t slot) const {
        return players_.at(slot);
    }

private:
    uint32_t modeTag_ = NET_MODE_LOOP;   // +0x2288
    uint32_t localSlot_ = 0;             // +0x610
    uint32_t frameState_ = 0;            // +0x278
    uint32_t eventChecksum_ = 0;         // +0x227c
    uint32_t bytesRemaining_ = 0xffffffff;  // +0x225c; initial value NOT
                                            // evidenced -- the demo uses the
                                            // full 32-bit budget
    uint32_t bandwidthAvg_ = 0;         // +0x2258
    uint32_t globalCounter_ = 0;        // +0x1c68
    uint32_t recordState_ = 0;          // +0xbe4
    uint32_t contextFlag2270_ = 0;      // +0x2270, meaning UNVERIFIED
    uint32_t contextFlag226c_ = 0;      // +0x226c, meaning UNVERIFIED
    bool masterBypass_ = true;          // +0x173c == 0 (dispatch enabled)
    std::array<bool, 0x40> perTypeEnabled_{};  // +0x1744 table, default on
    uint8_t currentEventType_ = 0;      // +0x1b40
    std::vector<PerPlayerAccounting> players_;
};

// Game-type magic (FUN_6f30e2b0, 21 B, net_decomp.txt):
// returns "WAR3" (0x57415233) for Reign of Chaos, "W3XP" (0x57335850)
// for Frozen Throne (selected by global DAT_6fb592f0 == 1).
uint32_t GameTypeMagic(bool frozenThrone);

}  // namespace warcraf::net

#endif  // WARCRAF_NET_SESSION_H
