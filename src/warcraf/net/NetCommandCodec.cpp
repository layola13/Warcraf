// NetCommandCodec.h -- implementation.
//
// The wire layouts come from the per-command readers (CNetCommand.h):
// encode writes exactly what the reader reads, in read order.

#include "warcraf/net/NetCommandCodec.h"

namespace warcraf::net {

bool HasEvidencedWireLayout(uint8_t command) {
    switch (command) {
        case 0x1a:
        case 0x1b:
        case 0x1c:
        case 0x17:
        case 0x16:
            return true;
        default:
            return false;
    }
}

bool EncodeCommand(const CNetCommandBase& command, DataStore& out) {
    if (!HasEvidencedWireLayout(command.command)) {
        return false;
    }
    out.WriteByte(command.command);
    switch (command.command) {
        case 0x1a:
        case 0x1b:
        case 0x1c:
            // One dword (FUN_6f83ee60 / FUN_6f83f010 / FUN_6f83efd0).
            out.WriteDword(command.value1);
            return true;
        case 0x17:
            // 1 dword + 1 byte + 2 dwords (FUN_6f83f090), in read order.
            out.WriteDword(command.value1);
            out.WriteByte(command.flag);
            out.WriteDword(command.value2);
            out.WriteDword(command.value3);
            return true;
        case 0x16:
            // 1 byte + dword (FUN_6f83f060). PARTIAL: the reader also
            // calls FUN_6f83f530 between the two reads; that call's wire
            // effect is unknown and is not reproduced here.
            out.WriteByte(command.flag);
            out.WriteDword(command.value1);
            return true;
        default:
            return false;
    }
}

bool DecodeCommand(DataStore& in, CNetCommandBase& out) {
    uint8_t commandByte = 0;
    if (!in.ReadByte(commandByte)) {
        return false;
    }
    if (!HasEvidencedWireLayout(commandByte)) {
        return false;
    }
    out = CNetCommandBase{};
    out.command = commandByte;
    switch (commandByte) {
        case 0x1a:
        case 0x1b:
        case 0x1c:
            return in.ReadDword(out.value1);
        case 0x17: {
            uint8_t flag = 0;
            if (!in.ReadDword(out.value1) || !in.ReadByte(flag) ||
                !in.ReadDword(out.value2) || !in.ReadDword(out.value3)) {
                return false;
            }
            out.flag = flag;
            return true;
        }
        case 0x16: {
            uint8_t flag = 0;
            if (!in.ReadByte(flag) || !in.ReadDword(out.value1)) {
                return false;
            }
            out.flag = flag;
            return true;
        }
        default:
            return false;
    }
}

bool EncodeTurnChunk(const std::vector<CNetCommandBase>& commands,
                     DataStore& out) {
    // Reconstruction (see the header): [u32 length][commands][0x1f].
    // The length covers the command bytes only, not the length field
    // itself and not the end marker.
    DataStore body;
    for (const CNetCommandBase& command : commands) {
        if (!EncodeCommand(command, body)) {
            return false;
        }
    }
    out.WriteDword(static_cast<uint32_t>(body.Size()));
    out.WriteBytes(body.Data(), body.Size());
    out.WriteByte(0x1f);  // end flag (0x1f batch path, S2/S3)
    return true;
}

bool DecodeTurnChunk(DataStore& in, std::vector<CNetCommandBase>& out) {
    out.clear();
    uint32_t length = 0;
    if (!in.ReadDword(length)) {
        return false;
    }
    // The command section must fit in the remaining bytes, plus the
    // one-byte end marker.
    if (length > in.Remaining() || in.Remaining() - length < 1) {
        return false;
    }
    size_t sectionEnd = in.ReadPosition() + length;
    while (in.ReadPosition() < sectionEnd) {
        CNetCommandBase command{};
        if (!DecodeCommand(in, command)) {
            return false;
        }
        out.push_back(command);
    }
    if (in.ReadPosition() != sectionEnd) {
        return false;  // a command overran the declared section
    }
    uint8_t endMarker = 0;
    if (!in.ReadByte(endMarker) || endMarker != 0x1f) {
        return false;
    }
    return true;
}

}  // namespace warcraf::net
