#include <iostream>
#include <stdexcept>
#include <vector>

#include "core/window.h"
#include "renderer/vulkan_context.h"
#include "renderer/swapchain.h"
#include "renderer/compute_pipeline.h"

#ifndef GARGANTUA_SHADER_DIR
#define GARGANTUA_SHADER_DIR "."
#endif

static VkSemaphore createSemaphore(VkDevice device) {
    VkSemaphoreCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore sem = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device, &ci, nullptr, &sem) != VK_SUCCESS) {
        throw std::runtime_error("[Main] Failed to create semaphore.");
    }
    return sem;
}

int main() {
    std::cout << "Gargantua - Black Hole Raytracer\n";
    std::cout << "=================================\n\n";

    try {
        Window window(1920, 1080, "Gargantua - Black Hole Raytracer");
        VulkanContext context(/*enableValidation=*/true);

        VkSurfaceKHR surface = window.createSurface(context.getInstance());
        context.initializeForSurface(surface);

        Swapchain swapchain(context, window);

        // Absolute path via compile definition
        std::string shaderPath = std::string(GARGANTUA_SHADER_DIR) + "/test.comp.spv";
        ComputePipeline compute(context, swapchain, shaderPath);

        VkDevice device = context.getDevice();
        VkSemaphore imageAvailable = createSemaphore(device);
        VkSemaphore renderFinished = createSemaphore(device);

        double fpsTimer = 0.0;
        uint32_t frames = 0;

        while (!window.shouldClose()) {
            window.pollEvents();
            float dt = window.getDeltaTime();
            fpsTimer += dt;
            ++frames;

            if (window.wasResized()) {
                vkDeviceWaitIdle(device);
                swapchain.recreate();
                compute.recreate();
                window.resetResizeFlag();
            }

            uint32_t imageIndex = swapchain.acquireNextImage(imageAvailable);
            compute.dispatch(imageIndex, imageAvailable, renderFinished);
            swapchain.present(imageIndex, renderFinished);

            if (fpsTimer >= 1.0) {
                std::cout << "[FPS] " << frames << "\n";
                fpsTimer -= 1.0;
                frames = 0;
            }
        }

        vkDeviceWaitIdle(device);
        vkDestroySemaphore(device, renderFinished, nullptr);
        vkDestroySemaphore(device, imageAvailable, nullptr);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[Error] " << e.what() << "\n";
        return -1;
    }
}
