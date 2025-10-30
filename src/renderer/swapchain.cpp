#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "swapchain.h"
#include "vulkan_context.h"
#include "../core/window.h"

#include <stdexcept>
#include <iostream>
#include <limits>
#include <algorithm>

static VkImageView createImageView(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &ci, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("[Swapchain] Failed to create image view.");
    }
    return view;
}

Swapchain::Swapchain(VulkanContext& context, Window& window)
    : vulkanContext(context), windowRef(window) {

    surface = vulkanContext.getSurface();
    if (surface == VK_NULL_HANDLE) {
        throw std::runtime_error("[Swapchain] VulkanContext has no surface; call initializeForSurface first.");
    }

    createSwapchain();
    createImageViews();

    std::cout << "[Swapchain] Ready with " << swapchainImages.size()
              << " images, format " << static_cast<int>(swapchainImageFormat) << ".\n";
}

Swapchain::~Swapchain() { cleanup(); }

void Swapchain::cleanup() {
    VkDevice device = vulkanContext.getDevice();
    for (auto view : swapchainImageViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(windowRef.getHandle(), &w, &h);
        VkExtent2D actual{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
        actual.width  = std::clamp(actual.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        actual.height = std::clamp(actual.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actual;
    }
}

void Swapchain::createSwapchain() {
    VkPhysicalDevice pd = vulkanContext.getPhysicalDevice();
    VkDevice         device = vulkanContext.getDevice();

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &formatCount, nullptr);
    if (formatCount == 0) throw std::runtime_error("[Swapchain] No surface formats.");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) throw std::runtime_error("[Swapchain] No present modes.");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    VkExtent2D extent = chooseSwapExtent(capabilities);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = surfaceFormat.format;
    ci.imageColorSpace = surfaceFormat.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    // No STORAGE bit here; we blit from our offscreen storage image
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t graphicsFamily = vulkanContext.getGraphicsQueueFamily();
    uint32_t presentFamily  = vulkanContext.getPresentQueueFamily();
    uint32_t computeFamily  = vulkanContext.getComputeQueueFamily();

    std::vector<uint32_t> families{graphicsFamily};
    if (std::find(families.begin(), families.end(), presentFamily) == families.end())
        families.push_back(presentFamily);
    if (std::find(families.begin(), families.end(), computeFamily) == families.end())
        families.push_back(computeFamily);

    if (families.size() > 1) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = static_cast<uint32_t>(families.size());
        ci.pQueueFamilyIndices = families.data();
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform = capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("[Swapchain] Failed to create swapchain.");
    }

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
    swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void Swapchain::createImageViews() {
    VkDevice device = vulkanContext.getDevice();
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        swapchainImageViews[i] = createImageView(device, swapchainImages[i], swapchainImageFormat);
    }
}

void Swapchain::recreate() {
    int w = 0, h = 0;
    do {
        glfwGetFramebufferSize(windowRef.getHandle(), &w, &h);
        glfwWaitEvents();
    } while (w == 0 || h == 0);

    vkDeviceWaitIdle(vulkanContext.getDevice());
    cleanup();
    createSwapchain();
    createImageViews();

    std::cout << "[Swapchain] Recreated at " << swapchainExtent.width << "x" << swapchainExtent.height << ".\n";
}

// -------- THE TWO FUNCTIONS THE LINKER COMPLAINED ABOUT --------

uint32_t Swapchain::acquireNextImage(VkSemaphore semaphore) {
    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(
        vulkanContext.getDevice(),
        swapchain,
        std::numeric_limits<uint64_t>::max(),
        semaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate();
        res = vkAcquireNextImageKHR(
            vulkanContext.getDevice(),
            swapchain,
            std::numeric_limits<uint64_t>::max(),
            semaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );
    }

    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("[Swapchain] Failed to acquire swapchain image.");
    }
    return imageIndex;
}

void Swapchain::present(uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = (waitSemaphore != VK_NULL_HANDLE) ? 1u : 0u;
    pi.pWaitSemaphores = (waitSemaphore != VK_NULL_HANDLE) ? &waitSemaphore : nullptr;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(vulkanContext.getPresentQueue(), &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        recreate();
        return;
    }
    if (res != VK_SUCCESS) {
        throw std::runtime_error("[Swapchain] Failed to present swapchain image.");
    }
}
