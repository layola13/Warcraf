// NetDataStore.h -- packet buffer with the evidenced CDataStore primitives.
//
// Evidence:
//   - ~/workspace/Warcraf/include/warcraf/net/CNetCommand.h (S3): the
//     CDataStore primitives -- FUN_6f04e1d0 (write dword, 81 B), FUN_6f04dfa0
//     (write byte, 80 B), FUN_6f83f8e0 (alloc/realloc, 118 B, via Storm
//     ordinals 401/405), the vtable +0x28 "read three" used by FUN_6f30ba60,
//     and the byte read FUN_6f04d3b0 used by the turn-packet loop.
//   - ~/workspace/warcraf_stage_simulation.md (S3) section on CDataStore.
//   - FUN_6f2feb10 (net_decomp.txt): the send path builds a
//     CDataStoreCache<1460> -- a 1460-byte store -- before transmitting.
//
// This class implements the *behaviour* of those primitives (append with
// growth, sequential read); it is NOT the original CDataStore object
// layout (that layout is UNVERIFIED -- see CNetCommand.h -- and this class
// deliberately does not claim it). The original CDataStore struct declared
// in CNetCommand.h is untouched.
//
// Wire integers are little-endian, matching the original's x86 layout.
// All read/write positions are explicit, so the buffer behaves identically
// on 64-bit hosts: no pointer width ever reaches the wire.
//
// Growth policy: the original grows through its vtable (+0x0c) on overflow
// via SMemAlloc/SMemReAlloc (FUN_6f83f8e0); the exact growth factor is NOT
// evidenced. This implementation doubles capacity on overflow --
// reconstruction, marked as such. A store created with the 1460-byte
// default never grows for a single turn chunk in the demo.

#ifndef WARCRAF_NET_DATA_STORE_H
#define WARCRAF_NET_DATA_STORE_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace warcraf::net {

// Capacity of the store the original send path builds (CDataStoreCache<1460>,
// FUN_6f2feb10). 1460 is the classic MTU-sized payload (1500 - 40).
inline constexpr size_t kDataStoreCacheCapacity = 1460;

class DataStore {
public:
    explicit DataStore(size_t capacity = kDataStoreCacheCapacity);

    // ---- writes (FUN_6f04e1d0 / FUN_6f04dfa0) ---------------------------
    // Append one little-endian dword / one byte; grow on overflow.
    // FUN_6f04e1d0: *(base - begin + pos) = val, pos += 4.
    // FUN_6f04dfa0: same shape, pos += 1.
    void WriteDword(uint32_t value);
    void WriteByte(uint8_t value);
    void WriteBytes(const uint8_t* data, size_t length);

    // ---- reads ------------------------------------------------------------
    // Sequential reads from the read position; each returns false (and
    // leaves the position unchanged) when fewer bytes remain than asked.
    // ReadThreeDwords is the vtable +0x28 "read three" (FUN_6f30ba60).
    bool ReadDword(uint32_t& out);
    bool ReadByte(uint8_t& out);
    bool ReadBytes(uint8_t* out, size_t length);
    bool ReadThreeDwords(uint32_t& a, uint32_t& b, uint32_t& c);

    // ---- positions / views ------------------------------------------------
    size_t Size() const { return writePos_; }             // bytes written
    size_t Remaining() const { return writePos_ - readPos_; }
    size_t Capacity() const { return buffer_.size(); }
    size_t ReadPosition() const { return readPos_; }
    void SetReadPosition(size_t pos);  // clamps to Size()
    void Rewind() { readPos_ = 0; }
    void Clear() { writePos_ = 0; readPos_ = 0; }
    const uint8_t* Data() const { return buffer_.data(); }
    uint8_t* MutableData() { return buffer_.data(); }

private:
    void EnsureCapacity(size_t needed);

    std::vector<uint8_t> buffer_;
    size_t writePos_ = 0;
    size_t readPos_ = 0;
};

}  // namespace warcraf::net

#endif  // WARCRAF_NET_DATA_STORE_H
