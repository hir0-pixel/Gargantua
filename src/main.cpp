#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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

struct Camera {
    float x = 0.0f;
    float y = 0.0f;
    float zoom = 1.0f;
};

static Camera camera;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        float speed = 5000.0f;
        if (key == GLFW_KEY_W) camera.y += speed;
        if (key == GLFW_KEY_S) camera.y -= speed;
        if (key == GLFW_KEY_A) camera.x -= speed;
        if (key == GLFW_KEY_D) camera.x += speed;
        if (key == GLFW_KEY_Q) camera.zoom *= 1.1f;
        if (key == GLFW_KEY_E) camera.zoom *= 0.9f;
        if (key == GLFW_KEY_R) { camera.x = 0; camera.y = 0; camera.zoom = 1.0f; }
    }
}

int main() {
    std::cout << "Gargantua - Black Hole Raytracer\n";
    std::cout << "=================================\n";
    std::cout << "Controls: WASD=Pan, Q/E=Zoom, R=Reset\n\n";

    try {
        Window window(1920, 1080, "Gargantua - Black Hole Raytracer");
        VulkanContext context(true);

        VkSurfaceKHR surface = window.createSurface(context.getInstance());
        context.initializeForSurface(surface);

        Swapchain swapchain(context, window);

        std::string shaderPath = std::string(GARGANTUA_SHADER_DIR) + "/test.comp.spv";
        ComputePipeline compute(context, swapchain, shaderPath);

        VkDevice device = context.getDevice();
        const size_t MAX_FRAMES = 3;
        std::vector<VkSemaphore> imageAvailableSems(MAX_FRAMES);
        std::vector<VkSemaphore> renderFinishedSems(MAX_FRAMES);

        for (size_t i = 0; i < MAX_FRAMES; i++) {
            imageAvailableSems[i] = createSemaphore(device);
            renderFinishedSems[i] = createSemaphore(device);
        }

        uint32_t currentFrame = 0;

        glfwSetKeyCallback(window.getHandle(), keyCallback);

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

            uint32_t imageIndex = swapchain.acquireNextImage(imageAvailableSems[currentFrame]);

            CameraData camData{camera.x, camera.y, camera.zoom, static_cast<float>(glfwGetTime())};
            compute.dispatch(imageIndex, imageAvailableSems[currentFrame], renderFinishedSems[currentFrame], camData);
            swapchain.present(imageIndex, renderFinishedSems[currentFrame]);

            currentFrame = (currentFrame + 1) % MAX_FRAMES;

            if (fpsTimer >= 1.0) {
                std::cout << "[FPS] " << frames << " | Cam: ("
                         << camera.x << ", " << camera.y << ") Zoom: " << camera.zoom << "\n";
                fpsTimer -= 1.0;
                frames = 0;
            }
        }

        vkDeviceWaitIdle(device);
        for (size_t i = 0; i < MAX_FRAMES; i++) {
            vkDestroySemaphore(device, renderFinishedSems[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSems[i], nullptr);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[Error] " << e.what() << "\n";
        return -1;
    }
}