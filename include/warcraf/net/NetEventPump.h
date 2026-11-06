// NetEventPump.h -- event queue, pump, and dispatchers.
//
// Evidence:
//   - FUN_6f30ade0 (event pump, 954 B), FUN_6f3098d0 (game dispatcher,
//     5074 B), FUN_6f307b50 (Battle.net dispatcher, 2625 B),
//     FUN_6f307980 (pre-dispatch, 38 B), FUN_6f307880 (post-dispatch,
//     62 B): all decompiled in ~/workspace/warcraf_ghidra/sync_decomp.txt
//     (pump) and net_decomp.txt (pre/post/BN dispatcher).
//   - ~/workspace/warcraf_stage_network.md (S1) sections 3.1-3.5.
//   - Event object layout (CNetEvent, 24 B; type +0x14, flag +0x15,
//     payload +0x8, length +0x0c): CNetData.h.
//
// Pump flow (FUN_6f30ade0, transcribed as behaviour, not copied):
//   Walk the event list (head +0xf40, next saved before dispatch).
//   For each event, first the outer gate: dispatch only when the master
//   switch (+0x173c) is clear or the per-type table entry (+0x1744 +
//   type*4) is non-zero. Then, for game events (type < 0x40), the
//   checksum gate: when (flag != 0) == (local slot == 0) and frame state
//   (+0x278) > 3, compute the event checksum value (0x1e/0x1f roll the
//   payload; other types use the type byte alone) and fold it into the
//   session accumulator -- unless the pump was asked to stop after the
//   first gate-passing event (param_2 == 0). Before dispatch, 0x17 goes
//   down the TurnsSync-store path, 0x21/0x2f get payload preprocessing.
//   Then pre-dispatch (retain payload, FUN_6f307980) -> dispatcher
//   (FUN_6f3098d0) -> post-dispatch (release the 24-byte event,
//   FUN_6f307880). After dispatching a 0x1e or 0x1f event the pump stops
//   (both set local_38 in the shared checksum block): at most one
//   0x1e/0x1f event per pump call.
//   Events with type >= 0x40 and (type - 0x40) < 0xbf go to the
//   Battle.net dispatcher instead (FUN_6f307b50).
//
// What is NOT implemented here (naming/evidence gaps, recorded not hidden):
//   - The per-type event constructors (~213 B each, FUN_6f2f35d0-6f2f5400
//     range): their identities are unknown. Dispatch therefore calls
//     handlers registered by the embedding application; an event type
//     with no handler is counted as "dispatched, no handler" -- the
//     original always constructs an object, but this code will not
//     pretend to know which one.
//   - FUN_6f02d2d0 (payload preprocessing for 0x21/0x2f), FUN_6f83e540 /
//     FUN_6f83e670 (the 0x21/0x2f follow-ups), FUN_6f0fb2c0 (the 0x17
//     store path, called three times), FUN_6f312dc0 (payload retain),
//     FUN_6f3039f0 (post-dispatch step), FUN_6f30ada0 (event delivery):
//     addresses known, behaviour unidentified. They appear as named
//     hook points below; the default hook only records that it ran.
//   - The Battle.net dispatcher body (FUN_6f307b50): OUT OF SCOPE for
//     this work (user scope 2026-11-06). BN events are recorded and
//     counted by the stub; no handshake, no BN event is ever delivered.

#ifndef WARCRAF_NET_EVENT_PUMP_H
#define WARCRAF_NET_EVENT_PUMP_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include "warcraf/net/NetSession.h"

namespace warcraf::net {

// One queued event. The original CNetEvent is a 24-byte heap object whose
// payload is a pointer; this runtime form holds the payload bytes inline
// (no pointer width on the wire or in the queue). `payloadRetainCount`
// mirrors the FUN_6f307980 retain: 0 while unretained, +1 per retain.
struct QueuedNetEvent {
    uint8_t type = 0;   // +0x14: event type (0x01-0x31 game, >= 0x40 BN)
    uint8_t flag = 0;   // +0x15: flag byte
    std::vector<uint8_t> payload;  // +0x08/+0x0c: payload bytes
    int payloadRetainCount = 0;
};

class NetEventPump {
public:
    using EventHandler = std::function<void(const QueuedNetEvent&)>;

    // Enqueues an event (a copy of the payload). Returns the queue depth.
    size_t Enqueue(uint8_t type, uint8_t flag, std::vector<uint8_t> payload);

    // Registers the handler for one game event type (0x01-0x31). The
    // handler stands in for the original's per-type event constructor
    // (identity unknown -- see the header comment).
    void RegisterHandler(uint8_t type, EventHandler handler);

    // Hook points for the unidentified helpers (addresses in the comment
    // of each setter in the .cpp). The default hook records the call and
    // does nothing else.
    //   0x17 TurnsSync-store path (FUN_6f0fb2c0, called three times).
    void SetTurnsSyncStoreHook(EventHandler hook);
    //   0x21/0x2f payload preprocessing (FUN_6f02d2d0).
    void SetPayloadPreprocessHook(EventHandler hook);
    //   0x21 follow-up (FUN_6f83e540) and 0x2f follow-up (FUN_6f83e670).
    void SetLatencyEventHook(EventHandler hook);
    void SetTriggerEventHook(EventHandler hook);

    // Runs the pump against the session (FUN_6f30ade0).
    //   continueAfterFirst == false mirrors param_2 == 0: the pump
    //   computes the first gate-passing event's checksum value and its
    //   pre-dispatch handling, then stops BEFORE folding it into the
    //   session accumulator and BEFORE dispatching it; that event stays
    //   queued for the next Pump call.
    //   continueAfterFirst == true drains the list, except a dispatched
    //   0x1e or 0x1f event always stops the pump (local_38 is set in the
    //   shared 0x1e/0x1f checksum block): at most one 0x1e/0x1f event
    //   per call.
    void Pump(NetSession& session, bool continueAfterFirst);

    // ---- observability (for tests / the demo) -----------------------------
    size_t QueuedCount() const { return queue_.size(); }
    size_t DispatchedCount() const { return dispatched_; }
    size_t NoHandlerCount() const { return noHandler_; }
    size_t BattleNetStubCount() const { return battleNetStub_; }
    size_t ChecksummedCount() const { return checksummed_; }
    size_t PreprocessHookCalls() const { return preprocessHookCalls_; }
    size_t StoreHookCalls() const { return storeHookCalls_; }

private:
    // Pre-dispatch (FUN_6f307980): retain the payload when present.
    // Returns false when the event has no payload (nothing retained).
    bool RetainEventPayload(QueuedNetEvent& event);
    // Post-dispatch (FUN_6f307880): clear the session's current event
    // and release the event object (here: drop it from the queue).
    void ReleaseEventAfterDispatch(NetSession& session);
    // Game dispatcher (FUN_6f3098d0): type clamped to 0x32 above 0x3f,
    // then the registered handler, or a counted no-handler dispatch.
    void DispatchGameEvent(const QueuedNetEvent& event);
    // Battle.net dispatcher stub (FUN_6f307b50): record + count only.
    void DispatchBattleNetEventStub(const QueuedNetEvent& event);

    std::deque<QueuedNetEvent> queue_;
    EventHandler handlers_[0x32]{};  // game types 0x00-0x31
    EventHandler turnsSyncStoreHook_;
    EventHandler payloadPreprocessHook_;
    EventHandler latencyEventHook_;
    EventHandler triggerEventHook_;
    size_t dispatched_ = 0;
    size_t noHandler_ = 0;
    size_t battleNetStub_ = 0;
    size_t checksummed_ = 0;
    size_t preprocessHookCalls_ = 0;
    size_t storeHookCalls_ = 0;
};

}  // namespace warcraf::net

#endif  // WARCRAF_NET_EVENT_PUMP_H
