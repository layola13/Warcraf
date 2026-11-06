// NetTransport.h -- router setup and peer transport.
//
// Evidence:
//   - FUN_6f283540 (router setup, 612 B, net_decomp.txt) and S1 section 5:
//     flag rules (0x10/0x400000 -> parameter 6 is fixed to 0xc; 0x200
//     reuses an existing connection; 0x80000000 from param_14), bounded
//     copies via Storm SStrCopy (ordinal 501: name 0x20, password 0x10,
//     map/path 0x36), and the string "Router is running in lockstep mode".
//   - FUN_6f2feb10 (event send, 338 B, net_decomp.txt): builds a
//     CDataStoreCache<1460>, fills it (FUN_6f83f350), then calls
//     FUN_6f311750(eventType, &store). The internals of FUN_6f83f350 and
//     FUN_6f311750 are NOT in our decompiles -- the transmit itself is
//     an evidence gap (see SendEvent below).
//   - S1 section 2: transport goes via WS2_32.dll (26 ordinal imports)
//     + internal Net functions; SNet is NOT in Game.dll's Storm import
//     table. OsNet RTTI (S10): NETCONN / TCPLISTEN / NETCONNECT /
//     LOOPCONN / NETSELSOCKPTR -- LOOPCONN is the loopback connection
//     object; the layouts are unverified.
//   - NET_MODE_LOOP (0x4c4f4f50) / NET_MODE_NONE (0x4e4f4e45): CNetData.h.
//
// What this file does, and what it does NOT claim:
//   - RouterSetup assembles connection parameters with the evidenced
//     flag rules and bounded copies, then creates the connection. The
//     original's internal setup calls (FUN_6f1c0680, FUN_6f1cccc0,
//     FUN_6f1bb730, FUN_6f76e060, FUN_6f0210a0, FUN_6f287160,
//     FUN_6f2821f0, thunk_FUN_6f842600, FUN_6f18cb00) are unidentified
//     and are listed here as a naming gap -- they are NOT reimplemented.
//   - The socket calls are the platform equivalents of the original's
//     WS2_32 use: POSIX sockets on Linux, Winsock on Windows (the
//     Windows path is written but UNTESTED on this host -- no Windows
//     machine here). UDP datagrams are used (per the approved task
//     brief); whether the original's turn path used datagrams or a
//     stream is NOT evidenced -- TCPLISTEN exists in OsNet RTTI, so a
//     stream transport exists somewhere in the original. Marked here.
//   - A datagram carries EXACTLY the CDataStore bytes (the evidenced
//     CDataStoreCache<1460> payload). No envelope is added: the event
//     type that FUN_6f2feb10 passes to FUN_6f311750 is recorded in the
//     send log but its wire effect is unknown (FUN_6f311750 gap), so
//     the receiving side is told the event type by the harness, not by
//     the wire. Do NOT read a wire format into this that is not there.

#ifndef WARCRAF_NET_TRANSPORT_H
#define WARCRAF_NET_TRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "warcraf/net/NetDataStore.h"

namespace warcraf::net {

// Connection parameters assembled by RouterSetup (FUN_6f283540).
// Only the evidenced fields are modelled; the original takes 15
// parameters, most of them unidentified (see the header comment).
struct RouterSetupParams {
    std::string name;      // bounded copy, 0x20 (SStrCopy, ord 501)
    std::string password;  // bounded copy, 0x10
    std::string mapPath;   // bounded copy, 0x36
    uint32_t flags = 0;    // 0x10/0x400000 -> parameter6 = 0xc;
                           // 0x200 -> reuse existing connection
    uint32_t parameter6 = 0;  // forced to 0xc when flags say so
    bool highFlag = false;    // param_14: sets 0x80000000 in the flags
    uint32_t modeTag = 0x4c4f4f50;  // NET_MODE_LOOP by default
};

// One peer connection (loopback or UDP peer). The original's OsNet
// LOOPCONN is the loopback object; this class is the runtime equivalent
// for the demo: a UDP socket bound to 127.0.0.1.
class NetConnection {
public:
    NetConnection() = default;
    NetConnection(const NetConnection&) = delete;
    NetConnection& operator=(const NetConnection&) = delete;
    NetConnection(NetConnection&& other) noexcept;
    NetConnection& operator=(NetConnection&& other) noexcept;
    ~NetConnection();

    // Creates a loopback connection: binds localPort on 127.0.0.1 and
    // addresses datagrams at peerPort on 127.0.0.1. Returns false on a
    // socket error (message in `error`).
    bool OpenLoopback(uint16_t localPort, uint16_t peerPort,
                      std::string& error);
    // Creates a peer connection to an arbitrary host (same UDP mechanics).
    bool OpenPeer(const std::string& peerHost, uint16_t peerPort,
                  uint16_t localPort, std::string& error);

    bool IsOpen() const { return socket_ >= 0; }
    uint16_t LocalPort() const { return localPort_; }
    uint16_t PeerPort() const { return peerPort_; }

    // Sends one event (FUN_6f2feb10 shape: event type + store). The
    // datagram carries exactly the store's bytes -- see the header
    // comment on the FUN_6f311750 envelope gap. The type is used only
    // for the send log, never placed on the wire. Returns false on a
    // socket error.
    bool SendEvent(uint8_t eventType, const DataStore& store);

    // Receives one datagram, waiting up to timeoutMs (0 = poll once,
    // < 0 = block indefinitely). Returns false on timeout or error
    // (distinguish via TimedOut()).
    bool ReceiveDatagram(std::vector<uint8_t>& out, int timeoutMs);
    bool TimedOut() const { return timedOut_; }

    void Close();

private:
    // Socket handle as intptr_t so the same field holds a POSIX fd on
    // Linux and a Winsock SOCKET (UINT_PTR) on Windows. -1 means closed.
    intptr_t socket_ = -1;
    uint16_t localPort_ = 0;
    uint16_t peerPort_ = 0;
    // Peer IPv4 address in network byte order (resolved once in
    // OpenPeer); the sockaddr is rebuilt from this on each send, so no
    // platform socket type leaks into the header.
    uint32_t peerIpNetworkOrder_ = 0;
    bool timedOut_ = false;
};

// Router setup (FUN_6f283540): applies the evidenced flag rules and
// bounded copies, then opens the connection. `localPort`/`peerPort`
// are the demo's socket endpoints (the original's port selection is
// inside the unidentified setup calls). Returns false (with `error`)
// when the connection cannot be opened.
bool RouterSetup(const RouterSetupParams& params, uint16_t localPort,
                 uint16_t peerPort, NetConnection& out, std::string& error);

}  // namespace warcraf::net

#endif  // WARCRAF_NET_TRANSPORT_H
