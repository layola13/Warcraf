// NetSession.h -- implementation.

#include "warcraf/net/NetSession.h"

namespace warcraf::net {

NetSession::NetSession(size_t playerCount) : players_(playerCount) {
    perTypeEnabled_.fill(true);
}

bool NetSession::IsEventTypeEnabled(uint8_t type) const {
    if (type >= perTypeEnabled_.size()) {
        return false;
    }
    return perTypeEnabled_[type];
}

void NetSession::SetEventTypeEnabled(uint8_t type, bool enabled) {
    if (type < perTypeEnabled_.size()) {
        perTypeEnabled_[type] = enabled;
    }
}

uint32_t GameTypeMagic(bool frozenThrone) {
    // FUN_6f30e2b0: "WAR3" unless the global DAT_6fb592f0 is 1, then "W3XP".
    return frozenThrone ? NET_MAGIC_W3XP : NET_MAGIC_WAR3;
}

}  // namespace warcraf::net
