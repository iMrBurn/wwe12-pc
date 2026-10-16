// Is there a Vulkan graphics driver? The game's native renderer needs one (plume, Vulkan only), and
// without it the player only sees a black window. The check loads vulkan-1.dll at run time (the
// launcher itself must start without it), creates an instance and lists the graphics cards, in a
// background thread because some drivers take a moment to answer.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VulkanCheck {
 public:
  enum class State { Checking, Ok, Missing };

  void Start() {
    worker_ = std::thread([this] { Run(); });
  }
  ~VulkanCheck() {
    if (worker_.joinable())
      worker_.detach();
  }

  State GetState() const { return state_.load(); }
  // "NVIDIA GeForce GTX 1650 (Vulkan 1.3), Intel(R) UHD Graphics (Vulkan 1.3)": every real card.
  std::string Detail() {
    std::lock_guard lock(mutex_);
    return detail_;
  }
  // Indented lines for a bug report: every device with its driver, or why Vulkan is missing.
  std::string Report() {
    std::lock_guard lock(mutex_);
    return report_;
  }

 private:
  void Run() {
    std::string detail, report;
    const bool ok = Probe(detail, report);
    {
      std::lock_guard lock(mutex_);
      detail_ = detail;
      report_ = report;
    }
    state_ = ok ? State::Ok : State::Missing;
  }

  static std::string Version(uint32_t v) {
    return std::to_string(VK_VERSION_MAJOR(v)) + "." + std::to_string(VK_VERSION_MINOR(v));
  }

  static bool Probe(std::string &detail, std::string &report) {
    HMODULE lib = LoadLibraryW(L"vulkan-1.dll");  // kept loaded: unloading can upset drivers
    if (!lib) {
      report = "  vulkan-1.dll not found (no Vulkan loader: graphics driver too old or missing)\n";
      return false;
    }
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(lib, "vkGetInstanceProcAddr"));
    auto create = gipa ? reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr, "vkCreateInstance")) : nullptr;
    if (!create) {
      report = "  vulkan-1.dll found but unusable\n";
      return false;
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_0;  // only asks whether Vulkan answers at all
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    VkInstance instance = nullptr;
    const VkResult created = create(&info, nullptr, &instance);
    if (created != VK_SUCCESS || !instance) {
      report = "  vkCreateInstance failed (" + std::to_string(int(created)) + "): no Vulkan driver\n";
      return false;
    }
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(gipa(instance, "vkDestroyInstance"));
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        gipa(instance, "vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        gipa(instance, "vkGetPhysicalDeviceProperties"));
    bool found = false;
    uint32_t count = 0;
    if (enumerate && properties && enumerate(instance, &count, nullptr) == VK_SUCCESS && count) {
      std::vector<VkPhysicalDevice> devices(count);
      enumerate(instance, &count, devices.data());
      for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties p{};
        properties(devices[i], &p);
        const bool software = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
        const char *type = software ? "software" : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete"
                         : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated" : "other";
        char line[320];
        snprintf(line, sizeof(line), "  %s | Vulkan %s | %s | vendor 0x%04X | driver 0x%08X\n", p.deviceName,
                 Version(p.apiVersion).c_str(), type, p.vendorID, p.driverVersion);
        report += line;
        if (software)  // software rendering: far too slow to count
          continue;
        if (found)
          detail += ", ";
        detail += std::string(p.deviceName) + " (Vulkan " + Version(p.apiVersion) + ")";
        found = true;
      }
    }
    if (report.empty())
      report = "  Vulkan answers but lists no graphics card\n";
    else if (!found)
      report += "  (only software rendering: too slow)\n";
    if (destroy)
      destroy(instance, nullptr);
    return found;
  }

  std::atomic<State> state_{State::Checking};
  std::mutex mutex_;
  std::string detail_, report_;
  std::thread worker_;
};
