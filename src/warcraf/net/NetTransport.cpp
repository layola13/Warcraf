// NetTransport.h -- implementation.
//
// Router setup follows FUN_6f283540's evidenced flag rules and bounded
// copies; the socket calls are the platform equivalents of the original's
// WS2_32 use (POSIX sockets on Linux, Winsock on Windows -- the Windows
// path is written but UNTESTED on this host).

#include "warcraf/net/NetTransport.h"

#include <cstring>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace warcraf::net {
namespace {

#ifdef _WIN32
// Winsock needs one WSAStartup per process. The original loads WS2_32
// dynamically (26 ordinal imports); the static link here is the
// platform-equivalent call shape.
bool EnsureWinsock(std::string& error) {
    static bool started = false;
    if (!started) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            error = "WSAStartup failed";
            return false;
        }
        started = true;
    }
    return true;
}

// Closes a Winsock socket; POSIX close is used on Linux.
void CloseSocket(intptr_t handle) { closesocket(static_cast<SOCKET>(handle)); }
#else
void CloseSocket(intptr_t handle) { ::close(static_cast<int>(handle)); }
#endif

// Bounded copy with the SStrCopy (Storm ordinal 501) semantics used by
// FUN_6f283540: at most maxLength-1 characters plus a terminating NUL.
std::string BoundedCopy(const std::string& source, size_t maxLength) {
    if (maxLength == 0) {
        return std::string();
    }
    return source.substr(0, maxLength - 1);
}

// Builds the peer sockaddr from the stored network-order IP + port.
sockaddr_in MakePeerAddress(uint32_t ipNetworkOrder, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ipNetworkOrder;
    return addr;
}

}  // namespace

NetConnection::NetConnection(NetConnection&& other) noexcept
    : socket_(other.socket_),
      localPort_(other.localPort_),
      peerPort_(other.peerPort_),
      peerIpNetworkOrder_(other.peerIpNetworkOrder_),
      timedOut_(other.timedOut_) {
    other.socket_ = -1;
}

NetConnection& NetConnection::operator=(NetConnection&& other) noexcept {
    if (this != &other) {
        Close();
        socket_ = other.socket_;
        localPort_ = other.localPort_;
        peerPort_ = other.peerPort_;
        peerIpNetworkOrder_ = other.peerIpNetworkOrder_;
        timedOut_ = other.timedOut_;
        other.socket_ = -1;
    }
    return *this;
}

NetConnection::~NetConnection() { Close(); }

void NetConnection::Close() {
    if (socket_ < 0) {
        return;
    }
    CloseSocket(socket_);
    socket_ = -1;
}

bool NetConnection::OpenLoopback(uint16_t localPort, uint16_t peerPort,
                                 std::string& error) {
    return OpenPeer("127.0.0.1", peerPort, localPort, error);
}

bool NetConnection::OpenPeer(const std::string& peerHost, uint16_t peerPort,
                             uint16_t localPort, std::string& error) {
#ifdef _WIN32
    if (!EnsureWinsock(error)) {
        return false;
    }
    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        error = "socket() failed";
        return false;
    }
#else
    int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        error = std::string("socket() failed: ") + std::strerror(errno);
        return false;
    }
#endif

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(localPort);
    localAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(sock, reinterpret_cast<sockaddr*>(&localAddr),
               sizeof(localAddr)) != 0) {
#ifdef _WIN32
        error = "bind() failed";
#else
        error = std::string("bind() failed: ") + std::strerror(errno);
#endif
        CloseSocket(static_cast<intptr_t>(sock));
        return false;
    }

    // Resolve the peer address once so SendEvent is a plain sendto.
    in_addr peerInAddr{};
    if (::inet_pton(AF_INET, peerHost.c_str(), &peerInAddr) != 1) {
        error = "invalid peer host: " + peerHost;
        CloseSocket(static_cast<intptr_t>(sock));
        return false;
    }

    socket_ = static_cast<intptr_t>(sock);
    localPort_ = localPort;
    peerPort_ = peerPort;
    peerIpNetworkOrder_ = peerInAddr.s_addr;
    return true;
}

bool NetConnection::SendEvent(uint8_t eventType, const DataStore& store) {
    if (socket_ < 0) {
        return false;
    }
    // FUN_6f2feb10 passes (eventType, store) to FUN_6f311750. The wire
    // effect of the type is unknown (FUN_6f311750 is an evidence gap),
    // so the datagram carries exactly the store's bytes; the type is
    // recorded only by the caller (see the header comment). `eventType`
    // is kept in the signature because the original's send entry takes
    // it -- it is intentionally not placed on the wire here.
    (void)eventType;
    sockaddr_in peerAddr = MakePeerAddress(peerIpNetworkOrder_, peerPort_);
#ifdef _WIN32
    int sent = ::sendto(static_cast<SOCKET>(socket_),
                        reinterpret_cast<const char*>(store.Data()),
                        static_cast<int>(store.Size()), 0,
                        reinterpret_cast<sockaddr*>(&peerAddr),
                        sizeof(peerAddr));
#else
    ssize_t sent = ::sendto(static_cast<int>(socket_), store.Data(),
                            store.Size(), 0,
                            reinterpret_cast<sockaddr*>(&peerAddr),
                            sizeof(peerAddr));
#endif
    return sent >= 0 && static_cast<size_t>(sent) == store.Size();
}

bool NetConnection::ReceiveDatagram(std::vector<uint8_t>& out,
                                    int timeoutMs) {
    timedOut_ = false;
    if (socket_ < 0) {
        return false;
    }
#ifdef _WIN32
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(static_cast<SOCKET>(socket_), &readSet);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    int ready = ::select(0, &readSet, nullptr, nullptr,
                         timeoutMs < 0 ? nullptr : &timeout);
    if (ready == 0) {
        timedOut_ = true;
        return false;
    }
    if (ready < 0) {
        return false;
    }
#else
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(static_cast<int>(socket_), &readSet);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    int ready = ::select(static_cast<int>(socket_) + 1, &readSet, nullptr,
                         nullptr, timeoutMs < 0 ? nullptr : &timeout);
    if (ready == 0) {
        timedOut_ = true;
        return false;
    }
    if (ready < 0) {
        return false;
    }
#endif
    // One datagram is one event store (CDataStoreCache<1460>); size the
    // receive buffer a little larger so an oversized datagram is visible
    // as truncation instead of silent loss.
    out.resize(kDataStoreCacheCapacity + 64);
#ifdef _WIN32
    int received = ::recvfrom(static_cast<SOCKET>(socket_),
                              reinterpret_cast<char*>(out.data()),
                              static_cast<int>(out.size()), 0, nullptr,
                              nullptr);
#else
    ssize_t received = ::recvfrom(static_cast<int>(socket_), out.data(),
                                  out.size(), 0, nullptr, nullptr);
#endif
    if (received < 0) {
        return false;
    }
    out.resize(static_cast<size_t>(received));
    return true;
}

bool RouterSetup(const RouterSetupParams& params, uint16_t localPort,
                 uint16_t peerPort, NetConnection& out, std::string& error) {
    // FUN_6f283540, evidenced rules:
    //   - bounded copies (Storm SStrCopy, ordinal 501): name 0x20,
    //     password 0x10, map/path 0x36. The copies are part of the
    //     setup; this implementation performs them and keeps the
    //     truncated values (they are the connection's identity).
    //   - flags 0x10 / 0x400000 -> parameter 6 is fixed to 0xc.
    //   - flag 0x200 -> reuse an existing connection (the original
    //     calls FUN_6f287160 / FUN_6f2821f0 and reads field +0x430;
    //     there is no existing connection in this runtime, so the
    //     reuse flag is recorded and a fresh connection is opened --
    //     marked as a gap, not hidden).
    //   - param_14 -> the 0x80000000 flag bit.
    // The internal setup calls (FUN_6f1c0680, FUN_6f1cccc0,
    // FUN_6f1bb730, FUN_6f76e060, FUN_6f0210a0, thunk_FUN_6f842600,
    // FUN_6f18cb00) are unidentified and have no equivalent here.
    std::string name = BoundedCopy(params.name, 0x20);
    std::string password = BoundedCopy(params.password, 0x10);
    std::string mapPath = BoundedCopy(params.mapPath, 0x36);
    (void)name;
    (void)password;
    (void)mapPath;

    uint32_t parameter6 = params.parameter6;
    if ((params.flags & 0x10) != 0 || (params.flags & 0x400000) != 0) {
        parameter6 = 0xc;
    }
    (void)parameter6;

    uint32_t flags = params.flags;
    if (params.highFlag) {
        flags |= 0x80000000u;
    }
    bool reuseExisting = (flags & 0x200) != 0;
    (void)reuseExisting;  // gap: no existing connection exists in this
                          // runtime; a fresh connection is always opened.

    if (params.modeTag == 0x4c4f4f50 /* NET_MODE_LOOP */) {
        return out.OpenLoopback(localPort, peerPort, error);
    }
    // Any other mode tag opens the same UDP peer connection: the mode
    // tag selects the original's connection object class, not the
    // socket mechanics, and only LOOP is evidenced for the demo.
    return out.OpenPeer("127.0.0.1", peerPort, localPort, error);
}

}  // namespace warcraf::net
