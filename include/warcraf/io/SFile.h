// SFile.h -- Storm MPQ file layer: SFile objects, ARCHIVEREC, MPQ format.
//
// Evidence:
//   - ~/workspace/warcraf_stage_mpq.md (S7): SFile object layout (0x60),
//     ARCHIVEREC layout (0x25c), MPQ header/table formats, signature verify
//     (Storm ordinal 251, FUN_15020ea0), boot mount chain (FUN_6f020e20).
//   - ~/workspace/warcraf_stage_network.md (S1): Game.dll imports the SFile
//     ordinals 251-302; ordinals 251-302 are ALL SFile* (NOT SNet).
//   - ~/workspace/warcraftIII/IDA/Map Storm Imports.idc (TinkerWorX): the
//     ordinal -> name mapping. Names are third-party RE labels, not
//     individually call-site verified.
//   - Storm.dll image base 0x15000000. SFile functions are called by ordinal;
//     no signatures are given here except where the IDC carries a type string
//     (none for SFile*), so only the ordinal table + object layouts are
//     declared -- no prototypes are invented.
//
// Split note: this file holds the SFile/MPQ part that used to live in
// core/Storm.h; Storm.h keeps the SMem/SReg/SErr/SCmd/SLog/SStr/SComp/SBig
// part. The two together cover the whole Storm import table.

#ifndef WARCRAF_SFILE_H
#define WARCRAF_SFILE_H

#include "warcraf/core/Types.h"

// ---- SFile ordinal -> name table (IDC; S1 VA ranges) ----------------------
//   Ordinal  Name (IDC)                  Storm VA / range (S1)
//   -------  --------------------------  --------------------------------------
//   251      SFileAuthenticateArchive    0x15020ea0 (signature verify, S7)
//   252      SFileCloseArchive           0x15021540
//   253      SFileCloseFile              0x15021610
//   263      SFileEnableDirectAccess     0x15022220-0x150238e0 cluster
//   264      SFileGetFileArchive         (same cluster; exact VA unverified)
//   265      SFileGetFileSize            (same cluster)
//   266      SFileOpenArchive            (same cluster)
//   267      SFileOpenFile               (same cluster; used by audio file
//                                          callbacks, S8, and FUN_6f144a00, S7)
//   268      SFileOpenFileEx             (same cluster)
//   269      SFileReadFile               (same cluster; audio file callbacks, S8)
//   270      SFileSetBasePath            (same cluster)
//   271      SFileSetFilePointer         (same cluster; audio file callbacks, S8)
//   272      SFileSetLocale              (same cluster)
//   275      SFileGetArchiveName         (same cluster)
//   279      SFileLoadFile               (same cluster)
//   280      SFileUnloadFile             (same cluster)
//   281      SFileLoadFileEx             (same cluster)
//   288      SFileFileExists             (same cluster)
//   289      SFileFileExistsEx           (same cluster)
//   293      SFileOpenFileAsArchive      (same cluster)
//   294      SFileGetLocale              (same cluster)
//   295      SFileRegisterLoadNotifyProc (same cluster)
//   300      SFileOpenPathAsArchive      (same cluster)
//   302      StormGetInstance            0x1503c110

enum SFileOrdinal {
    ORD_SFILE_AUTHENTICATE_ARCHIVE = 251,
    ORD_SFILE_CLOSE_ARCHIVE        = 252,
    ORD_SFILE_CLOSE_FILE           = 253,
    ORD_SFILE_ENABLE_DIRECT_ACCESS = 263,
    ORD_SFILE_GET_FILE_ARCHIVE     = 264,
    ORD_SFILE_GET_FILE_SIZE        = 265,
    ORD_SFILE_OPEN_ARCHIVE         = 266,
    ORD_SFILE_OPEN_FILE            = 267,
    ORD_SFILE_OPEN_FILE_EX         = 268,
    ORD_SFILE_READ_FILE            = 269,
    ORD_SFILE_SET_BASE_PATH        = 270,
    ORD_SFILE_SET_FILE_POINTER     = 271,
    ORD_SFILE_SET_LOCALE           = 272,
    ORD_SFILE_GET_ARCHIVE_NAME     = 275,
    ORD_SFILE_LOAD_FILE            = 279,
    ORD_SFILE_UNLOAD_FILE          = 280,
    ORD_SFILE_LOAD_FILE_EX         = 281,
    ORD_SFILE_FILE_EXISTS          = 288,
    ORD_SFILE_FILE_EXISTS_EX       = 289,
    ORD_SFILE_OPEN_FILE_AS_ARCHIVE = 293,
    ORD_SFILE_GET_LOCALE           = 294,
    ORD_SFILE_REGISTER_LOAD_NOTIFY = 295,
    ORD_SFILE_OPEN_PATH_AS_ARCHIVE = 300,
    ORD_STORM_GET_INSTANCE         = 302
};

// ---- SFile object (S7, 0x60 bytes) ----------------------------------------
// Five dispatch types (S7): 0 = plain, 1 = .bz zlib inflate, 2 = archive
// file, 3 = specified archive file, 4 = resource. Field meanings verified
// in S7; the full constructor behaviour is unverified.
struct SFile { /* size = 0x60 */
    _DWORD type;         // +0x00: SFile type 0-4 (see above)
    void*  fileHandle;   // +0x04: FILE*
    _DWORD archiveRec;   // +0x08: archive record pointer (ARCHIVEREC*)
    char*  name;         // +0x0c: file name pointer
    _DWORD field_10;
    _DWORD field_14;
    _DWORD field_18;
    _DWORD field_1C;
    _DWORD field_20;
    _DWORD critSect;     // +0x24: SCritSect (sync primitive, S1/S7)
    _DWORD field_28;
    _DWORD field_2C;
    _DWORD field_30;
    _DWORD field_34;
    _DWORD field_38;
    _DWORD fileRecord;   // +0x3c: file record
    _DWORD resource;     // +0x40: resource pointer
    _DWORD field_44;
    _DWORD field_48;
    _DWORD field_4C;
    _DWORD field_50;
    _DWORD field_54;
    _DWORD field_58;
    _DWORD field_5C;
};

// ---- ARCHIVEREC (S7, 0x25c bytes) -----------------------------------------
struct ArchiveRec { /* size = 0x25c */
    _DWORD field_0;
    _DWORD field_4;
    char*  path;         // +0x08: archive path
    _DWORD field_0C;
    _DWORD field_10;
    _DWORD field_14;
    _DWORD field_18;
    _DWORD field_1C;
    _DWORD field_20;
    _DWORD field_24;
    _DWORD field_28;
    _DWORD field_2C;
    _DWORD field_30;
    _DWORD field_34;
    _DWORD field_38;
    _DWORD field_3C;
    _DWORD field_40;
    _DWORD field_44;
    _DWORD field_48;
    _DWORD field_4C;
    _DWORD field_50;
    _DWORD field_54;
    _DWORD field_58;
    _DWORD field_5C;
    _DWORD field_60;
    _DWORD field_64;
    _DWORD field_68;
    _DWORD field_6C;
    _DWORD field_70;
    _DWORD field_74;
    _DWORD field_78;
    _DWORD field_7C;
    _DWORD field_80;
    _DWORD field_84;
    _DWORD field_88;
    _DWORD field_8C;
    _DWORD field_90;
    _DWORD field_94;
    _DWORD field_98;
    _DWORD field_9C;
    _DWORD field_A0;
    _DWORD field_A4;
    _DWORD field_A8;
    _DWORD field_AC;
    _DWORD field_B0;
    _DWORD field_B4;
    _DWORD field_B8;
    _DWORD field_BC;
    _DWORD field_C0;
    _DWORD field_C4;
    _DWORD field_C8;
    _DWORD field_CC;
    _DWORD field_D0;
    _DWORD field_D4;
    _DWORD field_D8;
    _DWORD field_DC;
    _DWORD field_E0;
    _DWORD field_E4;
    _DWORD field_E8;
    _DWORD field_EC;
    _DWORD field_F0;
    _DWORD field_F4;
    _DWORD field_F8;
    _DWORD field_FC;
    _DWORD field_100;
    _DWORD field_104;
    _DWORD field_108;
    _DWORD handle;       // +0x10c: archive handle
    _DWORD flags;        // +0x110
    _DWORD field_114;
    _DWORD priority;     // +0x118: mount priority (patch chain)
    _DWORD field_11C;
    _DWORD field_120;
    _DWORD sectorSize;   // +0x124
    _DWORD sectorBuf;    // +0x128: sector buffer pointer
    _DWORD field_12C;
    _DWORD base;         // +0x130: base offset
    _DWORD fileSize;     // +0x134
    _DWORD header;       // +0x138: MPQ header pointer
    _DWORD blockTable;   // +0x13c: block table pointer
    _DWORD hashTable;    // +0x140: hash table pointer
    _DWORD refCount1;    // +0x144
    _DWORD refCount2;    // +0x148
    _DWORD field_14C;
    _DWORD field_150;
    _DWORD field_154;
    _DWORD field_158;
    _DWORD field_15C;
    _DWORD field_160;
    _DWORD field_164;
    _DWORD field_168;
    _DWORD field_16C;
    _DWORD field_170;
    _DWORD field_174;
    _DWORD field_178;
    _DWORD field_17C;
    _DWORD field_180;
    _DWORD field_184;
    _DWORD field_188;
    _DWORD field_18C;
    _DWORD field_190;
    _DWORD field_194;
    _DWORD field_198;
    _DWORD field_19C;
    _DWORD field_1A0;
    _DWORD field_1A4;
    _DWORD field_1A8;
    _DWORD field_1AC;
    _DWORD field_1B0;
    _DWORD field_1B4;
    _DWORD field_1B8;
    _DWORD field_1BC;
    _DWORD field_1C0;
    _DWORD field_1C4;
    _DWORD field_1C8;
    _DWORD field_1CC;
    _DWORD field_1D0;
    _DWORD field_1D4;
    _DWORD field_1D8;
    _DWORD field_1DC;
    _DWORD field_1E0;
    _DWORD field_1E4;
    _DWORD field_1E8;
    _DWORD field_1EC;
    _DWORD field_1F0;
    _DWORD field_1F4;
    _DWORD field_1F8;
    _DWORD field_1FC;
    _DWORD field_200;
    _DWORD field_204;
    _DWORD field_208;
    _DWORD field_20C;
    _DWORD field_210;
    _DWORD field_214;
    _DWORD field_218;
    _DWORD field_21C;
    _DWORD field_220;
    _DWORD field_224;
    _DWORD field_228;
    _DWORD field_22C;
    _DWORD field_230;
    _DWORD field_234;
    _DWORD field_238;
    _DWORD field_23C;
    _DWORD field_240;
    _DWORD field_244;
    _DWORD field_248;
    _DWORD field_24C;
    _DWORD field_250;
    _DWORD field_254;
    _DWORD refCount3;    // +0x258
};

// ---- MPQ format constants (S7, verified) ----------------------------------
#define MPQ_MAGIC        0x1a51504d  // "MPQ\x1a"
#define MPQ_HASH_ENTRY   16          // hash table entry, bytes
#define MPQ_BLOCK_DISK   16          // block table entry on disk, bytes
#define MPQ_BLOCK_MEM    0x2c        // block table entry in memory, bytes
// Sector size = 0x200 << (header byte 0x0e & 0x1f).
// Signature verify: Storm ordinal 251 (FUN_15020ea0), CryptoAPI container
// "Blizzard_Storm", resource BLIZZARDKEY #256, "(signature)" file.
// Boot mount (FUN_6f020e20): War3.mpq pri 0, War3X.mpq 1, War3XLocal.mpq 2,
// War3Patch.mpq 8, (teen) -> t.mpq 17 / tx.mpq 18.
// CORRUPT_* error table: codes 0-0x1d (FUN_6f7bd750).
// No write-MPQ function located -- UNVERIFIED whether one exists.// ---- Compile-time layout checks (verified offsets only) -------------------
// SFile / ArchiveRec contain pointers: 32-bit target only (see Types.h).
#include <cstddef>
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(SFile) == 0x60, "SFile must be 0x60 bytes");
static_assert(offsetof(SFile, type) == 0x00, "SFile type +0x00");
static_assert(offsetof(SFile, fileHandle) == 0x04, "SFile fileHandle +0x04");
static_assert(offsetof(SFile, archiveRec) == 0x08, "SFile archiveRec +0x08");
static_assert(offsetof(SFile, name) == 0x0c, "SFile name +0x0c");
static_assert(offsetof(SFile, critSect) == 0x24, "SFile critSect +0x24");
static_assert(offsetof(SFile, fileRecord) == 0x3c, "SFile fileRecord +0x3c");
static_assert(offsetof(SFile, resource) == 0x40, "SFile resource +0x40");
static_assert(sizeof(ArchiveRec) == 0x25c, "ArchiveRec must be 0x25c bytes");
static_assert(offsetof(ArchiveRec, path) == 0x08, "ArchiveRec path +0x08");
static_assert(offsetof(ArchiveRec, handle) == 0x10c, "ArchiveRec handle +0x10c");
static_assert(offsetof(ArchiveRec, flags) == 0x110, "ArchiveRec flags +0x110");
static_assert(offsetof(ArchiveRec, priority) == 0x118, "ArchiveRec priority +0x118");
static_assert(offsetof(ArchiveRec, sectorSize) == 0x124, "ArchiveRec sectorSize +0x124");
static_assert(offsetof(ArchiveRec, sectorBuf) == 0x128, "ArchiveRec sectorBuf +0x128");
static_assert(offsetof(ArchiveRec, base) == 0x130, "ArchiveRec base +0x130");
static_assert(offsetof(ArchiveRec, fileSize) == 0x134, "ArchiveRec fileSize +0x134");
static_assert(offsetof(ArchiveRec, header) == 0x138, "ArchiveRec header +0x138");
static_assert(offsetof(ArchiveRec, blockTable) == 0x13c, "ArchiveRec blockTable +0x13c");
static_assert(offsetof(ArchiveRec, hashTable) == 0x140, "ArchiveRec hashTable +0x140");
static_assert(offsetof(ArchiveRec, refCount1) == 0x144, "ArchiveRec refCount1 +0x144");
static_assert(offsetof(ArchiveRec, refCount2) == 0x148, "ArchiveRec refCount2 +0x148");
static_assert(offsetof(ArchiveRec, refCount3) == 0x258, "ArchiveRec refCount3 +0x258");
#endif
#endif // WARCRAF_SFILE_H
