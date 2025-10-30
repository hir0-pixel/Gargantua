#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class VulkanContext {
public:
    explicit VulkanContext(bool enableValidation = true);
    ~VulkanContext();

    // Deleted copy/move (RAII, single owner of handles)
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Getters
    VkInstance      getInstance()            const { return instance; }
    VkPhysicalDevice getPhysicalDevice()     const { return physicalDevice; }
    VkDevice        getDevice()              const { return device; }
    VkQueue         getComputeQueue()        const { return computeQueue; }
    VkQueue         getGraphicsQueue()       const { return graphicsQueue; }
    VkCommandPool   getCommandPool()         const { return commandPool; }
    uint32_t        getComputeQueueFamily()  const { return computeQueueFamily; }
    uint32_t        getGraphicsQueueFamily() const { return graphicsQueueFamily; }

private:
    void createInstance();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    // Helpers
    static bool checkValidationLayerSupport();
    static bool deviceHasCompute(const VkPhysicalDevice& pd);
    static void printPhysicalDeviceInfo(const VkPhysicalDevice& pd);
    static uint32_t findComputeQueueFamily(const VkPhysicalDevice& pd);
    static uint32_t findGraphicsQueueFamily(const VkPhysicalDevice& pd);

    // Vulkan handles
    VkInstance       instance            = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice      = VK_NULL_HANDLE;
    VkDevice         device              = VK_NULL_HANDLE;
    VkQueue          computeQueue        = VK_NULL_HANDLE;
    VkQueue          graphicsQueue       = VK_NULL_HANDLE;
    VkCommandPool    commandPool         = VK_NULL_HANDLE;
    uint32_t         computeQueueFamily  = UINT32_MAX;
    uint32_t         graphicsQueueFamily = UINT32_MAX;
    bool             validationEnabled   = false;
};
