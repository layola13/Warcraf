// NetDataStore.h -- implementation.
//
// Primitives follow FUN_6f04e1d0 (write dword) / FUN_6f04dfa0 (write byte):
// append at the write position and advance it; grow on overflow. The
// growth itself is a reconstruction (see the header).

#include "warcraf/net/NetDataStore.h"

#include <algorithm>
#include <cstring>

namespace warcraf::net {

DataStore::DataStore(size_t capacity) : buffer_(capacity) {}

void DataStore::EnsureCapacity(size_t needed) {
    if (needed <= buffer_.size()) {
        return;
    }
    // Reconstruction: the original grows via its vtable (+0x0c) through
    // SMemAlloc/SMemReAlloc (FUN_6f83f8e0); the growth factor is not
    // evidenced, so this doubles, which keeps amortised append linear.
    size_t grown = buffer_.size() == 0 ? needed : buffer_.size();
    while (grown < needed) {
        grown *= 2;
    }
    buffer_.resize(grown);
}

void DataStore::WriteDword(uint32_t value) {
    EnsureCapacity(writePos_ + 4);
    buffer_[writePos_] = static_cast<uint8_t>(value & 0xff);
    buffer_[writePos_ + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
    buffer_[writePos_ + 2] = static_cast<uint8_t>((value >> 16) & 0xff);
    buffer_[writePos_ + 3] = static_cast<uint8_t>((value >> 24) & 0xff);
    writePos_ += 4;
}

void DataStore::WriteByte(uint8_t value) {
    EnsureCapacity(writePos_ + 1);
    buffer_[writePos_] = value;
    writePos_ += 1;
}

void DataStore::WriteBytes(const uint8_t* data, size_t length) {
    if (length == 0) {
        return;
    }
    EnsureCapacity(writePos_ + length);
    std::memcpy(buffer_.data() + writePos_, data, length);
    writePos_ += length;
}

bool DataStore::ReadDword(uint32_t& out) {
    if (Remaining() < 4) {
        return false;
    }
    out = static_cast<uint32_t>(buffer_[readPos_]) |
          (static_cast<uint32_t>(buffer_[readPos_ + 1]) << 8) |
          (static_cast<uint32_t>(buffer_[readPos_ + 2]) << 16) |
          (static_cast<uint32_t>(buffer_[readPos_ + 3]) << 24);
    readPos_ += 4;
    return true;
}

bool DataStore::ReadByte(uint8_t& out) {
    if (Remaining() < 1) {
        return false;
    }
    out = buffer_[readPos_];
    readPos_ += 1;
    return true;
}

bool DataStore::ReadBytes(uint8_t* out, size_t length) {
    if (Remaining() < length) {
        return false;
    }
    std::memcpy(out, buffer_.data() + readPos_, length);
    readPos_ += length;
    return true;
}

bool DataStore::ReadThreeDwords(uint32_t& a, uint32_t& b, uint32_t& c) {
    // The vtable +0x28 "read three" (FUN_6f30ba60): three sequential
    // dword reads. Implemented as three reads so a short buffer fails
    // atomically at the first missing dword.
    size_t saved = readPos_;
    if (!ReadDword(a) || !ReadDword(b) || !ReadDword(c)) {
        readPos_ = saved;
        return false;
    }
    return true;
}

void DataStore::SetReadPosition(size_t pos) {
    readPos_ = std::min(pos, writePos_);
}

}  // namespace warcraf::net
