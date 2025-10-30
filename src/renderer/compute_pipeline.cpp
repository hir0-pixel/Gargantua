#include "compute_pipeline.h"
#include "vulkan_context.h"
#include "swapchain.h"

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create shader module.");
    }
    return mod;
}

std::vector<char> ComputePipeline::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) throw std::runtime_error(std::string("[Compute] Failed to open shader file: ") + path);
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}

ComputePipeline::ComputePipeline(VulkanContext& context, Swapchain& swapchain, const std::string& shaderSpvPath)
    : ctx(context), sc(swapchain), device(context.getDevice()) {

    // 1) Read shader first
    shaderCode = readFile(shaderSpvPath);

    // 2) Create Vulkan objects
    createDescriptorSetLayout();
    createPipelineLayout();
    createPipelineFromCode(shaderCode);
    createStorageImage();
    createDescriptorPoolAndSets();
    allocateCommandBuffers();

    // internal semaphore (compute -> graphics)
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkCreateSemaphore(device, &sci, nullptr, &computeFinished_) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create internal semaphore.");
    }

    std::cout << "[Compute] Pipeline ready (offscreen storage image + blit).\n";
}

ComputePipeline::~ComputePipeline() {
    VkDevice dev = device;

    if (computeFinished_)     { vkDestroySemaphore(dev, computeFinished_, nullptr); computeFinished_ = VK_NULL_HANDLE; }
    if (descriptorPool)       vkDestroyDescriptorPool(dev, descriptorPool, nullptr);
    if (pipeline)             vkDestroyPipeline(dev, pipeline, nullptr);
    if (pipelineLayout)       vkDestroyPipelineLayout(dev, pipelineLayout, nullptr);
    if (descriptorSetLayout)  vkDestroyDescriptorSetLayout(dev, descriptorSetLayout, nullptr);

    destroyStorageImage();
    // Command buffers are freed with their pools in VulkanContext
}

void ComputePipeline::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create descriptor set layout.");
    }
}

void ComputePipeline::createPipelineLayout() {
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &ci, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create pipeline layout.");
    }
}

void ComputePipeline::createPipelineFromCode(const std::vector<char>& code) {
    VkShaderModule mod = createShaderModule(device, code);

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = stage;
    ci.layout = pipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, mod, nullptr);
        throw std::runtime_error("[Compute] Failed to create compute pipeline.");
    }

    vkDestroyShaderModule(device, mod, nullptr);
}

uint32_t ComputePipeline::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.getPhysicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw std::runtime_error("[Compute] Suitable memory type not found.");
}

void ComputePipeline::createStorageImage() {
    storageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent = sc.getExtent();

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = storageFormat;
    ici.extent = { extent.width, extent.height, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &ici, nullptr, &storageImage) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create storage image.");
    }

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, storageImage, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &mai, nullptr, &storageMemory) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to allocate storage image memory.");
    }

    vkBindImageMemory(device, storageImage, storageMemory, 0);

    // View
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = storageImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = storageFormat;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &vci, nullptr, &storageView) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create storage image view.");
    }

    // Transition storage image to GENERAL (compute writes)
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = ctx.getComputeCommandPool();
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer tmp;
    vkAllocateCommandBuffers(device, &ai, &tmp);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmp, &bi);

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = storageImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(tmp, &dep);
    vkEndCommandBuffer(tmp);

    VkCommandBufferSubmitInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cb.commandBuffer = tmp;

    VkSubmitInfo2 sub{};
    sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    sub.commandBufferInfoCount = 1;
    sub.pCommandBufferInfos = &cb;

    vkQueueSubmit2(ctx.getComputeQueue(), 1, &sub, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.getComputeQueue());

    vkFreeCommandBuffers(device, ctx.getComputeCommandPool(), 1, &tmp);
}

void ComputePipeline::destroyStorageImage() {
    if (storageView)   { vkDestroyImageView(device, storageView, nullptr); storageView = VK_NULL_HANDLE; }
    if (storageImage)  { vkDestroyImage(device, storageImage, nullptr); storageImage = VK_NULL_HANDLE; }
    if (storageMemory) { vkFreeMemory(device, storageMemory, nullptr); storageMemory = VK_NULL_HANDLE; }
}

void ComputePipeline::createDescriptorPoolAndSets() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(device, &pci, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to create descriptor pool.");
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &ai, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to allocate descriptor set.");
    }

    VkDescriptorImageInfo info{};
    info.imageView = storageView;
    info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &info;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void ComputePipeline::allocateCommandBuffers() {
    // Compute CMDBUF
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = ctx.getComputeCommandPool();
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &ai, &cmdCompute) != VK_SUCCESS) {
            throw std::runtime_error("[Compute] Failed to allocate compute command buffer.");
        }
    }
    // Graphics CMDBUF
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = ctx.getGraphicsCommandPool();
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &ai, &cmdGraphics) != VK_SUCCESS) {
            throw std::runtime_error("[Compute] Failed to allocate graphics command buffer.");
        }
    }
}

void ComputePipeline::recreate() {
    vkDeviceWaitIdle(device);
    destroyStorageImage();
    createStorageImage();

    // Rebuild descriptor set pool+set (points to storageView)
    if (descriptorPool) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    createDescriptorPoolAndSets();
}

void ComputePipeline::dispatch(uint32_t imageIndex, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore) {
    // ------- 1) COMPUTE: record on compute CMDBUF -------
    vkResetCommandBuffer(cmdCompute, 0);
    VkCommandBufferBeginInfo biCompute{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    biCompute.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdCompute, &biCompute);

    vkCmdBindPipeline(cmdCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdCompute, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    VkExtent2D extent = sc.getExtent();
    const uint32_t wgX = (extent.width  + 15) / 16;
    const uint32_t wgY = (extent.height + 15) / 16;
    vkCmdDispatch(cmdCompute, wgX, wgY, 1);

    // storage GENERAL (shader write) -> TRANSFER_SRC for blit (later, on graphics)
    VkImageMemoryBarrier2 storageToSrc{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    storageToSrc.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    storageToSrc.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    storageToSrc.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    storageToSrc.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    storageToSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageToSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    storageToSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageToSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageToSrc.image = storageImage;
    storageToSrc.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo depCompute{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depCompute.imageMemoryBarrierCount = 1;
    depCompute.pImageMemoryBarriers = &storageToSrc;

    vkCmdPipelineBarrier2(cmdCompute, &depCompute);
    vkEndCommandBuffer(cmdCompute);

    // Submit compute, waiting on acquire-semaphore (if provided), and signal internal computeFinished_
    VkCommandBufferSubmitInfo cbCompute{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cbCompute.commandBuffer = cmdCompute;

    VkSemaphoreSubmitInfo waitAcquire{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    waitAcquire.semaphore = waitSemaphore;
    waitAcquire.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    VkSemaphoreSubmitInfo signalComputeDone{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    signalComputeDone.semaphore = computeFinished_;
    signalComputeDone.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    VkSubmitInfo2 submitCompute{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    if (waitSemaphore != VK_NULL_HANDLE) {
        submitCompute.waitSemaphoreInfoCount = 1;
        submitCompute.pWaitSemaphoreInfos = &waitAcquire;
    }
    submitCompute.commandBufferInfoCount = 1;
    submitCompute.pCommandBufferInfos = &cbCompute;
    submitCompute.signalSemaphoreInfoCount = 1;
    submitCompute.pSignalSemaphoreInfos = &signalComputeDone;

    if (vkQueueSubmit2(ctx.getComputeQueue(), 1, &submitCompute, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to submit compute pass.");
    }

    // ------- 2) GRAPHICS: record blit on graphics CMDBUF -------
    vkResetCommandBuffer(cmdGraphics, 0);
    VkCommandBufferBeginInfo biGfx{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    biGfx.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdGraphics, &biGfx);

    VkImage swapImg = sc.getImage(imageIndex);

    // Transition swapchain to TRANSFER_DST
    VkImageMemoryBarrier2 presentToDst{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    presentToDst.srcStageMask = VK_PIPELINE_STAGE_2_NONE; // fresh from acquire
    presentToDst.srcAccessMask = 0;
    presentToDst.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    presentToDst.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    presentToDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;         // safe after acquire
    presentToDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentToDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentToDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentToDst.image = swapImg;
    presentToDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo depGfxBegin{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depGfxBegin.imageMemoryBarrierCount = 1;
    depGfxBegin.pImageMemoryBarriers = &presentToDst;
    vkCmdPipelineBarrier2(cmdGraphics, &depGfxBegin);

    // Blit storage -> swapchain
    VkOffset3D src0{0, 0, 0}, src1{ (int)extent.width, (int)extent.height, 1 };
    VkOffset3D dst0{0, 0, 0}, dst1{ (int)extent.width, (int)extent.height, 1 };

    VkImageBlit blit{};
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[0] = src0;
    blit.srcOffsets[1] = src1;
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[0] = dst0;
    blit.dstOffsets[1] = dst1;

    vkCmdBlitImage(
        cmdGraphics,
        storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapImg,      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_NEAREST
    );

    // Transition swapchain back to PRESENT, storage back to GENERAL
    VkImageMemoryBarrier2 dstToPresent{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    dstToPresent.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    dstToPresent.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    dstToPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    dstToPresent.dstAccessMask = 0;
    dstToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    dstToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstToPresent.image = swapImg;
    dstToPresent.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageMemoryBarrier2 storageBack{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    storageBack.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    storageBack.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    storageBack.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    storageBack.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    storageBack.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    storageBack.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    storageBack.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageBack.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    storageBack.image = storageImage;
    storageBack.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkImageMemoryBarrier2 barriersEnd[2] = { dstToPresent, storageBack };
    VkDependencyInfo depGfxEnd{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depGfxEnd.imageMemoryBarrierCount = 2;
    depGfxEnd.pImageMemoryBarriers = barriersEnd;
    vkCmdPipelineBarrier2(cmdGraphics, &depGfxEnd);

    vkEndCommandBuffer(cmdGraphics);

    // Submit graphics: wait on internal computeFinished_, signal app-provided signalSemaphore
    VkCommandBufferSubmitInfo cbGfx{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    cbGfx.commandBuffer = cmdGraphics;

    VkSemaphoreSubmitInfo waitComputeDone{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    waitComputeDone.semaphore = computeFinished_;
    waitComputeDone.stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;

    VkSemaphoreSubmitInfo signalRenderDone{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    signalRenderDone.semaphore = signalSemaphore;
    signalRenderDone.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submitGfx{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitGfx.waitSemaphoreInfoCount = 1;
    submitGfx.pWaitSemaphoreInfos = &waitComputeDone;
    submitGfx.commandBufferInfoCount = 1;
    submitGfx.pCommandBufferInfos = &cbGfx;
    submitGfx.signalSemaphoreInfoCount = (signalSemaphore != VK_NULL_HANDLE) ? 1u : 0u;
    submitGfx.pSignalSemaphoreInfos = (signalSemaphore != VK_NULL_HANDLE) ? &signalRenderDone : nullptr;

    if (vkQueueSubmit2(ctx.getGraphicsQueue(), 1, &submitGfx, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("[Compute] Failed to submit graphics blit.");
    }

    // TEMP safety: fully serialize to avoid semaphore reuse warnings
    vkQueueWaitIdle(ctx.getGraphicsQueue());
}
