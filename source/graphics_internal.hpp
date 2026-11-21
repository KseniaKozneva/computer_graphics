#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace graphics::internal {

struct Context {
    VkPhysicalDevice physical_device      = VK_NULL_HANDLE;
    VkDevice         device               = VK_NULL_HANDLE;
    VmaAllocator     allocator            = VK_NULL_HANDLE;
    VkQueue          graphics_queue       = VK_NULL_HANDLE;
    uint32_t         graphics_queue_index = 0;
    VkFormat         swapchain_format     = VK_FORMAT_UNDEFINED;
    VkExtent2D       swapchain_extent     = { 0, 0 };
    VkFormat         depth_format         = VK_FORMAT_UNDEFINED;
    VkRenderPass     render_pass          = VK_NULL_HANDLE;
    uint32_t         api_version          = VK_API_VERSION_1_0;
    bool             sampler_anisotropy   = false;
    float            max_anisotropy       = 1.0f;
};

struct FrameData {
    VkFramebuffer   framebuffer    = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    bool            valid          = false;
};

extern Context context;

bool initialize(GLFWwindow* window);
void shutdown();
void resize(uint32_t width, uint32_t height);

FrameData prepare();
void      submitAndPresent();

VkCommandBuffer beginSingleTimeCommands();
void            endSingleTimeCommands(VkCommandBuffer command_buffer);

// Основной render pass стартует уже ПОСЛЕ shadow-проходов.
void beginMainRenderPass(VkCommandBuffer cmd);

// Dynamic rendering: вызываем только через загруженные указатели,
// иначе при instance < 1.3 получаем вызов по NULL.
void cmdBeginRendering(VkCommandBuffer cmd, const VkRenderingInfo* info);
void cmdEndRendering(VkCommandBuffer cmd);

bool     dynamicRenderingViaKHR();
uint32_t apiVersion();

} // namespace graphics::internal