// D3D12 exe: the build-time pre-linked spec-constant variants (re:Blue's reblue_prelink tool
// writes generated/linked_shader_cache.cpp) are not made for WWE '12: none listed here, so
// guest_shaders.cpp links every spec-constant shader at run time (shader_linker.cpp, DXC), then
// the pipeline cache keeps the result.
#include "gpu/shaders/linked_shader_cache.h"

LinkedShaderCacheEntry g_linkedShaderCacheEntries[1] = {{0, 0, 0, 0}};
const size_t g_linkedShaderCacheEntryCount = 0;
const u8 g_compressedLinkedDxilCache[1] = {0};
const size_t g_linkedDxilCacheCompressedSize = 0;
const size_t g_linkedDxilCacheDecompressedSize = 0;
