#include <iostream>
#include <stdexcept>

#include <vulkan/vulkan.h>
#include "renderer/vulkan_context.h"

int main() {
    std::cout << "Gargantua - Black Hole Raytracer\n";
    std::cout << "=================================\n\n";

    try {
        // Enable validation only in debug builds (constructor will honor _DEBUG)
        VulkanContext ctx{/*enableValidation=*/true};

        // Print selected GPU info
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(ctx.getPhysicalDevice(), &props);

        std::cout << "\n[Info] Selected GPU: " << props.deviceName << "\n";
        std::cout << "       API Version:  "
                  << VK_API_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_API_VERSION_MINOR(props.apiVersion) << "."
                  << VK_API_VERSION_PATCH(props.apiVersion) << "\n";
        std::cout << "       Vendor ID:    0x" << std::hex << props.vendorID
                  << "  Device ID: 0x" << props.deviceID << std::dec << "\n";

        std::cout << "[Info] Queue Families:\n";
        std::cout << "       Compute  Family Index: " << ctx.getComputeQueueFamily()  << "\n";
        std::cout << "       Graphics Family Index: " << ctx.getGraphicsQueueFamily() << "\n";

        std::cout << "\nSetup successful! VulkanContext is ready for compute workloads.\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n[Error] " << e.what() << "\n";
        return -1;
    }
}
