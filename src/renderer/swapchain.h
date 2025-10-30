#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;
class Window;

/**
 * Swapchain
 * =========
 * Responsible for creating and managing the Vulkan swapchain and its images/views.
 * Supports presentation, window resize handling, and compute shader writes via STORAGE usage.
 */
class Swapchain {
public:
    Swapchain(VulkanContext& context, Window& window);
    ~Swapchain();

    // Non-copyable
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // --- Core lifecycle ---
    void     recreate();   // Recreate on window resize
    uint32_t acquireNextImage(VkSemaphore semaphore);   // Acquire next image index
    void     present(uint32_t imageIndex, VkSemaphore waitSemaphore); // Present to screen

    // --- Accessors ---
    VkFormat   getImageFormat() const { return swapchainImageFormat; }
    VkExtent2D getExtent()     const { return swapchainExtent; }
    size_t     getImageCount() const { return swapchainImages.size(); }

    VkImageView getImageView(size_t index) const { return swapchainImageViews[index]; }
    VkImage     getImage(size_t index)     const { return swapchainImages[index]; }

private:
    // --- Internal creation helpers ---
    void createSwapchain();
    void createImageViews();
    void cleanup();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR   chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D         chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

private:
    VulkanContext&  vulkanContext;
    Window&         windowRef;
    VkSurfaceKHR    surface = VK_NULL_HANDLE;

    VkSwapchainKHR             swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>       swapchainImages;
    std::vector<VkImageView>   swapchainImageViews;
    VkFormat                   swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D                 swapchainExtent{0, 0};
};
