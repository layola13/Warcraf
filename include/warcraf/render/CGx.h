// CGx.h -- Gx rendering abstraction (D3D9 / OpenGL dual backend).
//
// Evidence: ~/workspace/warcraf_stage_render.md (S5).
//   - CGxDevice base (vtable) with CGxDeviceD3d (0x1ef8 B) / CGxDeviceOpenGl
//     (0xbfc B); factory FUN_6f0e2f70 (param 0 = OpenGL, 1 = D3D; singleton
//     DAT_6fbc5420); backend selector FUN_6f0e3190; registry key "Gfx OpenGL"
//     (0 = OpenGL, 1 = D3D).
//   - D3D9 dynamically loaded (FUN_6f0ec530: LoadLibrary d3d9.dll +
//     Direct3DCreate9(0x20); no d3d9.dll in the import table); OpenGL
//     statically imported (OPENGL32.dll).
//   - GL capability probe FUN_6f0ebb40 (version parse + ARB multitexture
//     pointer table); texture upload FUN_6f0eb600; scene readback D3D
//     FUN_6f0ed080 / OGL FUN_6f0f0cc0 (isomorphic); graphics init
//     FUN_6f027e20 (393 B).
//   - RTTI: CGxDevice / CGxDeviceD3d / CGxDeviceOpenGl / CGxMesh / CGxMat /
//     CGxStateBom / CGxAppRenderState / CGxVertexPNT0 (et al.).
//
// Device object layouts, capability-flag semantics, D3D format codes, BLP
// decoding, MDX loading, font rasterisation/atlas packing, GxVideo, lighting
// and the render-frame-rate vs 25 ms step decoupling are UNVERIFIED.

#ifndef WARCRAF_CGX_H
#define WARCRAF_CGX_H

#include "warcraf/core/Types.h"

// ---- Backend selection (verified, S5) ---------------------------------------
enum GxBackend {
    GX_OPENGL = 0,  // factory param 0; registry "Gfx OpenGL" = 0
    GX_D3D9   = 1   // factory param 1; registry "Gfx OpenGL" = 1
};
// Factory: FUN_6f0e2f70(backend) -> CGxDevice* (singleton DAT_6fbc5420).
// Selector: FUN_6f0e3190. Graphics init: FUN_6f027e20 (393 B).
// D3D9 loader: FUN_6f0ec530 (LoadLibrary "d3d9.dll" + Direct3DCreate9(0x20)).

// ---- CGxDevice (base; vtable-based, S5) -------------------------------------
// The device is used through its vtable; the per-frame scene submit call
// chain from the main loop is UNVERIFIED (S12 residual point 1 -- the
// "last hop" from the engine loop to CGxDevice goes through a vtable).
// Object layout UNVERIFIED; only the subclass sizes are evidenced.
struct CGxDevice { /* size = UNVERIFIED */
    void* vtable;      // +0x00: vtable pointer (assumed; UNVERIFIED)
};
struct CGxDeviceD3d { /* size = 0x1ef8 */
    CGxDevice base;    // base subobject (vtable pointer only, as declared)
    _DWORD field_4[0x7be];  // remainder to 0x1ef8 (contents UNVERIFIED)
};
struct CGxDeviceOpenGl { /* size = 0xbfc */
    CGxDevice base;    // base subobject (vtable pointer only, as declared)
    _DWORD field_4[0x2fe];  // remainder to 0xbfc (contents UNVERIFIED)
};
// NOTE: the remainder arrays above only pin the TOTAL SIZE; the real field
// layout inside each device class is UNVERIFIED. The array counts assume the
// base subobject is exactly the 4-byte vtable pointer declared above -- that
// assumption itself is UNVERIFIED and is stated here so it is not mistaken
// for evidence.

// ---- Other Gx RTTI types (names only, S5) -----------------------------------
//   CGxMesh, CGxMat, CGxStateBom, CGxAppRenderState, CGxVertexPNT0 (et al.).
// Layouts UNVERIFIED.
struct CGxMesh { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};
struct CGxMat { /* size = UNVERIFIED */
    void* vtable;      // +0x00 (assumed; UNVERIFIED)
    _DWORD field_4;    // UNVERIFIED
};

// ---- Key functions (verified addresses, S5) ---------------------------------
//   FUN_6f0e2f70  device factory (param 0 = OpenGL, 1 = D3D)
//   FUN_6f0e3190  backend selector
//   FUN_6f0ec530  D3D9 dynamic load (d3d9.dll + Direct3DCreate9(0x20))
//   FUN_6f0ebb40  GL capability probe (version parse + ARB multitexture)
//   FUN_6f0eb600  texture upload
//   FUN_6f0ed080  scene readback (D3D) / FUN_6f0f0cc0 (OpenGL) -- isomorphic
//   FUN_6f027e20  graphics init (393 B)
// UI integration (S6/S12): CBackdropFrame primitives are submitted into the
// CGxDevice batch via FUN_6f12a6a0; font texture atlases back CTextFrame.
// ---- Compile-time layout checks (verified sizes only) ---------------------
// Both device structs contain a vtable pointer: 32-bit target only (Types.h).
#if WARCRAF_32BIT_LAYOUT
static_assert(sizeof(CGxDeviceD3d) == 0x1ef8, "CGxDeviceD3d must be 0x1ef8 bytes");
static_assert(sizeof(CGxDeviceOpenGl) == 0xbfc, "CGxDeviceOpenGl must be 0xbfc bytes");
#endif

#endif // WARCRAF_CGX_H
