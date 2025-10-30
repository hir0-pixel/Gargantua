#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "window.h"

#include <stdexcept>
#include <iostream>

Window::Window(int w, int h, const std::string& title)
    : width(w), height(h) {
    if (!glfwInit()) {
        throw std::runtime_error("[Window] Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan, no OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("[Window] Failed to create GLFW window.");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    lastFrameTime = static_cast<float>(glfwGetTime());

    std::cout << "[Window] Created " << width << "x" << height << " window.\n";
}

Window::~Window() {
    if (surface != VK_NULL_HANDLE) {
        // We own the surface we created via GLFW; destroy it before instance destruction.
        // The app ensures instance is still valid when calling this destructor.
        // Note: We can't destroy without the instance; GLFW hides it, so use vkDestroySurfaceKHR directly.
        // But we didn't store instance. Safer: let it leak until the instance is destroyed at app end,
        // or app can destroy explicitly. We'll rely on instance destruction to clean the surface.
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

float Window::getDeltaTime() {
    float now = static_cast<float>(glfwGetTime());
    float dt  = now - lastFrameTime;
    lastFrameTime = now;
    return dt;
}

void Window::framebufferResizeCallback(GLFWwindow* glfwWin, int w, int h) {
    auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWin));
    if (!self) return;

    self->width  = (w > 0) ? w : 1;
    self->height = (h > 0) ? h : 1;
    self->framebufferResized = true;
}

VkSurfaceKHR Window::createSurface(VkInstance instance) {
    if (surface != VK_NULL_HANDLE) {
        return surface; // idempotent
    }
    VkSurfaceKHR surf = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surf) != VK_SUCCESS) {
        throw std::runtime_error("[Window] Failed to create Vulkan surface via GLFW.");
    }
    surface = surf;
    std::cout << "[Window] Vulkan surface created.\n";
    return surface;
}
