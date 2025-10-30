#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

class VulkanContext {
public:
    explicit VulkanContext(bool enableValidation = true, VkSurfaceKHR surface = VK_NULL_HANDLE);
    ~VulkanContext() noexcept;

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // If you created the context without a surface, call this once when you have it.
    void initializeForSurface(VkSurfaceKHR surface);

    // Getters
    VkInstance        getInstance()            const { return instance; }
    VkPhysicalDevice  getPhysicalDevice()      const { return physicalDevice; }
    VkDevice          getDevice()              const { return device; }
    VkQueue           getComputeQueue()        const { return computeQueue; }
    VkQueue           getGraphicsQueue()       const { return graphicsQueue; }
    VkQueue           getPresentQueue()        const { return presentQueue; }
    VkCommandPool     getComputeCommandPool()  const { return computeCmdPool; }
    VkCommandPool     getGraphicsCommandPool() const { return graphicsCmdPool; }
    uint32_t          getComputeQueueFamily()  const { return computeQueueFamily; }
    uint32_t          getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    uint32_t          getPresentQueueFamily()  const { return presentQueueFamily; }
    VkSurfaceKHR      getSurface()             const { return surface; }

    // ---- Legacy shim (keeps old code building) ----
    // Old code used context.getCommandPool() for compute work.
    // Keep this until you migrate call sites to getComputeCommandPool().
    VkCommandPool     getCommandPool()         const { return computeCmdPool; }

private:
    // Init stages
    void createInstance();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createCommandPools();

    // Helpers
    static bool      checkValidationLayerSupport();
    static bool      deviceHasCompute(const VkPhysicalDevice& pd);
    static void      printPhysicalDeviceInfo(const VkPhysicalDevice& pd);
    static uint32_t  findComputeQueueFamily(const VkPhysicalDevice& pd);
    static uint32_t  findGraphicsQueueFamily(const VkPhysicalDevice& pd);
    static bool      queueFamilySupportsPresent(VkPhysicalDevice pd, uint32_t family, VkSurfaceKHR surface);

    // Teardown
    void shutdown() noexcept;

    // Vulkan handles
    VkInstance        instance              = VK_NULL_HANDLE;
    VkPhysicalDevice  physicalDevice        = VK_NULL_HANDLE;
    VkDevice          device                = VK_NULL_HANDLE;

    VkQueue           computeQueue          = VK_NULL_HANDLE;
    VkQueue           graphicsQueue         = VK_NULL_HANDLE;
    VkQueue           presentQueue          = VK_NULL_HANDLE;

    VkCommandPool     computeCmdPool        = VK_NULL_HANDLE;
    VkCommandPool     graphicsCmdPool       = VK_NULL_HANDLE;

    VkSurfaceKHR      surface               = VK_NULL_HANDLE;

    // Families
    uint32_t          computeQueueFamily    = UINT32_MAX;
    uint32_t          graphicsQueueFamily   = UINT32_MAX;
    uint32_t          presentQueueFamily    = UINT32_MAX;

    // State
    bool              validationEnabled     = false;
    bool              initializedForSurface = false;
};
