// NetEventPump.h -- implementation.
//
// Flow follows FUN_6f30ade0 (pump), FUN_6f307980 (pre-dispatch),
// FUN_6f307880 (post-dispatch), FUN_6f3098d0 (game dispatcher) and
// FUN_6f307b50 (Battle.net dispatcher stub). Behaviour is transcribed;
// the unidentified helpers appear as hook points (see the header).

#include "warcraf/net/NetEventPump.h"

#include "warcraf/net/NetChecksum.h"

namespace warcraf::net {

size_t NetEventPump::Enqueue(uint8_t type, uint8_t flag,
                             std::vector<uint8_t> payload) {
    QueuedNetEvent event;
    event.type = type;
    event.flag = flag;
    event.payload = std::move(payload);
    queue_.push_back(std::move(event));
    return queue_.size();
}

void NetEventPump::RegisterHandler(uint8_t type, EventHandler handler) {
    if (type < 0x32) {
        handlers_[type] = std::move(handler);
    }
}

void NetEventPump::SetTurnsSyncStoreHook(EventHandler hook) {
    turnsSyncStoreHook_ = std::move(hook);
}

void NetEventPump::SetPayloadPreprocessHook(EventHandler hook) {
    payloadPreprocessHook_ = std::move(hook);
}

void NetEventPump::SetLatencyEventHook(EventHandler hook) {
    latencyEventHook_ = std::move(hook);
}

void NetEventPump::SetTriggerEventHook(EventHandler hook) {
    triggerEventHook_ = std::move(hook);
}

bool NetEventPump::RetainEventPayload(QueuedNetEvent& event) {
    // FUN_6f307980: no payload (payload pointer < 1) -> return 0,
    // nothing retained. Otherwise FUN_6f312dc0(payload): the payload's
    // reference count goes up by one (identity of FUN_6f312dc0 is
    // UNVERIFIED; it is inferred to be the retain from the call shape).
    if (event.payload.empty()) {
        return false;
    }
    event.payloadRetainCount += 1;
    return true;
}

void NetEventPump::ReleaseEventAfterDispatch(NetSession& session) {
    // FUN_6f307880: if the event being released is the session's
    // current event (+0x1b40), clear it; then FUN_6f3039f0() (identity
    // UNVERIFIED, no equivalent here) and the 24-byte event object is
    // freed via FUN_6f78a5c0(..., 0x18, "CNetEvent", -2). Here the
    // event object is the queue node itself: dropping it frees it.
    if (!queue_.empty() &&
        queue_.front().type == session.CurrentEventType()) {
        session.SetCurrentEventType(0);
    }
    if (!queue_.empty()) {
        queue_.pop_front();
    }
}

void NetEventPump::DispatchGameEvent(const QueuedNetEvent& event) {
    // FUN_6f3098d0: types above 0x3f are clamped to 0x32 at the
    // dispatcher entry (S1). The pump only sends types < 0x40 here,
    // so the clamp only affects 0x32-0x3f, which have no case in the
    // 0x01-0x31 table and therefore no handler.
    uint8_t effectiveType = event.type > 0x3f ? 0x32 : event.type;
    ++dispatched_;
    if (effectiveType < 0x32 && handlers_[effectiveType]) {
        handlers_[effectiveType](event);
    } else {
        // No handler: the original always constructs a per-type event
        // object (~213 B, FUN_6f2f35d0-6f2f5400 range), but those
        // constructors are unidentified, so there is nothing faithful
        // to call here. Counted, not hidden.
        ++noHandler_;
    }
}

void NetEventPump::DispatchBattleNetEventStub(const QueuedNetEvent& event) {
    // FUN_6f307b50 (Battle.net dispatcher): OUT OF SCOPE. The event is
    // recorded and counted; no BN handler runs, no handshake is ever
    // attempted. `event` is unused by design.
    (void)event;
    ++battleNetStub_;
}

void NetEventPump::Pump(NetSession& session, bool continueAfterFirst) {
    // FUN_6f30ade0: walk the list; the next node is saved (+0x1b40)
    // before dispatch, because dispatch may free the current event.
    while (!queue_.empty()) {
        QueuedNetEvent& event = queue_.front();
        uint8_t type = event.type;
        uint8_t flag = event.flag;
        session.SetCurrentEventType(type);

        // Outer gate: dispatch only when the master switch (+0x173c)
        // is clear or this type's table entry (+0x1744 + type*4) is set.
        // A gated-out event is skipped but stays in the queue only
        // conceptually -- the original moves on to the next node; here
        // the event is dropped after counting, because an event the
        // original never dispatches can never be dispatched later by
        // this pump (its gate inputs are session state, re-evaluated
        // on the next Pump call only if the event is re-enqueued).
        if (!session.EventDispatchMasterBypass() &&
            !session.IsEventTypeEnabled(type)) {
            queue_.pop_front();
            continue;
        }

        if (type < 0x40) {
            // Checksum gate: (flag != 0) == (local slot == 0) and
            // frame state (+0x278) > 3.
            bool flagSet = flag != 0;
            bool localIsSlotZero = session.LocalSlot() == 0;
            if ((flagSet == localIsSlotZero) && session.FrameState() > 3) {
                uint32_t value = EventChecksumValue(
                    type, event.payload.data(), event.payload.size());

                // Pre-dispatch special handling (inside the gate block):
                // 0x17 -> the TurnsSync store path (FUN_6f0fb2c0, three
                // calls); 0x21/0x2f -> payload preprocessing
                // (FUN_6f02d2d0) then a per-type follow-up.
                if (type == 0x17) {
                    if (turnsSyncStoreHook_) {
                        turnsSyncStoreHook_(event);
                        ++storeHookCalls_;
                    }
                } else if (type == 0x21 || type == 0x2f) {
                    if (payloadPreprocessHook_) {
                        payloadPreprocessHook_(event);
                        ++preprocessHookCalls_;
                    }
                    if (type == 0x21 && latencyEventHook_) {
                        latencyEventHook_(event);
                    } else if (type == 0x2f && triggerEventHook_) {
                        triggerEventHook_(event);
                    }
                }

                if (!continueAfterFirst) {
                    // param_2 == 0: the decompile breaks here -- before
                    // the fold into +0x227c and before dispatch. The
                    // event stays queued for the next Pump call.
                    break;
                }
                FoldIntoSessionChecksum(session.EventChecksumRef(), value);
                ++checksummed_;
            }

            RetainEventPayload(event);
            DispatchGameEvent(event);
            ReleaseEventAfterDispatch(session);

            // A 0x1e or 0x1f event stops the pump after dispatch:
            // both set local_38 in the shared checksum block
            // (LAB_6f30aeb2), so at most one of them per pump call.
            if (type == 0x1e || type == 0x1f) {
                break;
            }
        } else if ((type - 0x40) < 0xbf) {
            // Battle.net events: pre-dispatch, BN dispatcher stub,
            // post-dispatch. No checksum touches them (FUN_6f30ade0).
            RetainEventPayload(event);
            DispatchBattleNetEventStub(event);
            ReleaseEventAfterDispatch(session);
        } else {
            // Type >= 0x40 + 0xbf: outside the decompile's BN range;
            // the original's `else if` fails and the event is skipped.
            queue_.pop_front();
        }
    }
}

}  // namespace warcraf::net
