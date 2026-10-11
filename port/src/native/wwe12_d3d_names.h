// WWE '12 addresses for the XDK D3D functions that re:Blue's renderer hooks by name.
//
// Force-included into the native renderer sources (cmake/native_renderer.cmake), so
// REX_HOOK(D3DDevice_Clear, ...) overrides WWE '12's recompiled sub_82AAA390 and
// __imp__D3DDevice_Clear calls its original body. Each entry comes from docs/d3d_functions.md:
// call census in game (call rates, argument values, object types behind pointers), PM4 packets
// emitted, and the device fields the function writes. Replaces the SvR 2010 svr_d3d_names.h.
//
// WWE '12 builds its D3D shaders, vertex declarations and buffers itself (no Create* / Unlock
// calls were seen), and uses no BeginVertices/EndVertices: the hooks on those are compiled out by
// the WWE12_NO_* switches below, and shaders are discovered when first bound instead.

#pragma once

#define WWE12_NATIVE 1

// Device / frame
#define D3DDevice_Present                     sub_82AA9000  // calls Swap; never called in game
#define D3DDevice_Swap                        sub_82AA8898  // sure: the only VdSwap caller
#define D3DDevice_SynchronizeToPresentationInterval sub_82AA83D8
#define D3DDevice_Resolve                     sub_82AA1028
#define D3DDevice_Clear                       sub_82AAA390
#define D3DDevice_SetRenderTarget             sub_82AA6290
#define D3DDevice_SetDepthStencilSurface      sub_82AA6620
// D3DDevice_SetScissorRect: not identified yet.
#define WWE12_NO_SET_SCISSOR_RECT 1

// Draws. sub_82AA29B8 is BeginVertices, not DrawVertices: every caller calls it, then
// sub_82AA29A8 (16 bytes, just before it: EndVertices, the SvR 2010 layout) 0x14 bytes later;
// its arguments are (device, PrimitiveType 13, VertexCount 4, VertexStride 20). No
// DrawVertices identified yet, so its hook is compiled out.
#define SVR_NO_DRAW_VERTICES 1
#define D3DDevice_DrawIndexedVertices         sub_82AA2E78
#define D3DDevice_BeginVertices               sub_82AA29B8
#define D3DDevice_EndVertices                 sub_82AA29A8

// Buffers: read from the device at draw time (svr_geometry.cpp); no Unlock hooks.
#define WWE12_NO_BUFFER_UNLOCK 1

// Shaders and vertex declarations: no Create* calls; discovered at Set* time.
#define WWE12_NO_CREATE_SHADERS 1

// Resource description getters: not identified yet.
#define WWE12_NO_DESC_HOOKS 1

// State
#define D3DDevice_SetTexture                  sub_82A9F310
#define D3DDevice_SetVertexShader             sub_82AA2050
#define D3DDevice_SetPixelShader              sub_82AA1E48
#define D3DDevice_SetVertexDeclaration        sub_82AA2268
#define D3DDevice_SetStreamSource             sub_82AA59E8
#define D3DDevice_SetIndices                  sub_82AA5B08

// Original bodies (__imp__ = the recompiled function, for hooks that call through).
#define __imp__D3DDevice_Present              __imp__sub_82AA9000
#define __imp__D3DDevice_Swap                 __imp__sub_82AA8898
#define __imp__D3DDevice_Resolve              __imp__sub_82AA1028
#define __imp__D3DDevice_Clear                __imp__sub_82AAA390
#define __imp__D3DDevice_SetTexture           __imp__sub_82A9F310
#define __imp__D3DDevice_SetRenderTarget      __imp__sub_82AA6290
#define __imp__D3DDevice_SetDepthStencilSurface __imp__sub_82AA6620
#define __imp__D3DDevice_DrawIndexedVertices  __imp__sub_82AA2E78
#define __imp__D3DDevice_BeginVertices        __imp__sub_82AA29B8
#define __imp__D3DDevice_EndVertices          __imp__sub_82AA29A8
#define __imp__D3DDevice_SetVertexShader      __imp__sub_82AA2050
#define __imp__D3DDevice_SetPixelShader       __imp__sub_82AA1E48
#define __imp__D3DDevice_SetVertexDeclaration __imp__sub_82AA2268
#define __imp__D3DDevice_SetStreamSource      __imp__sub_82AA59E8
#define __imp__D3DDevice_SetIndices           __imp__sub_82AA5B08
