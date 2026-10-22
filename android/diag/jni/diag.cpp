// WWE '12 PC - Android device check. Native half: Vulkan capabilities the port's renderer
// (plume + re:Blue) relies on, the 4 GB guest memory arena the ReXGlue runtime maps, CPU details
// and a small benchmark. Returns one plain-text report; MainActivity adds the Java-side facts.
#include <android/sharedmem.h>
#include <jni.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "bench.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace {

std::string g_out;
std::vector<std::string> g_fail, g_warn;

void P(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  g_out += buf;
  g_out += '\n';
}

const char *YN(bool b) { return b ? "yes" : "NO"; }

std::string Ver(uint32_t v) {
  return std::to_string(VK_API_VERSION_MAJOR(v)) + "." + std::to_string(VK_API_VERSION_MINOR(v)) +
         "." + std::to_string(VK_API_VERSION_PATCH(v));
}

// ---- CPU -------------------------------------------------------------------------------------

void CheckCpu() {
  P("== CPU");
  int n = static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));
  P("cores: %d", n);
  std::string line;
  for (int i = 0; i < n; ++i) {
    std::ifstream f("/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/cpuinfo_max_freq");
    long khz = 0;
    if (f >> khz) line += std::to_string(khz / 1000) + " ";
    else line += "? ";
  }
  P("max MHz per core: %s", line.c_str());
  std::ifstream ci("/proc/cpuinfo");
  std::set<std::string> parts;
  std::string hw;
  for (std::string l; std::getline(ci, l);) {
    if (l.rfind("CPU part", 0) == 0) parts.insert(l.substr(l.find(':') + 2));
    if (l.rfind("Hardware", 0) == 0) hw = l.substr(l.find(':') + 2);
  }
  std::string ps;
  for (auto &p : parts) ps += p + " ";
  P("cpu parts: %s", ps.empty() ? "?" : ps.c_str());
  if (!hw.empty()) P("hardware: %s", hw.c_str());
#if defined(__aarch64__)
  unsigned long hwcap = getauxval(AT_HWCAP);
  P("atomics(LSE) %s, fp16 %s, dotprod %s, crc32 %s", YN(hwcap & (1 << 8)), YN(hwcap & (1 << 9)),
    YN(hwcap & (1 << 20)), YN(hwcap & (1 << 7)));
#endif
  if (n < 6) g_warn.push_back("fewer than 6 CPU cores: the PC port freezes more often below 6 threads");
}

// ---- Guest memory ------------------------------------------------------------------------------

// The runtime creates one shared mapping of 0x120000000 bytes and maps 4 GB views of it at the
// first free 1<<n address (n = 32..63), plus the physical views right after. Try the same.
void CheckMemory() {
  P("== Memory");
  long page = sysconf(_SC_PAGESIZE);
  P("page size: %ld bytes", page);
  if (page != 4096)
    g_warn.push_back("16 KB memory pages: the Xbox 360 uses 4 KB pages, extra work needed");

  const size_t kSize = 0x120000000ull;
  int fd = ASharedMemory_create("wwe12diag", kSize);
  if (fd < 0) {
    P("guest arena: ASharedMemory_create(4.5 GB) FAILED");
    g_fail.push_back("cannot create the 4.5 GB shared memory the game needs");
    return;
  }
  void *got = nullptr;
  int bit = -1;
  for (int n = 32; n < 48 && !got; ++n) {
    void *want = reinterpret_cast<void *>(1ull << n);
    void *p = mmap(want, 0x100000000ull, PROT_NONE, MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0);
    if (p == MAP_FAILED) continue;
    if (p != want) {
      munmap(p, 0x100000000ull);
      continue;
    }
    // Second view (physical memory), right after the first.
    void *want2 = reinterpret_cast<uint8_t *>(want) + 0x100000000ull;
    void *p2 = mmap(want2, 0x20000000ull, PROT_NONE, MAP_SHARED | MAP_FIXED_NOREPLACE, fd,
                    0x100000000ull);
    if (p2 == want2) {
      got = p;
      bit = n;
      munmap(p2, 0x20000000ull);
    } else if (p2 != MAP_FAILED) {
      munmap(p2, 0x20000000ull);
    }
    munmap(p, 0x100000000ull);
  }
  close(fd);
  if (got) {
    P("guest arena: OK at 1<<%d", bit);
  } else {
    P("guest arena: FAILED (no free 4 GB block)");
    g_fail.push_back("no free 4 GB address block for the game memory");
  }
}

// ---- Vulkan -------------------------------------------------------------------------------------

bool HasExt(const std::vector<VkExtensionProperties> &exts, const char *name) {
  for (auto &e : exts)
    if (!strcmp(e.extensionName, name)) return true;
  return false;
}

void CheckDevice(VkInstance inst, VkPhysicalDevice pd, uint32_t instVersion) {
  auto gipa = vkGetInstanceProcAddr;
  auto getProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
      gipa(inst, "vkGetPhysicalDeviceProperties2"));
  auto getFeat2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
      gipa(inst, "vkGetPhysicalDeviceFeatures2"));

  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(pd, &props);
  uint32_t api = props.apiVersion < instVersion ? props.apiVersion : instVersion;

  uint32_t n = 0;
  vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
  std::vector<VkExtensionProperties> exts(n);
  vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, exts.data());

  P("== GPU: %s", props.deviceName);
  P("vulkan %s, vendor 0x%04X, device 0x%04X, driver 0x%08X", Ver(props.apiVersion).c_str(),
    props.vendorID, props.deviceID, props.driverVersion);

  // Driver name (1.2 core).
  VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
  VkPhysicalDeviceFloatControlsProperties fc{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
  bool have12 = api >= VK_API_VERSION_1_2 && getProps2 && getFeat2;
  if (have12) {
    drv.pNext = &fc;
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv};
    getProps2(pd, &p2);
    P("driver: %s %s", drv.driverName, drv.driverInfo);
  }

  // Features.
  VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  VkPhysicalDeviceRobustness2FeaturesEXT rob{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
  bool hasRob = HasExt(exts, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
  VkPhysicalDeviceFeatures2 feat2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  if (have12) {
    feat2.pNext = &f12;
    if (hasRob) f12.pNext = &rob;
    getFeat2(pd, &feat2);
  } else {
    vkGetPhysicalDeviceFeatures(pd, &feat2.features);
  }
  const VkPhysicalDeviceFeatures &f = feat2.features;

  bool bda = have12 && f12.bufferDeviceAddress;
  bool dix = have12 && f12.descriptorBindingPartiallyBound &&
             f12.descriptorBindingVariableDescriptorCount && f12.runtimeDescriptorArray;
  bool infnan = have12 && fc.shaderSignedZeroInfNanPreserveFloat32;

  P("-- required");
  P("vulkan 1.2+: %s", YN(api >= VK_API_VERSION_1_2));
  P("swapchain: %s", YN(HasExt(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)));
  P("bufferDeviceAddress: %s", YN(bda));
  P("descriptorIndexing: %s", YN(dix));
  P("-- important");
  P("BC (DXT) textures: %s", YN(f.textureCompressionBC));
  P("Inf/NaN preserve (fp32): %s", YN(infnan));
  P("scalarBlockLayout: %s", YN(have12 && f12.scalarBlockLayout));
  P("robustness2 nullDescriptor: %s", YN(hasRob && rob.nullDescriptor));
  P("samplerMirrorClampToEdge: %s", YN(have12 && f12.samplerMirrorClampToEdge));
  P("-- other features");
  P("geometryShader %s, tessellation %s, depthClamp %s, independentBlend %s",
    YN(f.geometryShader), YN(f.tessellationShader), YN(f.depthClamp), YN(f.independentBlend));
  P("dualSrcBlend %s, logicOp %s, fillModeNonSolid %s, anisotropy %s", YN(f.dualSrcBlend),
    YN(f.logicOp), YN(f.fillModeNonSolid), YN(f.samplerAnisotropy));
  P("shaderInt16 %s, shaderInt64 %s, shaderFloat64 %s, multiDrawIndirect %s",
    YN(f.shaderInt16), YN(f.shaderInt64), YN(f.shaderFloat64), YN(f.multiDrawIndirect));
  P("ETC2 %s, ASTC %s, timelineSemaphore %s", YN(f.textureCompressionETC2),
    YN(f.textureCompressionASTC_LDR), YN(have12 && f12.timelineSemaphore));
  P("present_wait %s, load_store_op_none %s, display_timing %s, custom_border_color %s",
    YN(HasExt(exts, VK_KHR_PRESENT_WAIT_EXTENSION_NAME)),
    YN(HasExt(exts, VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME)),
    YN(HasExt(exts, VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME)),
    YN(HasExt(exts, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME)));

  // Formats.
  auto fmt = [&](VkFormat vf) {
    VkFormatProperties fp;
    vkGetPhysicalDeviceFormatProperties(pd, vf, &fp);
    return fp.optimalTilingFeatures;
  };
  auto sampled = [&](VkFormat vf) { return (fmt(vf) & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0; };
  auto depth = [&](VkFormat vf) {
    return (fmt(vf) & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
  };
  auto rtBlend = [&](VkFormat vf) {
    return (fmt(vf) & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0;
  };
  bool d24 = depth(VK_FORMAT_D24_UNORM_S8_UINT), d32s8 = depth(VK_FORMAT_D32_SFLOAT_S8_UINT);
  P("-- formats");
  P("BC1 %s, BC2 %s, BC3 %s, BC4 %s, BC5 %s", YN(sampled(VK_FORMAT_BC1_RGBA_UNORM_BLOCK)),
    YN(sampled(VK_FORMAT_BC2_UNORM_BLOCK)), YN(sampled(VK_FORMAT_BC3_UNORM_BLOCK)),
    YN(sampled(VK_FORMAT_BC4_UNORM_BLOCK)), YN(sampled(VK_FORMAT_BC5_UNORM_BLOCK)));
  P("depth D24S8 %s, D32S8 %s, D32 %s", YN(d24), YN(d32s8), YN(depth(VK_FORMAT_D32_SFLOAT)));
  P("render+blend RGBA16F %s, R11G11B10 %s, RGB10A2 %s, RGBA8 %s",
    YN(rtBlend(VK_FORMAT_R16G16B16A16_SFLOAT)), YN(rtBlend(VK_FORMAT_B10G11R11_UFLOAT_PACK32)),
    YN(rtBlend(VK_FORMAT_A2B10G10R10_UNORM_PACK32)), YN(rtBlend(VK_FORMAT_R8G8B8A8_UNORM)));

  // Limits and memory.
  const VkPhysicalDeviceLimits &l = props.limits;
  P("-- limits");
  P("maxImage2D %u, maxColorAttachments %u, maxPushConstants %u, maxBoundSets %u",
    l.maxImageDimension2D, l.maxColorAttachments, l.maxPushConstantsSize, l.maxBoundDescriptorSets);
  P("maxPerStageSampledImages %u, maxStorageBufferRange %u MB", l.maxPerStageDescriptorSampledImages,
    l.maxStorageBufferRange >> 20);
  VkPhysicalDeviceMemoryProperties mp;
  vkGetPhysicalDeviceMemoryProperties(pd, &mp);
  for (uint32_t i = 0; i < mp.memoryHeapCount; ++i)
    P("heap %u: %llu MB%s", i, static_cast<unsigned long long>(mp.memoryHeaps[i].size >> 20),
      (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " (device local)" : "");

  // Verdict pieces.
  if (api < VK_API_VERSION_1_2) g_fail.push_back("Vulkan 1.2 is needed (this GPU driver has " +
                                                 Ver(props.apiVersion) + ")");
  if (!HasExt(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) g_fail.push_back("no Vulkan swapchain");
  if (!bda) g_fail.push_back("Vulkan bufferDeviceAddress missing");
  if (!dix) g_fail.push_back("Vulkan descriptorIndexing missing");
  if (!d24 && !d32s8) g_fail.push_back("no depth-stencil format (D24S8 / D32S8)");
  if (!f.textureCompressionBC)
    g_warn.push_back("no BC (DXT) textures: the game's textures must be converted (slower, not done yet)");
  if (have12 && !infnan)
    g_warn.push_back("no Inf/NaN preserve: risk of black eyes/teeth like on old Intel PCs");
}

void CheckVulkan() {
  auto enumVer = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
      vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  uint32_t instVersion = VK_API_VERSION_1_0;
  if (enumVer) enumVer(&instVersion);
  P("== Vulkan");
  P("loader: %s", Ver(instVersion).c_str());

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "WWE12 device check";
  app.apiVersion = instVersion >= VK_API_VERSION_1_3   ? VK_API_VERSION_1_3
                   : instVersion >= VK_API_VERSION_1_2 ? VK_API_VERSION_1_2
                                                       : instVersion;
  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ci.pApplicationInfo = &app;
  VkInstance inst = VK_NULL_HANDLE;
  VkResult r = vkCreateInstance(&ci, nullptr, &inst);
  if (r != VK_SUCCESS) {
    P("vkCreateInstance failed: %d", r);
    g_fail.push_back("Vulkan does not start on this device");
    return;
  }
  uint32_t n = 0;
  vkEnumeratePhysicalDevices(inst, &n, nullptr);
  std::vector<VkPhysicalDevice> pds(n);
  vkEnumeratePhysicalDevices(inst, &n, pds.data());
  if (!n) g_fail.push_back("no Vulkan GPU found");
  for (auto pd : pds) CheckDevice(inst, pd, app.apiVersion);
  vkDestroyInstance(inst, nullptr);
}

void RunBench() {
  P("== CPU benchmark");
  int threads = static_cast<int>(std::thread::hardware_concurrency());
  if (threads < 1) threads = 1;
  diag::BenchResult b = diag::RunBench(threads);
  // Reference: same code built for Windows (bench_pc.cpp) on the developer's PC.
  P("single-thread: %.0f   all %d threads: %.0f", b.single, b.threads, b.multi);
  P("(reference: developer PC i7-14700HX = single 665, all 28 threads ~6400; 60 fps there)");
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_imrburn_wwe12diag_MainActivity_runNative(JNIEnv *env, jclass, jboolean bench) {
  g_out.clear();
  g_fail.clear();
  g_warn.clear();
#if defined(__aarch64__)
  P("native code: arm64-v8a");
#elif defined(__x86_64__)
  P("native code: x86_64 (emulator, not a real phone)");
#endif
  CheckCpu();
  CheckMemory();
  CheckVulkan();
  if (bench) RunBench();

  std::string verdict = "== VERDICT\n";
  if (g_fail.empty() && g_warn.empty()) verdict += "GOOD: nothing blocks the port on this device.\n";
  else if (g_fail.empty()) verdict += "POSSIBLE, with extra work:\n";
  else verdict += "BLOCKED today:\n";
  for (auto &s : g_fail) verdict += "  [X] " + s + "\n";
  for (auto &s : g_warn) verdict += "  [!] " + s + "\n";
  std::string all = verdict + "\n" + g_out;
  return env->NewStringUTF(all.c_str());
}
