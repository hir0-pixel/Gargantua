#include "vulkan_context.h"

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <algorithm>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// If your windowing code destroys the surface, set this to 0.
#ifndef DESTROY_SURFACE_IN_CONTEXT
#define DESTROY_SURFACE_IN_CONTEXT 1
#endif

namespace {
    constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

    const char* deviceTypeName(VkPhysicalDeviceType t) {
        switch (t) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
            default:                                     return "Other";
        }
    }

    static int devicePreferenceScore(const VkPhysicalDevice& pd) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);

        int typeScore = 0;
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeScore = 1000; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeScore = 500;  break;
            default:                                     typeScore = 100;  break;
        }

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());
        bool hasCompute = std::any_of(qProps.begin(), qProps.end(),
                                      [](const VkQueueFamilyProperties& q) {
                                          return (q.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                                      });
        if (!hasCompute) return 0;

        int versionScore =
            static_cast<int>(VK_API_VERSION_MAJOR(props.apiVersion) * 100
                           + VK_API_VERSION_MINOR(props.apiVersion) * 10);
        return typeScore + versionScore;
    }
}

// ---------- Ctor / Dtor ----------

VulkanContext::VulkanContext(bool enableValidation, VkSurfaceKHR surf) {
#ifdef _DEBUG
    validationEnabled = enableValidation;
#else
    validationEnabled = false;
#endif

    createInstance();

    if (surf != VK_NULL_HANDLE) {
        initializeForSurface(surf);
    } else {
        std::cout << "[Vulkan] Instance ready. Waiting for surface to finish device init...\n";
    }
}

VulkanContext::~VulkanContext() noexcept {
    shutdown();
}

// ---------- Public API ----------

void VulkanContext::initializeForSurface(VkSurfaceKHR surf) {
    if (initializedForSurface) return;
    if (surf == VK_NULL_HANDLE)
        throw std::runtime_error("[Vulkan] initializeForSurface called with null surface.");

    surface = surf;
    selectPhysicalDevice();
    createLogicalDevice();
    createCommandPools();

    initializedForSurface = true;
}

// ---------- Teardown ----------

void VulkanContext::shutdown() noexcept {
    // Device-scoped first
    if (device != VK_NULL_HANDLE) {
        // vkDeviceWaitIdle(device); // optional, enable if needed.

        if (graphicsCmdPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, graphicsCmdPool, nullptr);
            graphicsCmdPool = VK_NULL_HANDLE;
        }
        if (computeCmdPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, computeCmdPool, nullptr);
            computeCmdPool = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

#if DESTROY_SURFACE_IN_CONTEXT
    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
#endif

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    physicalDevice        = VK_NULL_HANDLE;
    computeQueue          = VK_NULL_HANDLE;
    graphicsQueue         = VK_NULL_HANDLE;
    presentQueue          = VK_NULL_HANDLE;
    computeQueueFamily    = UINT32_MAX;
    graphicsQueueFamily   = UINT32_MAX;
    presentQueueFamily    = UINT32_MAX;
    initializedForSurface = false;
}

// ---------- Instance / Device setup ----------

bool VulkanContext::checkValidationLayerSupport() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& l : layers) {
        if (std::strcmp(l.layerName, kValidationLayer) == 0) return true;
    }
    return false;
}

void VulkanContext::createInstance() {
    std::cout << "[Vulkan] Creating instance"
              << (validationEnabled ? " (validation enabled)" : "") << "...\n";

    if (validationEnabled && !checkValidationLayerSupport()) {
        std::cerr << "[Vulkan] Warning: Validation layer not available. Disabling.\n";
        validationEnabled = false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Gargantua";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "GargantuaCore";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Ask GLFW for required instance extensions (surface + platform)
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    if (!glfwExts || glfwExtCount == 0) {
        throw std::runtime_error("[Vulkan] GLFW did not provide required instance extensions.");
    }

    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);
#ifdef _DEBUG
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    std::vector<const char*> layers;
    if (validationEnabled) layers.push_back(kValidationLayer);

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("[Vulkan] Failed to create instance (vkCreateInstance).");
    }
    std::cout << "[Vulkan] Instance created.\n";
}

void VulkanContext::selectPhysicalDevice() {
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("[Vulkan] selectPhysicalDevice called without surface.");
    }

    std::cout << "[Vulkan] Selecting physical device (with surface support)...\n";

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("[Vulkan] No Vulkan-capable GPUs found.");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    std::cout << "[Vulkan] Available devices:\n";
    for (const auto& pd : devices) {
        printPhysicalDeviceInfo(pd);
    }

    std::optional<VkPhysicalDevice> best;
    int bestScore = -1;

    for (const auto& pd : devices) {
        if (!deviceHasCompute(pd)) continue;

        // Must also support presenting to our surface from *some* queue
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

        bool anyPresent = false;
        for (uint32_t i = 0; i < qCount; ++i) {
            if (queueFamilySupportsPresent(pd, i, surface)) { anyPresent = true; break; }
        }
        if (!anyPresent) continue;

        int score = devicePreferenceScore(pd);
        if (score > bestScore) { bestScore = score; best = pd; }
    }

    if (!best.has_value()) {
        throw std::runtime_error("[Vulkan] No suitable GPU with compute + present support.");
    }

    physicalDevice = *best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "[Vulkan] Selected GPU: " << props.deviceName
              << " (" << deviceTypeName(props.deviceType) << ")\n";
}

bool VulkanContext::deviceHasCompute(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());
    for (const auto& q : qProps) if (q.queueFlags & VK_QUEUE_COMPUTE_BIT) return true;
    return false;
}

void VulkanContext::printPhysicalDeviceInfo(const VkPhysicalDevice& pd) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pd, &props);

    std::cout << "  - GPU: " << props.deviceName
              << "  | Type: " << deviceTypeName(props.deviceType)
              << "  | API: " << VK_API_VERSION_MAJOR(props.apiVersion)
              << "." << VK_API_VERSION_MINOR(props.apiVersion)
              << "." << VK_API_VERSION_PATCH(props.apiVersion) << "\n";
}

bool VulkanContext::queueFamilySupportsPresent(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surf) {
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(pd, family, surf, &supported);
    return supported == VK_TRUE;
}

uint32_t VulkanContext::findComputeQueueFamily(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

    for (uint32_t i = 0; i < qCount; ++i) {
        bool compute = (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        bool graphics = (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (compute && !graphics) return i; // prefer dedicated
    }
    for (uint32_t i = 0; i < qCount; ++i)
        if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;

    throw std::runtime_error("[Vulkan] No compute queue family found.");
}

uint32_t VulkanContext::findGraphicsQueueFamily(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

    for (uint32_t i = 0; i < qCount; ++i)
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;

    throw std::runtime_error("[Vulkan] No graphics queue family found.");
}

void VulkanContext::createLogicalDevice() {
    if (physicalDevice == VK_NULL_HANDLE)  throw std::runtime_error("[Vulkan] createLogicalDevice called before selecting a GPU.");
    if (surface == VK_NULL_HANDLE)         throw std::runtime_error("[Vulkan] createLogicalDevice requires a valid surface.");

    std::cout << "[Vulkan] Creating logical device...\n";

    computeQueueFamily  = findComputeQueueFamily(physicalDevice);
    graphicsQueueFamily = findGraphicsQueueFamily(physicalDevice);

    // Prefer graphics family for present; fallback to any present-capable family
    if (queueFamilySupportsPresent(physicalDevice, graphicsQueueFamily, surface)) {
        presentQueueFamily = graphicsQueueFamily;
    } else {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qCount, nullptr);
        for (uint32_t i = 0; i < qCount; ++i)
            if (queueFamilySupportsPresent(physicalDevice, i, surface)) { presentQueueFamily = i; break; }
        if (presentQueueFamily == UINT_MAX)
            throw std::runtime_error("[Vulkan] No queue family supports present for this surface.");
    }

    // Unique families
    std::vector<uint32_t> uniqueFamilies;
    uniqueFamilies.push_back(computeQueueFamily);
    if (graphicsQueueFamily != computeQueueFamily)
        uniqueFamilies.push_back(graphicsQueueFamily);
    if (presentQueueFamily != graphicsQueueFamily && presentQueueFamily != computeQueueFamily)
        uniqueFamilies.push_back(presentQueueFamily);

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(uniqueFamilies.size());
    for (uint32_t fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    // --- Enable Vulkan 1.3 features (Synchronization2) ---
    VkPhysicalDeviceVulkan13Features v13{};
    v13.synchronization2 = VK_TRUE;     // REQUIRED for vkCmdPipelineBarrier2 & vkQueueSubmit2
    v13.dynamicRendering = VK_TRUE;     // optional if you use it
    v13.maintenance4     = VK_TRUE;     // optional

    v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &v13;         // chain 1.3 features

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
        // , VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME  // optional, if you adopt present fences
    };

    std::vector<const char*> layers;
    if (validationEnabled) layers.push_back(kValidationLayer);

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    dci.pQueueCreateInfos    = queueInfos.data();
    dci.enabledExtensionCount = static_cast<uint32_t>(std::size(deviceExtensions));
    dci.ppEnabledExtensionNames = deviceExtensions;
    dci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    dci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    dci.pNext = &features2;     // IMPORTANT: pass features2 via pNext
    dci.pEnabledFeatures = nullptr; // when using features2, leave this null

    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("[Vulkan] Failed to create logical device.");
    }

    vkGetDeviceQueue(device, computeQueueFamily,  0, &computeQueue);
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily,  0, &presentQueue);

    std::cout << "[Vulkan] Logical device created.\n";
    std::cout << "  Compute  queue family: " << computeQueueFamily  << "\n";
    std::cout << "  Graphics queue family: " << graphicsQueueFamily << "\n";
    std::cout << "  Present  queue family: " << presentQueueFamily  << "\n";
}

void VulkanContext::createCommandPools() {
    // Compute
    {
        VkCommandPoolCreateInfo ci{ };
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = computeQueueFamily;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &ci, nullptr, &computeCmdPool) != VK_SUCCESS) {
            throw std::runtime_error("[Vulkan] Failed to create compute command pool.");
        }
    }
    // Graphics (needed for vkCmdBlitImage, render passes, etc.)
    {
        VkCommandPoolCreateInfo ci{ };
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = graphicsQueueFamily;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &ci, nullptr, &graphicsCmdPool) != VK_SUCCESS) {
            throw std::runtime_error("[Vulkan] Failed to create graphics command pool.");
        }
    }

    std::cout << "[Vulkan] Command pools created (compute + graphics).\n";
}
