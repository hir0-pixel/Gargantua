#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

int main() {
    std::cout << "Gargantua - Black Hole Raytracer\n";
    std::cout << "=================================\n\n";
    
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::cout << "Vulkan extensions available: " << extensionCount << "\n";
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    std::cout << "GLFW initialized successfully\n";
    
    if (!glfwVulkanSupported()) {
        std::cerr << "Vulkan not supported\n";
        glfwTerminate();
        return -1;
    }
    
    std::cout << "Vulkan supported!\n\n";
    std::cout << "Setup successful! Ready to build Gargantua.\n";
    
    glfwTerminate();
    return 0;
}