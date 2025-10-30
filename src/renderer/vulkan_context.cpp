#include "vulkan_context.h"

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <algorithm>

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
}

VulkanContext::VulkanContext(bool enableValidation) {
#ifdef _DEBUG
    validationEnabled = enableValidation;
#else
    validationEnabled = false; // force off in release
#endif

    createInstance();
    selectPhysicalDevice();
    createLogicalDevice();
    createCommandPool();

    std::cout << "[Vulkan] Initialization complete.\n";
}

VulkanContext::~VulkanContext() {
    if (device != VK_NULL_HANDLE) {
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

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

    // We’re not creating a surface here, so no required platform extensions.
    std::vector<const char*> enabledLayers;
    if (validationEnabled) {
        enabledLayers.push_back(kValidationLayer);
    }

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
    ci.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();
    ci.enabledExtensionCount = 0;
    ci.ppEnabledExtensionNames = nullptr;

    VkResult res = vkCreateInstance(&ci, nullptr, &instance);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Vulkan] Failed to create instance (vkCreateInstance).");
    }
    std::cout << "[Vulkan] Instance created.\n";
}

static int devicePreferenceScore(const VkPhysicalDevice& pd) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pd, &props);

    // Prefer discrete GPUs, then integrated, then others.
    int typeScore = 0;
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeScore = 1000; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeScore = 500;  break;
        default:                                     typeScore = 100;  break;
    }

    // Require compute capability; if missing, score is zero.
    // (Caller will filter this too, but we keep it defensive.)
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());
    bool hasCompute = std::any_of(qProps.begin(), qProps.end(),
                                  [](const VkQueueFamilyProperties& q) {
                                      return (q.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                                  });
    if (!hasCompute) return 0;

    // Prefer higher Vulkan API version and more limits (simple heuristic)
    int versionScore = static_cast<int>(VK_API_VERSION_MAJOR(props.apiVersion) * 100
                                      + VK_API_VERSION_MINOR(props.apiVersion) * 10);

    return typeScore + versionScore;
}

bool VulkanContext::deviceHasCompute(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());
    for (const auto& q : qProps) {
        if (q.queueFlags & VK_QUEUE_COMPUTE_BIT) return true;
    }
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

void VulkanContext::selectPhysicalDevice() {
    std::cout << "[Vulkan] Selecting physical device...\n";

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

    // Filter devices that have compute support; rank by preference.
    std::optional<VkPhysicalDevice> best;
    int bestScore = -1;
    for (const auto& pd : devices) {
        if (!deviceHasCompute(pd)) continue;
        int score = devicePreferenceScore(pd);
        if (score > bestScore) {
            bestScore = score;
            best = pd;
        }
    }

    if (!best.has_value()) {
        throw std::runtime_error("[Vulkan] No physical device with compute capability found.");
    }

    physicalDevice = *best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "[Vulkan] Selected GPU: " << props.deviceName
              << " (" << deviceTypeName(props.deviceType) << ")\n";
}

uint32_t VulkanContext::findComputeQueueFamily(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

    // Prefer a dedicated compute queue (compute but not graphics)
    for (uint32_t i = 0; i < qCount; ++i) {
        bool compute = (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        bool graphics = (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (compute && !graphics) return i;
    }
    // Fallback: any compute queue
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;
    }
    throw std::runtime_error("[Vulkan] No compute queue family found.");
}

uint32_t VulkanContext::findGraphicsQueueFamily(const VkPhysicalDevice& pd) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());

    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    }
    throw std::runtime_error("[Vulkan] No graphics queue family found.");
}

void VulkanContext::createLogicalDevice() {
    std::cout << "[Vulkan] Creating logical device...\n";

    computeQueueFamily  = findComputeQueueFamily(physicalDevice);
    graphicsQueueFamily = findGraphicsQueueFamily(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<uint32_t> uniqueFamilies;
    uniqueFamilies.push_back(computeQueueFamily);
    if (graphicsQueueFamily != computeQueueFamily) {
        uniqueFamilies.push_back(graphicsQueueFamily);
    }

    // Queue priority: 1.0f (highest)
    float priority = 1.0f;
    queueInfos.reserve(uniqueFamilies.size());
    for (uint32_t fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{}; // default; enable as needed later

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    dci.pQueueCreateInfos = queueInfos.data();
    dci.pEnabledFeatures = &features;

    // No device extensions required yet (compute-only bootstrap)
    dci.enabledExtensionCount = 0;
    dci.ppEnabledExtensionNames = nullptr;

    std::vector<const char*> enabledLayers;
    if (validationEnabled) {
        enabledLayers.push_back(kValidationLayer);
    }
    // Older Vulkan loaders still read device layer lists; modern ones ignore these.
    dci.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
    dci.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();

    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("[Vulkan] Failed to create logical device.");
    }

    vkGetDeviceQueue(device, computeQueueFamily,  0, &computeQueue);
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

    std::cout << "[Vulkan] Logical device created.\n";
    std::cout << "  Compute  queue family: " << computeQueueFamily  << "\n";
    std::cout << "  Graphics queue family: " << graphicsQueueFamily << "\n";
}

void VulkanContext::createCommandPool() {
    std::cout << "[Vulkan] Creating compute command pool...\n";

    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = computeQueueFamily;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &ci, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("[Vulkan] Failed to create command pool for compute queue.");
    }
    std::cout << "[Vulkan] Command pool created.\n";
}
//
// Created by MSI on 10/30/2025.
//