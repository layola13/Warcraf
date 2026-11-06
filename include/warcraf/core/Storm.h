// Storm.h -- Storm.dll interface (file / memory / registry / string / log).
//
// Evidence:
//   - ~/workspace/warcraf_stage_network.md (S1): Game.dll imports 112 Storm
//     ordinals + 1 named import (SErrSetBlizzardErrorFunction); Storm.dll has
//     814 exports, ordinal base 101, 54 named (all SFile). Ordinals 548/501/403
//     verified by call-site decompilation (SLogWrite / SStrCopy / SMemFree).
//     Ordinal -> Storm VA ranges in S1 section 2.
//   - ~/workspace/warcraf_stage_mpq.md (S7): MPQ file layer. The SFile
//     object (0x60), ARCHIVEREC (0x25c) and MPQ format constants now live in
//     warcraf/io/SFile.h (split out of this header); this file keeps the
//     SMem/SReg/SErr/SCmd/SLog/SStr/SComp/SBig part of the Storm interface.
//   - ~/workspace/warcraftIII/IDA/Map Storm Imports.idc (TinkerWorX): the
//     ordinal -> name table below. Names from the IDC are third-party RE
//     labels, NOT individually call-site verified -- only 501/548/403 are.
//     Signatures are given ONLY where the IDC itself carries a type string.
//   - Storm.dll image base 0x15000000, 1105 functions (Ghidra project storm).
//
// Rules: unmapped ordinals and unknown-semantics names are listed explicitly
// and marked UNVERIFIED. No signature is invented: functions whose signature
// the IDC does not give are declared as ...-prototypes (parameter list
// unknown) so callers cannot silently assume an arity.

#ifndef WARCRAF_STORM_H
#define WARCRAF_STORM_H

#include <cstddef>  // size_t for SStrVPrintf (the header must stand alone)

#include "warcraf/core/Types.h"

// ---- Storm.dll facts (verified, S1/S7) ------------------------------------
#define STORM_BASE          0x15000000
#define STORM_EXPORT_COUNT  814
#define STORM_ORDINAL_BASE  101
#define STORM_NAMED_COUNT   54   // all named exports are SFile* (MPQ)

// The only NAMED import in Game.dll's Storm import table (S1):
//   SErrSetBlizzardErrorFunction -- all other 112 imports are by ordinal.

// ---- Prototypes with IDC-verified signatures ------------------------------
// SMemAlloc:  void* __stdcall SMemAlloc(int size, const char* fileName,
//                                       int fileLine, int defaultValue)
//   Storm ordinal 401. VA: in range 0x1502b830-0x1502bf10 (exact VA unverified).
void* __stdcall SMemAlloc(int size, const char* fileName, int fileLine, int defaultValue);
// SMemFree: BOOL __stdcall SMemFree(void* handle, const char* fileName,
//                                   int fileLine, int defaultValue)
//   Storm ordinal 403, VA 0x1502be40. VERIFIED by call site: FUN_6f304680
//   calls Ordinal_403(*(p+0x70), <file string>, -2, 0) -- a debug-tagged
//   conditional free, NOT a log call (corrects the earlier S1 misreading).
BOOL __stdcall SMemFree(void* handle, const char* fileName, int fileLine, int defaultValue);
// SMemReAlloc: void* __stdcall SMemReAlloc(void* handle, const char* fileName,
//                                           int fileLine, int defaultValue)
//   Storm ordinal 405. VA: in range 0x1502b830-0x1502bf10 (exact VA unverified).
void* __stdcall SMemReAlloc(void* handle, const char* fileName, int fileLine, int defaultValue);
// SStrCopy: int __stdcall SStrCopy(char* destination, char* source, int maxLength)
//   Storm ordinal 501, VA 0x1503a6c0. VERIFIED by call sites (FUN_6f283540
//   bounded copies: name 0x20, password 0x10).
int __stdcall SStrCopy(char* destination, char* source, int maxLength);
// SStrLen: int __stdcall SStrLen(char* str)
//   Storm ordinal 506. VA: in string cluster 0x1503a640-0x1503b890 (exact VA unverified).
int __stdcall SStrLen(char* str);
// SStrVPrintf: int __stdcall SStrVPrintf(char* dest, size_t size, const char* format, ...)
//   Storm ordinal 578.
int __stdcall SStrVPrintf(char* dest, size_t size, const char* format, ...);
// SStrHash2: int __stdcall SStrHash2(char* str)
//   Storm ordinal 590.
int __stdcall SStrHash2(char* str);

// ---- Ordinal -> name table (from Map Storm Imports.idc) -------------------
// VAs are listed ONLY where S1 gives an exact address; otherwise the S1
// address range is noted. "IDC name only" = name from the IDC, no call-site
// verification in 1.27a. Ordinals NOT in this table are unmapped (see below).
//
//   Ordinal  Name (IDC)                  Storm VA / range (S1)
//   -------  --------------------------  --------------------------------------
//   151      (unmapped)                  0x150221e0 -- name unknown, UNVERIFIED
//   251-302  SFile* ordinals -- see warcraf/io/SFile.h for the full table.
//   401      SMemAlloc                   0x1502b830-0x1502bf10 cluster (see above)
//   403      SMemFree                    0x1502be40 (VERIFIED)
//   405      SMemReAlloc                 0x1502b830-0x1502bf10 cluster (see above)
//   406      (no IDC name)               0x1502b830-0x1502bf10 cluster -- UNVERIFIED
//   421-426  SRegLoadData / LoadString / LoadValue / SaveData / SaveString / SaveValue
//                                          0x15036130-0x15036700 cluster
//   428      (unmapped)                  0x15036130-0x15036700 cluster -- UNVERIFIED
//   460      SErrInit                    0x1501c790-0x1502bd10 clusters
//   461      SErrDisplayError            (same)
//   462      SErrGetErrorStr             (same)
//   463      SErrGetLastError            (same)
//   465      SErrSetLastError            (same)
//   469      SErrRegisterHandler         (same)
//   470      SErr470                     (same) -- name only, SEMANTICS UNKNOWN
//   471      (unmapped)                  -- UNVERIFIED
//   472      SCmdGetBool                 (same)
//   474      SCmdGetString               (same)
//   476      SCmdRegisterArgList         (same)
//   479      SCmdProcessCommandLine      (same)
//   482      SMemFindNextHeap            (same)
//   496      SMem496                     0x1502c980 -- name only, SEMANTICS UNKNOWN
//   501      SStrCopy                    0x1503a6c0 (VERIFIED)
//   502      SStrHash1                   0x1503a640-0x1503b890 cluster
//   503      SStrNCat                    (same cluster)
//   504      SStrTokenize                (same cluster)
//   506      SStrLen                     (same cluster; signature above)
//   507      SStrDup                     (same cluster)
//   508      SStrCmp                     (same cluster)
//   509      SStrCmpI                    (same cluster)
//   510      SStrUpper                   (same cluster)
//   524      (unmapped)                  0x150390e0-0x15039480 cluster -- UNVERIFIED
//   525      (unmapped)                  (same cluster) -- UNVERIFIED
//   534      (unmapped)                  (same cluster) -- UNVERIFIED
//   537      (unmapped)                  (same cluster) -- UNVERIFIED
//   541      SLogClose                   0x150298e0-0x1502a330 cluster
//   542      SLogCreate                  (same cluster)
//   543      SLog543                     (same cluster) -- name only, SEMANTICS UNKNOWN
//   544      SLogDump                    (same cluster)
//   545      SLogFlush                   (same cluster)
//   548      SLogWrite                   0x1502a330 (VERIFIED: printf-style log
//                                          outlet; all checksum prints go here)
//   551      SCompCompress               0x15016120
//   552      SCompDecompress             0x15016310
//   553      SLogVWrite                  0x150298e0-0x1502a330 cluster
//   563      SErrIsDisplayingError       0x1501c550-0x1501d7f0 cluster
//   565      SErrSetLogTitleString       (same cluster)
//   567      SErrCatchUnhandledExceptions (same cluster)
//   568      (unmapped)                  (same cluster) -- UNVERIFIED
//   569      SStrChr1                    0x1503a5e0 (same VA as 571: two ordinal
//                                          entries for one function, S1)
//   570      SStrChrR1                   0x1503a610 (same VA as 572)
//   571      SStrChr2                    0x1503a5e0 (same VA as 569)
//   572      SStrChrR2                   0x1503a610 (same VA as 570)
//   574      SStrToFloat                 0x1503a5e0-0x1503af00 cluster
//   575      SStrToInt                   (same cluster)
//   576      SStrToUnsigned              (same cluster)
//   577      SStrToInt64                 (same cluster)
//   578      SStrVPrintf                 (same cluster; signature above)
//   579      SStrLower                   (same cluster)
//   581      SStrPrintf                  (same cluster)
//   584-587  SStrStrI1 / StrI2 / Str1 / Str2  (same cluster)
//   589      (unmapped)                  (same cluster) -- UNVERIFIED
//   590      SStrHash2                   (same cluster; signature above)
//   595      (unmapped)                  (same cluster) -- UNVERIFIED
//   596      (unmapped)                  (same cluster) -- UNVERIFIED
//   597      (unmapped)                  (same cluster) -- UNVERIFIED
//   601      SBigAdd                     0x1500df20-0x1500e630, ~0x60 apart
//                                          (suspected small wrappers / jump
//                                          table, S1 -- UNVERIFIED)
//   603      SBigCompare                 (same run)
//   606      SBigDel                     (same run)
//   607      SBigDiv                     (same run)
//   609      SBigFromBinary              (same run)
//   612      SBigFromUnsigned            (same run)
//   619      SBigIsPrime                 (same run)
//   621      SBigMod                     (same run)
//   622      SBigMul                     (same run)
//   624      SBigNew                     (same run)
//   628      SBigPowMod                  (same run)
//   633      SBigShl                     (same run)
//   636      SBigSub                     (same run)
//   638      SBigToBinaryBuffer          (same run)
//
// Ordinals 251-302 are ALL SFile* (MPQ file layer), NOT SNet: SNet is not in
// Game.dll's Storm import table at all (S1 correction). Network I/O goes via
// WS2_32.dll (26 ordinal imports) + internal Net functions.

enum StormOrdinal {
    ORD_SMEM_ALLOC                 = 401,
    ORD_SMEM_FREE                  = 403,  // VERIFIED
    ORD_SMEM_REALLOC               = 405,
    ORD_SREG_LOAD_DATA             = 421,
    ORD_SREG_LOAD_STRING           = 422,
    ORD_SREG_LOAD_VALUE            = 423,
    ORD_SREG_SAVE_DATA             = 424,
    ORD_SREG_SAVE_STRING           = 425,
    ORD_SREG_SAVE_VALUE            = 426,
    ORD_SERR_INIT                  = 460,
    ORD_SERR_DISPLAY_ERROR         = 461,
    ORD_SERR_GET_ERROR_STR         = 462,
    ORD_SERR_GET_LAST_ERROR        = 463,
    ORD_SERR_SET_LAST_ERROR        = 465,
    ORD_SERR_REGISTER_HANDLER      = 469,
    ORD_SERR_470                   = 470,  // semantics UNKNOWN
    ORD_SCMD_GET_BOOL              = 472,
    ORD_SCMD_GET_STRING            = 474,
    ORD_SCMD_REGISTER_ARG_LIST     = 476,
    ORD_SCMD_PROCESS_CMDLINE       = 479,
    ORD_SMEM_FIND_NEXT_HEAP        = 482,
    ORD_SMEM_496                   = 496,  // semantics UNKNOWN
    ORD_SSTR_COPY                  = 501,  // VERIFIED
    ORD_SSTR_HASH1                 = 502,
    ORD_SSTR_NCAT                  = 503,
    ORD_SSTR_TOKENIZE              = 504,
    ORD_SSTR_LEN                   = 506,
    ORD_SSTR_DUP                   = 507,
    ORD_SSTR_CMP                   = 508,
    ORD_SSTR_CMPI                  = 509,
    ORD_SSTR_UPPER                 = 510,
    ORD_SLOG_CLOSE                 = 541,
    ORD_SLOG_CREATE                = 542,
    ORD_SLOG_543                   = 543,  // semantics UNKNOWN
    ORD_SLOG_DUMP                  = 544,
    ORD_SLOG_FLUSH                 = 545,
    ORD_SLOG_WRITE                 = 548,  // VERIFIED
    ORD_SCOMP_COMPRESS             = 551,
    ORD_SCOMP_DECOMPRESS           = 552,
    ORD_SLOG_VWRITE                = 553,
    ORD_SERR_IS_DISPLAYING_ERROR   = 563,
    ORD_SERR_SET_LOG_TITLE_STRING  = 565,
    ORD_SERR_CATCH_UNHANDLED       = 567,
    ORD_SSTR_CHR1                  = 569,
    ORD_SSTR_CHRR1                 = 570,
    ORD_SSTR_CHR2                  = 571,
    ORD_SSTR_CHRR2                 = 572,
    ORD_SSTR_TO_FLOAT              = 574,
    ORD_SSTR_TO_INT                = 575,
    ORD_SSTR_TO_UNSIGNED           = 576,
    ORD_SSTR_TO_INT64              = 577,
    ORD_SSTR_VPRINTF               = 578,
    ORD_SSTR_LOWER                 = 579,
    ORD_SSTR_PRINTF                = 581,
    ORD_SSTR_STRI1                 = 584,
    ORD_SSTR_STRI2                 = 585,
    ORD_SSTR_STR1                  = 586,
    ORD_SSTR_STR2                  = 587,
    ORD_SSTR_HASH2                 = 590,
    ORD_SBIG_ADD                   = 601,
    ORD_SBIG_COMPARE               = 603,
    ORD_SBIG_DEL                   = 606,
    ORD_SBIG_DIV                   = 607,
    ORD_SBIG_FROM_BINARY           = 609,
    ORD_SBIG_FROM_UNSIGNED         = 612,
    ORD_SBIG_IS_PRIME              = 619,
    ORD_SBIG_MOD                   = 621,
    ORD_SBIG_MUL                   = 622,
    ORD_SBIG_NEW                   = 624,
    ORD_SBIG_POW_MOD               = 628,
    ORD_SBIG_SHL                   = 633,
    ORD_SBIG_SUB                   = 636,
    ORD_SBIG_TO_BINARY_BUFFER      = 638
};
// Unmapped ordinals (imported by Game.dll, no name in the IDC -- UNVERIFIED):
//   151, 428, 471, 524, 525, 534, 537, 568, 589, 595, 596, 597.
// (406 also has no IDC name; it sits in the SMem cluster 401-406.)


#endif // WARCRAF_STORM_H
