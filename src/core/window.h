#pragma once
#include <string>
#include <vulkan/vulkan.h>
struct GLFWwindow; // fwd decl

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool   shouldClose() const;
    void   pollEvents();
    float  getDeltaTime();

    VkSurfaceKHR createSurface(VkInstance instance);
    GLFWwindow*  getHandle() const { return window; }
    int          getWidth()  const { return width; }
    int          getHeight() const { return height; }
    bool         wasResized() const { return framebufferResized; }
    void         resetResizeFlag() { framebufferResized = false; }

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* window = nullptr;
    int         width;
    int         height;
    bool        framebufferResized = false;
    float       lastFrameTime = 0.0f;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
};
