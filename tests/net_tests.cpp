// warcraf_net_tests -- unit tests for the network-layer reconstruction.
//
// Every expected value here is derived from the evidence, not from the
// implementation under test:
//   - Checksum vectors were computed with an independent Python
//     transcription of FUN_6f30ade0 (see the header comment of
//     NetChecksum.h). The C++ code must match the Python reference.
//   - Codec expectations come from the reader table in CNetCommand.h.
//   - Accounting expectations are FUN_6f310c80's arithmetic, worked out
//     by hand in the test comments.
//   - Pump expectations are FUN_6f30ade0's gate/break rules.
//
// Usage: warcraf_net_tests
// Exit code 0 = all checks passed; 1 = at least one failure (each
// failure is printed with its name).

#include <cstdio>
#include <cstring>
#include <string>
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

int g_pass = 0;
int g_fail = 0;
int g_skip = 0;

void Check(bool condition, const char* name) {
    if (condition) {
        ++g_pass;
        std::printf("[PASS] %s\n", name);
    } else {
        ++g_fail;
        std::printf("[FAIL] %s\n", name);
    }
}

void Skip(const char* name) {
    ++g_skip;
    std::printf("[SKIP] %s\n", name);
}

// Probes whether this host can send a UDP datagram on loopback (same
// probe as the demo). This sandbox rejects sendto with EPERM; the UDP
// round-trip checks are skipped there (counted as skipped, not passed).
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

CNetCommandBase MakeCommand(uint8_t number, uint32_t v1, uint8_t flag,
                            uint32_t v2, uint32_t v3) {
    CNetCommandBase cmd{};
    cmd.command = number;
    cmd.value1 = v1;
    cmd.flag = flag;
    cmd.value2 = v2;
    cmd.value3 = v3;
    return cmd;
}

void TestChecksum() {
    // Vectors from the independent Python reference (FUN_6f30ade0).
    const uint8_t oneByte[] = {0x01};
    const uint8_t oneDword[] = {0x01, 0x02, 0x03, 0x04};
    const uint8_t fiveBytes[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    const uint8_t asciiWar3[] = {'W', 'A', 'R', '3'};
    // One encoded turn chunk: 0x1a dword 7, 0x17 dword 42 / flag 1 /
    // dword 3 / dword 4, 0x1f end marker -- 20 bytes total.
    const uint8_t turnChunk[] = {0x1a, 0x07, 0x00, 0x00, 0x00,
                                 0x17, 0x2a, 0x00, 0x00, 0x00, 0x01,
                                 0x03, 0x00, 0x00, 0x00, 0x04, 0x00,
                                 0x00, 0x00, 0x1f};

    Check(RollPayloadChecksum(nullptr, 0) == 0x00000000,
          "checksum: empty payload rolls to 0");
    Check(RollPayloadChecksum(oneByte, 1) == 0x00000008,
          "checksum: roll([01]) == 0x00000008");
    Check(RollPayloadChecksum(oneDword, 4) == 0x20181008,
          "checksum: roll([01 02 03 04]) == 0x20181008");
    Check(RollPayloadChecksum(fiveBytes, 5) == 0x00c08069,
          "checksum: roll(5 bytes) == 0x00c08069");
    Check(RollPayloadChecksum(turnChunk, sizeof(turnChunk)) == 0x58fd0005,
          "checksum: roll(turn chunk) == 0x58fd0005");
    Check(RollPayloadChecksum(asciiWar3, 4) == 0x9a920ab9,
          "checksum: roll('WAR3') == 0x9a920ab9");

    Check(FoldChecksumWord(0) == 0x00000000,
          "checksum: fold(0) == 0");
    Check(FoldChecksumWord(0x00000008) == 0x0003d160,
          "checksum: fold(roll([01])) == 0x0003d160");
    Check(FoldChecksumWord(0x20181008) == 0x00262dc0,
          "checksum: fold(roll(dword)) == 0x00262dc0");
    Check(FoldChecksumWord(0x58fd0005) == 0x00a52088,
          "checksum: fold(roll(turn chunk)) == 0x00a52088");

    Check(EventChecksumValue(0x1f, turnChunk, sizeof(turnChunk)) ==
              0x00a520a7,
          "checksum: event value 0x1f + turn chunk == 0x00a520a7");
    Check(EventChecksumValue(0x1e, turnChunk, sizeof(turnChunk)) ==
              0x00a520a6,
          "checksum: event value 0x1e + turn chunk == 0x00a520a6 "
          "(0x1e shares the 0x1f block)");
    Check(EventChecksumValue(0x01, oneDword, 4) == 0x01,
          "checksum: event value 0x01 == type byte only");
    Check(EventChecksumValue(0x01, nullptr, 0) == 0x01,
          "checksum: event value 0x01 empty payload == 0x01");

    // Session fold chain (Python reference): 0x1f turn chunk, then
    // 0x1f 'WAR3', then 0x01 empty.
    uint32_t session = 0;
    FoldIntoSessionChecksum(
        session,
        EventChecksumValue(0x1f, turnChunk, sizeof(turnChunk)));
    Check(session == 0x00adb920,
          "checksum: session after 0x1f turn chunk == 0x00adb920");
    FoldIntoSessionChecksum(
        session, EventChecksumValue(0x1f, asciiWar3, 4));
    Check(session == 0x01a75264,
          "checksum: session after 0x1f 'WAR3' == 0x01a75264");
    FoldIntoSessionChecksum(session, EventChecksumValue(0x01, nullptr, 0));
    Check(session == 0x01a7cc90,
          "checksum: session after 0x01 empty == 0x01a7cc90");
}

void TestDataStore() {
    DataStore store;
    Check(store.Capacity() == kDataStoreCacheCapacity,
          "store: default capacity is 1460");
    store.WriteDword(0x04030201);
    store.WriteByte(0x05);
    Check(store.Size() == 5, "store: size after dword+byte is 5");
    // Little-endian on the wire: 0x04030201 -> 01 02 03 04.
    const uint8_t* bytes = store.Data();
    Check(bytes[0] == 0x01 && bytes[1] == 0x02 && bytes[2] == 0x03 &&
              bytes[3] == 0x04 && bytes[4] == 0x05,
          "store: dword is little-endian on the wire");

    store.Rewind();
    uint32_t dword = 0;
    uint8_t byte = 0;
    Check(store.ReadDword(dword) && dword == 0x04030201,
          "store: read dword round-trips");
    Check(store.ReadByte(byte) && byte == 0x05,
          "store: read byte round-trips");
    Check(!store.ReadDword(dword), "store: read past end fails");
    Check(!store.ReadByte(byte), "store: read byte past end fails");

    uint32_t a = 0, b = 0, c = 0;
    store.Clear();
    store.WriteDword(1);
    store.WriteDword(2);
    Check(!store.ReadThreeDwords(a, b, c),
          "store: read-three fails atomically on a short buffer");
    store.WriteDword(3);
    store.Rewind();
    Check(store.ReadThreeDwords(a, b, c) && a == 1 && b == 2 && c == 3,
          "store: read-three round-trips");

    DataStore big;
    for (int i = 0; i < 2000; ++i) {
        big.WriteByte(static_cast<uint8_t>(i & 0xff));
    }
    Check(big.Size() == 2000 && big.Capacity() >= 2000,
          "store: grows past the 1460 default");
    big.Rewind();
    uint8_t first = 0, last = 0;
    Check(big.ReadByte(first) && first == 0, "store: grown buffer head ok");
    big.SetReadPosition(1999);
    Check(big.ReadByte(last) && last == (1999 & 0xff),
          "store: grown buffer tail ok");
}

void TestCodec() {
    Check(HasEvidencedWireLayout(0x1a) && HasEvidencedWireLayout(0x1b) &&
              HasEvidencedWireLayout(0x1c) &&
              HasEvidencedWireLayout(0x17) &&
              HasEvidencedWireLayout(0x16),
          "codec: evidenced layouts are 0x1a/0x1b/0x1c/0x17/0x16");
    Check(!HasEvidencedWireLayout(0x10) &&
              !HasEvidencedWireLayout(0x23) &&
              !HasEvidencedWireLayout(0x2e),
          "codec: 0x10/0x23/0x2e have no evidenced layout");

    DataStore store;
    CNetCommandBase in = MakeCommand(0x17, 42, 1, 3, 4);
    Check(EncodeCommand(in, store), "codec: encode 0x17 succeeds");
    store.Rewind();
    CNetCommandBase out{};
    Check(DecodeCommand(store, out) && out.command == 0x17 &&
              out.value1 == 42 && out.flag == 1 && out.value2 == 3 &&
              out.value3 == 4,
          "codec: 0x17 round-trips all four fields");

    store.Clear();
    CNetCommandBase single = MakeCommand(0x1a, 0xdeadbeef, 0, 0, 0);
    Check(EncodeCommand(single, store), "codec: encode 0x1a succeeds");
    store.Rewind();
    Check(DecodeCommand(store, out) && out.command == 0x1a &&
              out.value1 == 0xdeadbeef,
          "codec: 0x1a round-trips its dword");

    store.Clear();
    CNetCommandBase partial = MakeCommand(0x16, 7, 9, 0, 0);
    Check(EncodeCommand(partial, store), "codec: encode 0x16 succeeds");
    store.Rewind();
    Check(DecodeCommand(store, out) && out.command == 0x16 &&
              out.flag == 9 && out.value1 == 7,
          "codec: 0x16 round-trips byte+dword");

    CNetCommandBase noLayout = MakeCommand(0x23, 1, 0, 0, 0);
    DataStore empty;
    Check(!EncodeCommand(noLayout, empty) && empty.Size() == 0,
          "codec: 0x23 encode refuses (no evidenced layout)");

    // Truncated decode fails.
    DataStore shortStore;
    shortStore.WriteByte(0x17);
    shortStore.WriteDword(42);
    shortStore.Rewind();
    Check(!DecodeCommand(shortStore, out),
          "codec: truncated 0x17 decode fails");

    // Turn chunk round-trip.
    DataStore chunk;
    std::vector<CNetCommandBase> commands = {
        MakeCommand(0x1a, 7, 0, 0, 0), MakeCommand(0x17, 42, 1, 3, 4)};
    Check(EncodeTurnChunk(commands, chunk),
          "codec: encode turn chunk succeeds");
    chunk.Rewind();
    std::vector<CNetCommandBase> decoded;
    Check(DecodeTurnChunk(chunk, decoded) && decoded.size() == 2 &&
              decoded[0].command == 0x1a && decoded[0].value1 == 7 &&
              decoded[1].command == 0x17 && decoded[1].value1 == 42 &&
              decoded[1].flag == 1 && decoded[1].value2 == 3 &&
              decoded[1].value3 == 4,
          "codec: turn chunk round-trips");

    // Corrupted length prefix fails.
    DataStore badLength;
    badLength.WriteDword(9999);
    badLength.WriteByte(0x1a);
    badLength.WriteDword(1);
    badLength.WriteByte(0x1f);
    badLength.Rewind();
    Check(!DecodeTurnChunk(badLength, decoded),
          "codec: turn chunk with a lying length fails");

    // Wrong end marker fails.
    DataStore badEnd;
    badEnd.WriteDword(5);
    badEnd.WriteByte(0x1a);
    badEnd.WriteDword(1);
    badEnd.WriteByte(0x1e);  // not the 0x1f end marker
    badEnd.Rewind();
    Check(!DecodeTurnChunk(badEnd, decoded),
          "codec: turn chunk with a wrong end marker fails");
}

void TestAccounting() {
    NetSession session(2);
    session.SetLocalSlot(1);
    session.SetModeTag(NET_MODE_LOOP);
    session.SetBytesRemaining(1000);
    TurnsSyncAccounting accounting;
    int countdownZero = 0;
    int advance = 0;
    int localData = 0;
    accounting.onCountdownZero = [&countdownZero](uint8_t) {
        ++countdownZero;
    };
    accounting.onAdvanceTurn = [&advance]() { ++advance; };
    accounting.onLocalTurnData = [&localData]() { ++localData; };

    // Slot 0, 100 bytes, flag on:
    //   global counter 0 -> 1; player0 bytes 0 -> 100; budget 1000 -> 900;
    //   bandwidth (0*7 + 100) >> 3 = 12; countdown 0 stays 0 (no call).
    accounting.AccountTurnData(session, 0, 100, true);
    Check(session.GlobalCounter() == 1,
          "accounting: slot 0 bumps the global counter");
    Check(session.Player(0).bytesAccumulated == 100,
          "accounting: per-player bytes accumulate (+0x284)");
    Check(session.BytesRemaining() == 900,
          "accounting: global budget decreases (+0x225c)");
    Check(session.BandwidthAverage() == 12,
          "accounting: bandwidth average (0*7+100)>>3 == 12");
    Check(countdownZero == 0, "accounting: zero countdown makes no call");

    // Countdown: player 1 starts at 250; 100 bytes -> 150, then 50,
    // then max(50,100)-100 = 0 -> the callback fires exactly once.
    session.Player(1).countdown = 250;
    accounting.AccountTurnData(session, 1, 100, true);
    Check(session.Player(1).countdown == 150,
          "accounting: countdown 250 - 100 == 150");
    accounting.AccountTurnData(session, 1, 100, true);
    Check(session.Player(1).countdown == 50,
          "accounting: countdown 150 - 100 == 50");
    accounting.AccountTurnData(session, 1, 100, true);
    Check(session.Player(1).countdown == 0 && countdownZero == 1,
          "accounting: countdown reaches zero and calls back once");

    // Advance: the decompile's condition is slot == 0 && flag &&
    // (recordState == 1 || local-slot-ready || networked-slot-zero),
    // where local-slot-ready is (context 0x2270 == 0 && context 0x226c
    // == 0 && local slot == 1). This session has local slot 1 and both
    // context flags 0, so local-slot-ready is true and the advance fires
    // on the very first slot-0 flagged chunk already.
    Check(advance == 1,
          "accounting: advance fires for slot 0 when local slot is ready");

    // Local-player data: slot 1 == local slot 1, flag on -> callback.
    Check(localData == 3,
          "accounting: local-player callback fired for slot 1 chunks");

    // Zero bytes force the flag off: no bandwidth change, no callbacks.
    uint32_t avgBefore = session.BandwidthAverage();
    accounting.AccountTurnData(session, 1, 0, true);
    Check(session.BandwidthAverage() == avgBefore && localData == 3,
          "accounting: zero bytes force the flag off");

    // Record state 1 is the other advance trigger: one more slot-0
    // flagged chunk -> advance fires a second time.
    session.SetRecordState(1);
    accounting.AccountTurnData(session, 0, 10, true);
    Check(advance == 2,
          "accounting: advance fires again for slot 0 + record 1");
}

void TestStore() {
    TurnsSyncStore store({0, 1});
    DataStore chunkA, chunkB;
    chunkA.WriteByte(0xaa);
    chunkB.WriteByte(0xbb);
    Check(!store.IsTurnComplete(0), "store: turn 0 incomplete at start");
    Check(store.SubmitTurn(1, 0, std::move(chunkB)),
          "store: submit slot 1 turn 0");
    Check(!store.IsTurnComplete(0),
          "store: still incomplete with only slot 1");
    Check(!store.SubmitTurn(1, 0, DataStore()),
          "store: duplicate submission refused");
    Check(store.SubmitTurn(0, 0, std::move(chunkA)),
          "store: submit slot 0 turn 0");
    Check(store.IsTurnComplete(0), "store: complete when both slots are in");
    std::vector<ClientTurn> taken = store.TakeTurn(0);
    Check(taken.size() == 2 && taken[0].slot == 0 && taken[1].slot == 1,
          "store: take returns both slots in slot order");
    Check(store.LowestPendingTurn() == TurnsSyncStore::kNoPendingTurn,
          "store: nothing pending after the take");
    Check(store.TakeTurn(5).empty(),
          "store: taking an incomplete turn returns nothing");
    Check(store.SubmitTurn(0, 2, DataStore()),
          "store: out-of-order turn 2 accepted");
    Check(store.LowestPendingTurn() == 2,
          "store: lowest pending turn is 2");
}

void TestPump() {
    // Basic 0x1f dispatch + checksum fold.
    NetSession session(2);
    session.SetLocalSlot(0);
    session.SetFrameState(4);
    NetEventPump pump;
    int dispatched = 0;
    pump.RegisterHandler(0x1f,
                         [&dispatched](const QueuedNetEvent&) { ++dispatched; });
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    pump.Enqueue(0x1f, 1, std::vector<uint8_t>(payload, payload + 4));
    pump.Pump(session, true);
    Check(dispatched == 1, "pump: 0x1f handler runs");
    // Event value 0x1f + roll([01 02 03 04]) -> fold -> 0x00920a38.
    Check(session.EventChecksum() == 0x00920a38,
          "pump: session checksum folded by the 0x1f event");
    Check(pump.ChecksummedCount() == 1, "pump: one event checksummed");

    // At most one 0x1f per Pump call.
    pump.Enqueue(0x1f, 1, std::vector<uint8_t>(payload, payload + 4));
    pump.Enqueue(0x1f, 1, std::vector<uint8_t>(payload, payload + 4));
    pump.Pump(session, true);
    Check(dispatched == 2 && pump.QueuedCount() == 1,
          "pump: one 0x1f per call; the second stays queued");
    pump.Pump(session, true);
    Check(dispatched == 3 && pump.QueuedCount() == 0,
          "pump: the second 0x1f goes on the next call");

    // param_2 == 0: the first gate-passing event is neither folded nor
    // dispatched; it stays queued.
    NetSession session2(2);
    session2.SetLocalSlot(0);
    session2.SetFrameState(4);
    NetEventPump pump2;
    int dispatched2 = 0;
    pump2.RegisterHandler(0x01,
                          [&dispatched2](const QueuedNetEvent&) {
                              ++dispatched2;
                          });
    pump2.Enqueue(0x01, 1, {});
    pump2.Pump(session2, false);
    Check(dispatched2 == 0 && session2.EventChecksum() == 0 &&
              pump2.QueuedCount() == 1,
          "pump: param_2==0 stops before fold and dispatch");
    pump2.Pump(session2, true);
    Check(dispatched2 == 1 && pump2.QueuedCount() == 0,
          "pump: the held event dispatches on the next call");

    // Checksum gate: flag 0 with local slot 0 -> no checksum, still
    // dispatched. Frame state 3 -> no checksum either.
    NetSession session3(2);
    session3.SetLocalSlot(0);
    session3.SetFrameState(4);
    NetEventPump pump3;
    int dispatched3 = 0;
    pump3.RegisterHandler(0x01,
                          [&dispatched3](const QueuedNetEvent&) {
                              ++dispatched3;
                          });
    pump3.Enqueue(0x01, 0, {});
    pump3.Pump(session3, true);
    Check(dispatched3 == 1 && pump3.ChecksummedCount() == 0,
          "pump: flag 0 + local slot 0 skips the checksum, still dispatches");
    session3.SetFrameState(3);
    pump3.Enqueue(0x01, 1, {});
    pump3.Pump(session3, true);
    Check(dispatched3 == 2 && pump3.ChecksummedCount() == 0,
          "pump: frame state 3 skips the checksum");

    // Battle.net events go to the stub: counted, never dispatched, never
    // checksummed.
    NetEventPump pump4;
    pump4.Enqueue(0x40, 1, {0x01, 0x02});
    pump4.Pump(session3, true);
    Check(pump4.BattleNetStubCount() == 1 && pump4.DispatchedCount() == 0,
          "pump: 0x40 goes to the Battle.net stub only");

    // No-handler game event is counted, not hidden.
    NetEventPump pump5;
    pump5.Enqueue(0x05, 1, {});
    pump5.Pump(session3, true);
    Check(pump5.DispatchedCount() == 1 && pump5.NoHandlerCount() == 1,
          "pump: unhandled game event is counted");

    // 0x17 store hook and 0x21 preprocess/latency hooks.
    NetSession session6(2);
    session6.SetLocalSlot(0);
    session6.SetFrameState(4);
    NetEventPump pump6;
    int storeHooks = 0, preprocessHooks = 0, latencyHooks = 0;
    pump6.SetTurnsSyncStoreHook(
        [&storeHooks](const QueuedNetEvent&) { ++storeHooks; });
    pump6.SetPayloadPreprocessHook(
        [&preprocessHooks](const QueuedNetEvent&) { ++preprocessHooks; });
    pump6.SetLatencyEventHook(
        [&latencyHooks](const QueuedNetEvent&) { ++latencyHooks; });
    pump6.Enqueue(0x17, 1, {});
    pump6.Enqueue(0x21, 1, {});
    pump6.Pump(session6, true);
    Check(storeHooks == 1, "pump: 0x17 runs the TurnsSync store hook");
    Check(preprocessHooks == 1 && latencyHooks == 1,
          "pump: 0x21 runs preprocess + latency hooks");
    Check(pump6.StoreHookCalls() == 1 &&
              pump6.PreprocessHookCalls() == 1,
          "pump: hook call counters agree");
}

void TestTurnPacket() {
    // Packet: header byte 0x00, length 3, sub-records 0x10, 0x11, 0x20.
    const uint8_t packet[] = {0x00, 0x03, 0x00, 0x10, 0x11, 0x20};
    auto bareHandler = [](uint8_t) -> size_t { return 0; };
    TurnPacketParseResult result = ParseTurnPacket(packet, sizeof(packet),
                                                   bareHandler);
    Check(result.status == TurnPacketStatus::kComplete,
          "turn packet: bare type bytes parse completely");
    Check(result.headerByte == 0x00 && result.declaredLength == 3,
          "turn packet: header byte and length recorded");
    Check(result.subRecords.size() == 3 &&
              result.subRecords[0].type == 0x10 &&
              result.subRecords[0].constructorAddress == 0x6f2fc4b0 &&
              result.subRecords[1].type == 0x11 &&
              result.subRecords[1].constructorAddress == 0x6f2fc7d0 &&
              result.subRecords[2].type == 0x20 &&
              result.subRecords[2].constructorAddress == 0x6f2fc3d0,
          "turn packet: sub-record types map to their constructors");

    // No handler: stops at the first sub-record with a layout gap.
    TurnPacketParseResult gap = ParseTurnPacket(packet, sizeof(packet));
    Check(gap.status == TurnPacketStatus::kPayloadLayoutGap &&
              gap.subRecords.size() == 1,
          "turn packet: no handler stops with a layout gap");

    // Truncated: declared 5, only 3 sub-record bytes present.
    const uint8_t shortPacket[] = {0x00, 0x05, 0x00, 0x10, 0x11, 0x20};
    TurnPacketParseResult truncated =
        ParseTurnPacket(shortPacket, sizeof(shortPacket), bareHandler);
    Check(truncated.status == TurnPacketStatus::kTruncated,
          "turn packet: short buffer is truncated");

    // Unknown sub-record type.
    const uint8_t unknownPacket[] = {0x00, 0x01, 0x00, 0x99};
    TurnPacketParseResult unknown =
        ParseTurnPacket(unknownPacket, sizeof(unknownPacket), bareHandler);
    Check(unknown.status == TurnPacketStatus::kUnknownSubType,
          "turn packet: unknown sub-record type is rejected");

    // Handler consuming bytes past the declared length.
    auto greedyHandler = [](uint8_t) -> size_t { return 10; };
    TurnPacketParseResult overrun =
        ParseTurnPacket(packet, sizeof(packet), greedyHandler);
    Check(overrun.status == TurnPacketStatus::kCountMismatch,
          "turn packet: handler overrun is a count mismatch");

    // Log line (FUN_6f2fd0d0 format) -- prints to stdout; the check is
    // that it does not crash. The demo prints these per applied turn.
    LogTurnCommand(7, "turn applied", 1);
    Check(true, "turn packet: log line prints");

    Check(GameTypeMagic(false) == 0x57415233,
          "session: GameTypeMagic(false) == 'WAR3'");
    Check(GameTypeMagic(true) == 0x57335850,
          "session: GameTypeMagic(true) == 'W3XP'");
}

void TestTransport() {
    // Loopback round-trip on fixed high ports (the demo uses 41000/41001;
    // the tests use a different pair to avoid clashing with a running demo).
    NetConnection a, b;
    std::string error;
    Check(RouterSetup(RouterSetupParams{}, 41800, 41801, a, error),
          "transport: RouterSetup opens peer A");
    RouterSetupParams params;
    params.modeTag = NET_MODE_LOOP;
    Check(RouterSetup(params, 41801, 41800, b, error),
          "transport: RouterSetup opens peer B (LOOP)");
    Check(a.IsOpen() && b.IsOpen() && a.LocalPort() == 41800 &&
              b.LocalPort() == 41801,
          "transport: both connections open on their ports");

    if (!UdpLoopbackAvailable()) {
        // This host blocks UDP datagram sends (sendto -> EPERM); the
        // round-trip below cannot run here. It is skipped, not passed,
        // and the demo's in-process channel covers the same bytes.
        Skip("transport: send one event store (host blocks UDP sendto)");
        Skip("transport: loopback datagram arrives byte-identical (host "
             "blocks UDP sendto)");
    } else {
        DataStore store;
        store.WriteDword(0xdeadbeef);
        store.WriteByte(0x1f);
        Check(a.SendEvent(0x1f, store), "transport: send one event store");
        std::vector<uint8_t> received;
        Check(b.ReceiveDatagram(received, 1000) && received.size() == 5 &&
                  received[0] == 0xef && received[1] == 0xbe &&
                  received[2] == 0xad && received[3] == 0xde &&
                  received[4] == 0x1f,
              "transport: loopback datagram arrives byte-identical");
    }

    // Timeout on an empty socket.
    std::vector<uint8_t> nothing;
    Check(!b.ReceiveDatagram(nothing, 50) && b.TimedOut(),
          "transport: empty socket times out cleanly");
}

}  // namespace

int main() {
    TestChecksum();
    TestDataStore();
    TestCodec();
    TestAccounting();
    TestStore();
    TestPump();
    TestTurnPacket();
    TestTransport();
    std::printf("\n%d passed, %d failed, %d skipped\n", g_pass, g_fail,
                g_skip);
    return g_fail == 0 ? 0 : 1;
}
