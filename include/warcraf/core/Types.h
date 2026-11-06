// Types.h -- shared base types for the Warcraf 1.27a header set.
//
// Evidence: TinkerWorX/warcraftIII headers (MIT, 2017 Nikolaj Mariager) use
// _DWORD / int / float / void* / char* throughout but never define _DWORD
// (it comes from the IDA prelude). This header supplies those definitions so
// the headers in this directory stand alone.
//
// Target: Warcraft III 1.27.0.52240, 32-bit PE, MSVC 2013.
//   Game.dll image base 0x6f000000 (only export: GameMain @ 0x6f0297d0).
//   Storm.dll image base 0x15000000. Mss32.dll image base 0x21100000.
//   All addresses in this header set are virtual addresses under those bases.
//
// Convention (TinkerWorX style):
//   - Plain C++ classes, single inheritance, fields named field_XX where XX is
//     the hex offset of the field within the class.
//   - NO #pragma pack: 4-byte members on 32-bit, natural alignment.
//   - /* size = N */ comments give the object size in bytes (or the vtable
//     entry count for *_VTable structs, as in the TinkerWorX originals).
//   - Everything not backed by a stage doc, a Ghidra dump, or an IDC script is
//     a field_XX / funcN placeholder and is marked UNVERIFIED.
//
// Deviation from the TinkerWorX originals (deliberate, see headers/README.md):
//   - Include guards added (the originals have none; Warcraf.h includes all
//     headers together, so guards are required to compile).
//   - __stdcall / __fastcall / __thiscall are defined empty on non-MSVC
//     compilers so the headers syntax-check with g++ on Linux. On MSVC they
//     keep their real meaning.

#ifndef WARCRAF_TYPES_H
#define WARCRAF_TYPES_H

// ---- MSVC calling-convention keywords (no-ops off MSVC) ------------------
#ifndef _MSC_VER
#define __stdcall
#define __fastcall
#define __thiscall
#define __cdecl
#endif

// ---- IDA/TinkerWorX base types -------------------------------------------
typedef unsigned int   _DWORD;   // 32-bit unsigned (dword)
typedef unsigned short _WORD;    // 16-bit unsigned
typedef unsigned char  _BYTE;    // 8-bit unsigned
typedef int            BOOL;     // Win32 BOOL (0 = FALSE, non-zero = TRUE)
typedef unsigned int   UINT;
typedef int            INT;
typedef long           LONG;
typedef unsigned long  DWORD;
typedef unsigned short WORD;
typedef unsigned char  BYTE;
typedef char           CHAR;
typedef void*          HANDLE;
typedef void*          HMODULE;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

// ---- 32-bit layout guard ---------------------------------------------------
// Every size/offset claim in this header set describes the 32-bit War3 1.27a
// layout (pointers are 4 bytes). static_asserts that involve a pointer are
// only meaningful on a 32-bit target, so they are guarded by this macro;
// pointer-free asserts (all-_DWORD/_BYTE structs) fire on any host. A
// 64-bit build therefore checks only the pointer-free subset -- full layout
// verification needs a 32-bit target (MSVC x86, or g++ -m32 with multilib).
#if (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4) || defined(_M_IX86)
#define WARCRAF_32BIT_LAYOUT 1
#else
#define WARCRAF_32BIT_LAYOUT 0
#endif

// ---- Image bases (verified: S12 summary, ~/workspace/warcraf_architecture_complete.md) ----
#define WARCRAF_GAME_BASE   0x6f000000  // Game.dll
#define WARCRAF_STORM_BASE  0x15000000  // Storm.dll
#define WARCRAF_MSS_BASE    0x21100000  // Mss32.dll (Miles Sound System 6.1d)

// ---- Engine-wide constants (verified) -------------------------------------
// Byte-mixing constant shared by the S0 event-pump rolling checksum, the S11
// string-hash seed (FUN_6f1ae7a0) and the rand-state fold (FUN_6f3131a0).
#define WARCRAF_MIX_CONST   0x7a2d
// Appears both as the S9 replay version constant and the S11 rand-checksum
// XOR constant (FUN_6f3130c0); same value, different uses -- not proven linked.
#define WARCRAF_CONST_17AB  0x17ab

#endif // WARCRAF_TYPES_H
