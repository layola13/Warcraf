// NetCommandCodec.h -- CNetCommandBase wire encode/decode.
//
// Evidence:
//   - ~/workspace/Warcraf/include/warcraf/net/CNetCommand.h (S2/S3):
//     CNetCommandBase is 24 bytes (value1/2/3 dwords at +0x8/+0xc/+0x10,
//     command byte +0x14, flag byte +0x15); the per-command readers
//     (FUN_6f83exxx) that define the wire layout of each command number;
//     the 0x1e/0x1f batch path (length + read + execute; 0x1f sets the
//     end flag).
//   - ~/workspace/warcraf_stage_simulation.md (S3): reader table --
//     0x1a/0x1b/0x1c read one dword (FUN_6f83ee60/6f83f010/6f83efd0);
//     0x16 reads 1 byte + a call + a dword (FUN_6f83f060);
//     0x17 reads 1 dword + 1 byte + 2 dwords (FUN_6f83f090);
//     0x23 reads 1 dword + 1 byte + variable data + a counted list
//     (FUN_6f83f380).
//   - DataStore primitives: NetDataStore.h (FUN_6f04e1d0/6f04dfa0).
//
// The codec is built FROM THE READERS: a reader that reads [dword, byte,
// dword, dword] implies the writer wrote them in that order. The field
// mapping (which read lands in value1/value2/value3/flag) follows the
// read order and is marked as reconstruction where the reader does not
// name its destination.
//
// What is NOT implemented (evidence gaps -- the codec refuses instead of
// inventing):
//   - 0x23: the variable-length list's count lives "at +0x1b" of the
//     reader's frame, not at an evidenced wire offset, so the wire
//     layout of the list cannot be reconstructed from the reader.
//   - Every other command number (0x10-0x15, 0x18-0x22, 0x24-0x2e,
//     0x30-0x31, ...): no reader is evidenced for them in our sources.
//   - Command-number SEMANTICS (which number is move/attack/cast):
//     UNVERIFIED (CNetCommand.h). Commands are opaque numbered payloads
//     here; the demo only checks round-trip fidelity, never game meaning.
//
// Turn-chunk framing (EncodeTurnChunk / DecodeTurnChunk) is a
// reconstruction from the 0x1e/0x1f batch-path description (a length is
// read first -- FUN_6f04d420 -- then the batch, and 0x1f sets the end
// flag): [u32 byte-length][commands...][0x1f end marker]. The decoder
// verifies both the length and the end marker. This framing is NOT
// claimed to be the original's.

#ifndef WARCRAF_NET_COMMAND_CODEC_H
#define WARCRAF_NET_COMMAND_CODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "warcraf/net/CNetCommand.h"
#include "warcraf/net/NetDataStore.h"

namespace warcraf::net {

// True only for command numbers whose wire layout is evidenced by a
// reader (see the header comment): 0x1a, 0x1b, 0x1c, 0x17, 0x16.
bool HasEvidencedWireLayout(uint8_t command);

// Encodes one command: the command byte, then the payload its reader
// reads, in read order. Returns false when the command has no evidenced
// wire layout (nothing is written). Field mapping (reconstruction from
// read order):
//   0x1a/0x1b/0x1c: [cmd][dword value1]
//   0x17:          [cmd][dword value1][byte flag][dword value2][dword value3]
//   0x16:          [cmd][byte flag][dword value1] -- PARTIAL: the reader
//                  also calls FUN_6f83f530 (identity UNVERIFIED) between
//                  the byte and the dword; the call's wire effect is unknown.
bool EncodeCommand(const CNetCommandBase& command, DataStore& out);

// Decodes one command from the store's current read position (command
// byte first, then the reader-defined payload). Returns false on an
// unknown command number, a truncated payload, or a failed read -- the
// store's read position is left where the failed read stopped.
bool DecodeCommand(DataStore& in, CNetCommandBase& out);

// Encodes a turn's command list as one chunk:
//   [u32 byte-length of the command section][commands...][0x1f end marker]
// Returns false when any command lacks an evidenced layout.
bool EncodeTurnChunk(const std::vector<CNetCommandBase>& commands,
                     DataStore& out);

// Decodes a chunk written by EncodeTurnChunk. Verifies the length prefix
// and the 0x1f end marker; returns false on any mismatch, truncation, or
// unknown command number.
bool DecodeTurnChunk(DataStore& in, std::vector<CNetCommandBase>& out);

}  // namespace warcraf::net

#endif  // WARCRAF_NET_COMMAND_CODEC_H
