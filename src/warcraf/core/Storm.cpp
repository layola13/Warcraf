// Storm.h -- placeholder stub for include/warcraf/core/Storm.h.
//
// 占位桩，仅用于占位与编译通过，不含引擎实现。
// This file exists only so the CMake target has a translation unit
// per header; the real implementations live in Game.dll / Storm.dll /
// Mss32.dll (Warcraft III 1.27a) and are described, not reimplemented,
// by this project.

#include "warcraf/core/Storm.h"
// Stub bodies for the Storm prototypes declared in core/Storm.h. They return
// defaults and do nothing; the real implementations live in Storm.dll. See
// the header for the evidence (IDC signatures for these seven functions).
void* __stdcall SMemAlloc(int /*size*/, const char* /*fileName*/,
                          int /*fileLine*/, int /*defaultValue*/) {
    return 0;
}
BOOL __stdcall SMemFree(void* /*handle*/, const char* /*fileName*/,
                        int /*fileLine*/, int /*defaultValue*/) {
    return FALSE;
}
void* __stdcall SMemReAlloc(void* /*handle*/, const char* /*fileName*/,
                            int /*fileLine*/, int /*defaultValue*/) {
    return 0;
}
int __stdcall SStrCopy(char* /*destination*/, char* /*source*/,
                       int /*maxLength*/) {
    return 0;
}
int __stdcall SStrLen(char* /*str*/) {
    return 0;
}
int __stdcall SStrVPrintf(char* /*dest*/, size_t /*size*/,
                          const char* /*format*/, ...) {
    return 0;
}
int __stdcall SStrHash2(char* /*str*/) {
    return 0;
}
