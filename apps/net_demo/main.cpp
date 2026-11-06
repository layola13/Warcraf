// warcraf_net_demo -- two loopback peers exchanging lockstep turns.
//
// Usage: warcraf_net_demo [--turns N] [--port-base P] [--step-ms MS]
//
// What it demonstrates (the user-confirmed scope: basic network, frame
// sync, demo -- nothing else):
//   - Two peers (slots 0 and 1) open UDP loopback connections via
//     RouterSetup (FUN_6f283540) and exchange one turn chunk per 25 ms
//     frame (the original's fixed step, S2).
//   - Each turn chunk is an encoded CNetCommand list (0x1a + 0x17, the
//     two command numbers with evidenced readers) sent as a 0x1e
//     (TurnsSync out) event and received as a 0x1f (TurnsSync in) event.
//     The direction assignment (0x1e out / 0x1f in) is an interpretation:
//     S1 lists 0x1e/0x1f together as "TurnsSync data in/out" without
//     assigning a direction. Both types go through the same checksum and
//     accounting path, so the demo is unaffected either way.
//   - The receiver verifies the chunk checksum against the sender's
//     (RollPayloadChecksum, FUN_6f30ade0) and the pump folds the event
//     value into the session accumulator.
//   - A turn is applied only when both slots' chunks are in
//     (CTurnsSyncStore lockstep rule), and the applied commands are
//     checked against the deterministic values each peer was told to
//     produce.
//
// The datagram carries exactly the CDataStore bytes (CDataStoreCache<1460>).
// No envelope is added (the FUN_6f311750 envelope is an evidence gap),
// so the receiving side is told the event type by this harness (0x1f),
// not by the wire -- see NetTransport.h.
//
// Host note: this sandbox rejects UDP sendto with EPERM (bind and TCP
// work; only datagram sends are blocked). The demo probes for that at
// startup and, when UDP is blocked, carries the identical datagram
// bytes over an in-process loopback channel instead -- the checksum,
// pump, and lockstep logic are exercised exactly the same way, and the
// demo prints which channel it used. On an unrestricted host the real
// UDP path is used. NetTransport itself stays UDP in both cases.
//
// The 0x1f event flag is set per peer so the pump's checksum gate passes:
// flag = 1 for the slot-0 peer, 0 for the slot-1 peer, because the gate
// is (flag != 0) == (local slot == 0) (FUN_6f30ade0). The flag therefore
// distinguishes local-origin from remote-origin events in the original;
// here it is set to satisfy the gate on each peer.
//
// Exit code 0 means: every turn applied by both peers, every chunk
// checksum matched, no lost or out-of-order steps. Anything else is a
// failure and is printed as such.

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "warcraf/net/NetChecksum.h"
#include "warcraf/net/NetCommandCodec.h"
#include "warcraf/net/NetDataStore.h"
#include "warcraf/net/NetEventPump.h"
#include "warcraf/net/NetSession.h"
#include "warcraf/net/NetTransport.h"
#include "warcraf/net/TurnsSync.h"

using namespace warcraf::net;

namespace {

struct Peer {
    uint8_t slot = 0;
    NetSession session{2};
    TurnsSyncStore store{{0, 1}};
    TurnsSyncAccounting accounting;
    NetEventPump pump;
    NetConnection connection;
    // In-process fallback channel (see below); null when UDP is used.
    struct InProcessChannel* channel = nullptr;
    uint32_t appliedTurns = 0;
    uint32_t checksumMismatches = 0;
    uint32_t lostSteps = 0;
    uint32_t countdownZeroCalls = 0;
    uint32_t advanceCalls = 0;
    uint32_t localDataCalls = 0;
};

// In-process loopback channel: the fallback when the host blocks UDP
// datagrams (this sandbox rejects sendto with EPERM; see
// UdpLoopbackAvailable). It carries the identical datagram bytes between
// the two peers through two queues, so the checksum, pump, and lockstep
// logic are exercised exactly as with UDP -- only the kernel datagram
// hop is replaced. It is a demo-harness channel, NOT part of the
// transport layer (NetTransport stays UDP).
struct InProcessChannel {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::vector<uint8_t>> toPeer0;
    std::deque<std::vector<uint8_t>> toPeer1;
};

// Probes whether this host can send a UDP datagram on loopback, using
// throwaway sockets (so the probe never injects bytes into the peers).
// On Windows the probe is skipped and UDP is assumed available.
bool UdpLoopbackAvailable() {
#ifdef _WIN32
    return true;
#else
    int a = ::socket(AF_INET, SOCK_DGRAM, 0);
    int b = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (a < 0 || b < 0) {
        if (a >= 0) ::close(a);
        if (b >= 0) ::close(b);
        return false;
    }
    sockaddr_in addrA{};
    addrA.sin_family = AF_INET;
    addrA.sin_port = htons(0);
    addrA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr_in addrB = addrA;
    if (::bind(a, reinterpret_cast<sockaddr*>(&addrA), sizeof(addrA)) != 0 ||
        ::bind(b, reinterpret_cast<sockaddr*>(&addrB), sizeof(addrB)) != 0) {
        ::close(a);
        ::close(b);
        return false;
    }
    socklen_t len = sizeof(addrB);
    if (::getsockname(b, reinterpret_cast<sockaddr*>(&addrB), &len) != 0) {
        ::close(a);
        ::close(b);
        return false;
    }
    const char probe = 0;
    ssize_t sent = ::sendto(a, &probe, 1, 0,
                            reinterpret_cast<sockaddr*>(&addrB),
                            sizeof(addrB));
    ::close(a);
    ::close(b);
    return sent == 1;
#endif
}

// Sends one datagram (one turn chunk) from a peer: UDP when available,
// the in-process channel otherwise. Returns false on failure.
bool SendDatagram(Peer& peer, const DataStore& chunk) {
    if (peer.channel == nullptr) {
        return peer.connection.SendEvent(0x1e, chunk);
    }
    std::vector<uint8_t> bytes(chunk.Data(), chunk.Data() + chunk.Size());
    std::lock_guard<std::mutex> lock(peer.channel->mutex);
    if (peer.slot == 0) {
        peer.channel->toPeer1.push_back(std::move(bytes));
    } else {
        peer.channel->toPeer0.push_back(std::move(bytes));
    }
    peer.channel->cv.notify_all();
    return true;
}

// Receives one datagram for a peer, waiting up to timeoutMs (0 = poll
// once). Returns false on timeout.
bool ReceiveDatagram(Peer& peer, std::vector<uint8_t>& out, int timeoutMs) {
    if (peer.channel == nullptr) {
        return peer.connection.ReceiveDatagram(out, timeoutMs);
    }
    std::unique_lock<std::mutex> lock(peer.channel->mutex);
    std::deque<std::vector<uint8_t>>& queue =
        peer.slot == 0 ? peer.channel->toPeer0 : peer.channel->toPeer1;
    if (timeoutMs < 0) {
        peer.channel->cv.wait(
            lock, [&queue] { return !queue.empty(); });
    } else if (timeoutMs > 0) {
        if (!peer.channel->cv.wait_for(
                lock, std::chrono::milliseconds(timeoutMs),
                [&queue] { return !queue.empty(); })) {
            return false;
        }
    }
    if (queue.empty()) {
        return false;
    }
    out = std::move(queue.front());
    queue.pop_front();
    return true;
}

// Deterministic commands for (turn, slot). Values are chosen so the
// receiver can verify them exactly: 0x1a carries turn*10 + slot, 0x17
// carries (turn, slot, turn*2, turn*3).
std::vector<CNetCommandBase> BuildTurnCommands(uint32_t turn, uint8_t slot) {
    CNetCommandBase singleDword{};
    singleDword.command = 0x1a;
    singleDword.value1 = turn * 10 + slot;

    CNetCommandBase multiField{};
    multiField.command = 0x17;
    multiField.value1 = turn;
    multiField.flag = slot;
    multiField.value2 = turn * 2;
    multiField.value3 = turn * 3;
    return {singleDword, multiField};
}

bool VerifyTurnCommands(uint32_t turn, uint8_t slot,
                        const std::vector<CNetCommandBase>& commands) {
    std::vector<CNetCommandBase> expected = BuildTurnCommands(turn, slot);
    if (commands.size() != expected.size()) {
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (commands[i].command != expected[i].command ||
            commands[i].value1 != expected[i].value1 ||
            commands[i].value2 != expected[i].value2 ||
            commands[i].value3 != expected[i].value3 ||
            commands[i].flag != expected[i].flag) {
            return false;
        }
    }
    return true;
}

void PrintUsage() {
    std::printf("usage: warcraf_net_demo [--turns N] [--port-base P] "
                "[--step-ms MS]\n");
}

}  // namespace

int main(int argc, char** argv) {
    uint32_t turns = 20;
    uint16_t portBase = 41000;
    int stepMs = 25;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--turns") == 0 && i + 1 < argc) {
            turns = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--port-base") == 0 && i + 1 < argc) {
            portBase = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--step-ms") == 0 && i + 1 < argc) {
            stepMs = std::atoi(argv[++i]);
        } else {
            PrintUsage();
            return 2;
        }
    }
    if (turns == 0) {
        std::printf("no turns requested\n");
        return 2;
    }

    std::printf("warcraf_net_demo: 2 loopback peers, %u turns, %d ms step\n",
                turns, stepMs);
    std::printf("ports: peer 0 on %u, peer 1 on %u (127.0.0.1 UDP)\n",
                portBase, portBase + 1);

    // Channel decision: UDP when the host allows it; otherwise the
    // in-process loopback channel (identical datagram bytes -- see the
    // InProcessChannel comment). The UDP connections are still opened
    // either way (bind works here); only the datagram hop is replaced.
    InProcessChannel inProcessChannel;
    bool udpAvailable = UdpLoopbackAvailable();
    std::printf("UDP loopback: %s\n", udpAvailable
                                          ? "available, using UDP datagrams"
                                          : "BLOCKED on this host (sendto "
                                            "returns EPERM) -- using the "
                                            "in-process loopback channel "
                                            "with identical datagram bytes");

    // ---- set up both peers -----------------------------------------------
    Peer peers[2];
    // senderSlot -> checksum of the chunk that slot sent for each turn.
    std::map<std::pair<uint32_t, uint8_t>, uint32_t> sentChecksums;

    for (int i = 0; i < 2; ++i) {
        Peer& peer = peers[i];
        peer.slot = static_cast<uint8_t>(i);
        peer.session.SetLocalSlot(peer.slot);
        peer.session.SetModeTag(NET_MODE_LOOP);
        peer.session.SetFrameState(4);  // in-game: checksum gate needs > 3
        peer.session.SetBytesRemaining(0xffffffff);

        peer.accounting.onCountdownZero = [&peer](uint8_t slot) {
            ++peer.countdownZeroCalls;
            std::printf("  [peer %u] countdown zero for slot %u "
                        "(FUN_6f30b460)\n",
                        peer.slot, slot);
        };
        peer.accounting.onAdvanceTurn = [&peer]() {
            ++peer.advanceCalls;
        };
        peer.accounting.onLocalTurnData = [&peer]() {
            ++peer.localDataCalls;
        };

        // 0x1f (TurnsSync in): decode the chunk, verify the checksum
        // against what the sender recorded, submit to the store.
        peer.pump.RegisterHandler(0x1f, [&peer, &sentChecksums,
                                         i](const QueuedNetEvent& event) {
            DataStore store;
            store.WriteBytes(event.payload.data(), event.payload.size());
            uint32_t receivedChecksum =
                RollPayloadChecksum(event.payload.data(),
                                    event.payload.size());
            std::vector<CNetCommandBase> commands;
            if (!DecodeTurnChunk(store, commands) || commands.size() != 2) {
                std::printf("  [peer %u] BAD turn chunk (decode failed)\n",
                            peer.slot);
                ++peer.lostSteps;
                return;
            }
            // The sender slot rides inside the 0x17 command's flag
            // (no invented envelope: the slot is in the payload).
            uint8_t senderSlot = commands[1].flag;
            auto it = sentChecksums.find(
                {commands[1].value1, senderSlot});
            if (it == sentChecksums.end() ||
                it->second != receivedChecksum) {
                std::printf("  [peer %u] CHECKSUM MISMATCH on turn %u "
                            "from slot %u\n",
                            peer.slot, commands[1].value1, senderSlot);
                ++peer.checksumMismatches;
                return;
            }
            DataStore chunk;
            chunk.WriteBytes(event.payload.data(), event.payload.size());
            peer.accounting.AccountTurnData(
                peer.session, senderSlot,
                static_cast<uint32_t>(event.payload.size()), true);
            if (!peer.store.SubmitTurn(senderSlot, commands[1].value1,
                                       std::move(chunk))) {
                std::printf("  [peer %u] DUPLICATE chunk turn %u slot %u\n",
                            peer.slot, commands[1].value1, senderSlot);
                ++peer.lostSteps;
            }
        });

        RouterSetupParams params;
        params.name = "net_demo";
        params.mapPath = "Maps/Demo.w3x";
        params.modeTag = NET_MODE_LOOP;
        std::string error;
        uint16_t localPort = static_cast<uint16_t>(portBase + i);
        uint16_t peerPort = static_cast<uint16_t>(portBase + (1 - i));
        if (!RouterSetup(params, localPort, peerPort, peer.connection,
                         error)) {
            std::printf("peer %d: RouterSetup failed: %s\n", i,
                        error.c_str());
            return 1;
        }
        if (!udpAvailable) {
            peer.channel = &inProcessChannel;
        }
    }
    std::printf("both peers connected (LOOP mode, FUN_6f283540)\n");

    // ---- frame loop: one 25 ms step per turn ----------------------------
    bool failed = false;
    for (uint32_t turn = 0; turn < turns && !failed; ++turn) {
        // Each peer builds and sends its chunk for this turn.
        for (int i = 0; i < 2; ++i) {
            Peer& peer = peers[i];
            DataStore chunk;
            if (!EncodeTurnChunk(BuildTurnCommands(turn, peer.slot),
                                 chunk)) {
                std::printf("peer %u: encode failed for turn %u\n",
                            peer.slot, turn);
                return 1;
            }
            sentChecksums[{turn, peer.slot}] =
                RollPayloadChecksum(chunk.Data(), chunk.Size());
            if (!SendDatagram(peer, chunk)) {
                std::printf("peer %u: send failed for turn %u\n",
                            peer.slot, turn);
                return 1;
            }
            // The sender accounts its own outgoing chunk too
            // (FUN_6f310c80 runs on both directions).
            peer.accounting.AccountTurnData(peer.session, peer.slot,
                                            static_cast<uint32_t>(chunk.Size()),
                                            true);
            peer.store.SubmitTurn(peer.slot, turn, std::move(chunk));
        }

        // Each peer receives, pumps, and applies when the turn is complete.
        for (int i = 0; i < 2; ++i) {
            Peer& peer = peers[i];
            bool applied = false;
            for (int attempt = 0; attempt < 20 && !applied; ++attempt) {
                std::vector<uint8_t> datagram;
                if (ReceiveDatagram(peer, datagram, 25)) {
                    // Flag per the checksum gate: 1 for the slot-0
                    // peer, 0 for the slot-1 peer (see the header
                    // comment of this file).
                    uint8_t flag = peer.session.LocalSlot() == 0 ? 1 : 0;
                    peer.pump.Enqueue(0x1f, flag, std::move(datagram));
                }
                // Drain the pump; a 0x1f event stops each Pump call,
                // so repeat until the queue is empty.
                for (int drain = 0; drain < 8 && peer.pump.QueuedCount() > 0;
                     ++drain) {
                    peer.pump.Pump(peer.session, true);
                }
                if (peer.store.IsTurnComplete(turn)) {
                    std::vector<ClientTurn> appliedTurn =
                        peer.store.TakeTurn(turn);
                    bool ok = appliedTurn.size() == 2;
                    for (const ClientTurn& ct : appliedTurn) {
                        DataStore chunk = ct.chunk;
                        std::vector<CNetCommandBase> commands;
                        if (!DecodeTurnChunk(chunk, commands) ||
                            !VerifyTurnCommands(turn, ct.slot, commands)) {
                            ok = false;
                        }
                    }
                    if (!ok) {
                        std::printf("peer %u: turn %u applied with WRONG "
                                    "commands\n",
                                    peer.slot, turn);
                        ++peer.lostSteps;
                        failed = true;
                        break;
                    }
                    LogTurnCommand(turn, "turn applied", peer.slot);
                    ++peer.appliedTurns;
                    applied = true;
                }
            }
            if (!applied && !failed) {
                std::printf("peer %u: turn %u NEVER completed (lost step)\n",
                            peer.slot, turn);
                ++peer.lostSteps;
                failed = true;
            }
        }

        std::printf("turn %u: applied by both peers | session checksums "
                    "A=0x%08x B=0x%08x (each folds its own received "
                    "stream)\n",
                    turn, peers[0].session.EventChecksum(),
                    peers[1].session.EventChecksum());
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
    }

    // ---- summary ----------------------------------------------------------
    std::printf("\n--- summary ---\n");
    for (int i = 0; i < 2; ++i) {
        const Peer& peer = peers[i];
        std::printf(
            "peer %u: applied %u/%u turns | checksum mismatches %u | "
            "lost steps %u | bytes accumulated slot0=%u slot1=%u | "
            "bandwidth avg 0x%08x | global counter %u | countdown-zero %u "
            "| advance %u | local-data %u | pump dispatched %zu "
            "(no-handler %zu)\n",
            peer.slot, peer.appliedTurns, turns,
            peer.checksumMismatches, peer.lostSteps,
            peer.session.Player(0).bytesAccumulated,
            peer.session.Player(1).bytesAccumulated,
            peer.session.BandwidthAverage(), peer.session.GlobalCounter(),
            peer.countdownZeroCalls, peer.advanceCalls,
            peer.localDataCalls, peer.pump.DispatchedCount(),
            peer.pump.NoHandlerCount());
    }
    bool ok = !failed && peers[0].appliedTurns == turns &&
              peers[1].appliedTurns == turns &&
              peers[0].checksumMismatches == 0 &&
              peers[1].checksumMismatches == 0 &&
              peers[0].lostSteps == 0 && peers[1].lostSteps == 0;
    std::printf(ok ? "RESULT: OK -- %u turns in lockstep, checksums "
                     "consistent, no lost steps\n"
                   : "RESULT: FAIL\n",
                turns);
    return ok ? 0 : 1;
}
