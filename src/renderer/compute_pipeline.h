#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
struct CameraData {
    float x, y, zoom, time;  // Changed padding to time
};
class VulkanContext;
class Swapchain;

class ComputePipeline {
public:
    // shaderSpvPath should be an absolute path
    ComputePipeline(VulkanContext& context, Swapchain& swapchain, const std::string& shaderSpvPath);
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    // Rebuild descriptors and storage image when swapchain changes
    void recreate();

    // Records & submits:
    //   1) compute dispatch (compute queue)
    //   2) storage->swapchain blit (graphics queue)
    // Uses waitSemaphore (from acquire) and signalSemaphore (for present).
    void dispatch(uint32_t imageIndex, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, const CameraData& camera);

private:
    // Creation
    void createDescriptorSetLayout();
    void createPipelineLayout();
    void createPipelineFromCode(const std::vector<char>& code);
    void createDescriptorPoolAndSets();     // single STORAGE image descriptor
    void allocateCommandBuffers();          // compute + graphics

    // Offscreen storage image (compute target)
    void createStorageImage();
    void destroyStorageImage();
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    // Helpers
    static std::vector<char> readFile(const std::string& path);

private:
    VulkanContext& ctx;
    Swapchain&     sc;
    VkDevice       device = VK_NULL_HANDLE;

    // Pipeline objects
    VkDescriptorSetLayout        descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout             pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline                   pipeline            = VK_NULL_HANDLE;
    VkDescriptorPool             descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet              descriptorSet       = VK_NULL_HANDLE; // bound to storage image

    // Command buffers
    VkCommandBuffer              cmdCompute          = VK_NULL_HANDLE; // from compute pool
    VkCommandBuffer              cmdGraphics         = VK_NULL_HANDLE; // from graphics pool

    // Internal chaining semaphore: compute -> graphics
    VkSemaphore                  computeFinished_    = VK_NULL_HANDLE;

    // Offscreen storage image + view + memory
    VkImage                      storageImage        = VK_NULL_HANDLE;
    VkDeviceMemory               storageMemory       = VK_NULL_HANDLE;
    VkImageView                  storageView         = VK_NULL_HANDLE;
    VkFormat                     storageFormat       = VK_FORMAT_R8G8B8A8_UNORM;

    // Cached SPIR-V
    std::vector<char>            shaderCode;
};
