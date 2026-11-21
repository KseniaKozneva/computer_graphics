#include "graphics_internal.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4189 4201 4324)
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace graphics::internal {

Context context{};

namespace {

vkb::Instance vk_b_instance{};
VkInstance    vk_instance = VK_NULL_HANDLE;
VkSurfaceKHR  vk_surface  = VK_NULL_HANDLE;

VkSwapchainKHR           vk_swapchain = VK_NULL_HANDLE;
std::vector<VkImage>     vk_swapchain_images;
std::vector<VkImageView> vk_swapchain_image_views;
std::vector<VkFramebuffer> vk_framebuffers;
uint32_t vk_swapchain_current_image = UINT32_MAX;

uint32_t vk_resize_width  = 0;
uint32_t vk_resize_height = 0;
bool     vk_resize_required = false;

VkImage       vk_depth_image = VK_NULL_HANDLE;
VmaAllocation vk_depth_alloc = VK_NULL_HANDLE;
VkImageView   vk_depth_view  = VK_NULL_HANDLE;

VkSemaphore              vk_semaphore_image_available = VK_NULL_HANDLE;
std::vector<VkSemaphore> vk_semaphores_image_finished;
VkFence                  vk_fence_frame_in_flight = VK_NULL_HANDLE;

VkCommandPool   vk_command_pool   = VK_NULL_HANDLE;
VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;

VkDescriptorPool vk_imgui_descriptor_pool = VK_NULL_HANDLE;

bool vk_dynamic_rendering_via_khr = false;
PFN_vkCmdBeginRendering pfn_begin_rendering = nullptr;
PFN_vkCmdEndRendering   pfn_end_rendering   = nullptr;

// ---------------------------------------------------------------------------

VkFormat selectDepthFormat(VkPhysicalDevice physical_device) {
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

bool createRenderPass() {
    const VkAttachmentDescription attachments[] = {
        {
            .format         = context.swapchain_format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        {
            .format         = context.depth_format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    const VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    const VkAttachmentReference depth_ref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    const VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    const VkSubpassDependency dependencies[] = {
        {
            .srcSubpass    = VK_SUBPASS_EXTERNAL,
            .dstSubpass    = 0,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass    = 0,
            .dstSubpass    = VK_SUBPASS_EXTERNAL,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = 0,
        },
    };

    const VkRenderPassCreateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 2,
        .pDependencies   = dependencies,
    };

    return vkCreateRenderPass(context.device, &info, nullptr, &context.render_pass) == VK_SUCCESS;
}

bool initializeImGui() {
    const VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
    };

    const VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000,
        .poolSizeCount = 4,
        .pPoolSizes    = pool_sizes,
    };

    if (vkCreateDescriptorPool(context.device, &pool_info, nullptr, &vk_imgui_descriptor_pool) != VK_SUCCESS) {
        std::cerr << "[vulkan] ImGui descriptor pool failed\n";
        return false;
    }

    const uint32_t count = static_cast<uint32_t>(vk_swapchain_images.size());

    ImGui_ImplVulkan_InitInfo init = {};
    init.Instance                 = vk_instance;
    init.PhysicalDevice           = context.physical_device;
    init.Device                   = context.device;
    init.QueueFamily              = context.graphics_queue_index;
    init.Queue                    = context.graphics_queue;
    init.DescriptorPool           = vk_imgui_descriptor_pool;
    init.MinImageCount            = count;
    init.ImageCount               = count;
    init.ApiVersion               = context.api_version;
    init.UseDynamicRendering      = false;
    init.PipelineInfoMain.RenderPass  = context.render_pass;
    init.PipelineInfoMain.Subpass     = 0;
    init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&init)) {
        std::cerr << "[vulkan] ImGui_ImplVulkan_Init failed\n";
        return false;
    }
    return true;
}

void destroySwapchainResources() {
    for (VkFramebuffer fb : vk_framebuffers) vkDestroyFramebuffer(context.device, fb, nullptr);
    vk_framebuffers.clear();

    for (VkSemaphore s : vk_semaphores_image_finished) vkDestroySemaphore(context.device, s, nullptr);
    vk_semaphores_image_finished.clear();

    if (vk_depth_view)  vkDestroyImageView(context.device, vk_depth_view, nullptr);
    if (vk_depth_image) vmaDestroyImage(context.allocator, vk_depth_image, vk_depth_alloc);
    vk_depth_view  = VK_NULL_HANDLE;
    vk_depth_image = VK_NULL_HANDLE;
    vk_depth_alloc = VK_NULL_HANDLE;

    for (VkImageView v : vk_swapchain_image_views) vkDestroyImageView(context.device, v, nullptr);
    vk_swapchain_image_views.clear();
    vk_swapchain_images.clear();

    if (vk_swapchain) vkDestroySwapchainKHR(context.device, vk_swapchain, nullptr);
    vk_swapchain = VK_NULL_HANDLE;
}

bool createDepthResources() {
    const VkImageCreateInfo image_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = context.depth_format,
        .extent        = { context.swapchain_extent.width, context.swapchain_extent.height, 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_AUTO };

    if (vmaCreateImage(context.allocator, &image_info, &alloc_info,
                       &vk_depth_image, &vk_depth_alloc, nullptr) != VK_SUCCESS) {
        std::cerr << "[vulkan] depth image creation failed\n";
        return false;
    }

    const VkImageViewCreateInfo view_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = vk_depth_image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = context.depth_format,
        .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
    };
    if (vkCreateImageView(context.device, &view_info, nullptr, &vk_depth_view) != VK_SUCCESS) {
        std::cerr << "[vulkan] depth image view creation failed\n";
        return false;
    }
    return true;
}

bool createFramebuffers() {
    VkImageView attachments[] = { VK_NULL_HANDLE, vk_depth_view };

    const VkFramebufferCreateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass      = context.render_pass,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .width           = context.swapchain_extent.width,
        .height          = context.swapchain_extent.height,
        .layers          = 1,
    };

    vk_framebuffers.resize(vk_swapchain_images.size());
    for (size_t i = 0; i < vk_swapchain_images.size(); ++i) {
        attachments[0] = vk_swapchain_image_views[i];
        if (vkCreateFramebuffer(context.device, &info, nullptr, &vk_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "[vulkan] framebuffer creation failed\n";
            return false;
        }
    }
    return true;
}

bool createImageFinishedSemaphores() {
    const VkSemaphoreCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vk_semaphores_image_finished.resize(vk_swapchain_images.size());
    for (auto& s : vk_semaphores_image_finished) {
        if (vkCreateSemaphore(context.device, &info, nullptr, &s) != VK_SUCCESS) return false;
    }
    return true;
}

bool buildSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain) {
    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physical_device, vk_surface, &caps) != VK_SUCCESS) {
        std::cerr << "[vulkan] vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed\n";
        return false;
    }

    if (caps.currentExtent.width != UINT32_MAX) {
        width  = caps.currentExtent.width;
        height = caps.currentExtent.height;
    } else {
        width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (width == 0 || height == 0) return false;

    vkb::SwapchainBuilder sb(context.physical_device, context.device, vk_surface,
                             context.graphics_queue_index, context.graphics_queue_index);
    auto result = sb.set_desired_extent(width, height)
                    .use_default_format_selection()
                    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                    .use_default_image_usage_flags()
                    .set_old_swapchain(old_swapchain)
                    .build();
    if (!result) {
        std::cerr << "[vulkan] swapchain build failed: " << result.error().message() << "\n";
        return false;
    }

    auto sc = result.value();
    vk_swapchain            = sc.swapchain;
    context.swapchain_format = sc.image_format;
    context.swapchain_extent = sc.extent;
    vk_swapchain_images      = sc.get_images().value();
    vk_swapchain_image_views = sc.get_image_views().value();

    // vkb::Swapchain при разрушении обёртки не уничтожает сам swapchain — освобождаем обёртку.
    sc.swapchain = VK_NULL_HANDLE;

    std::cout << "[vulkan] swapchain " << context.swapchain_extent.width << "x"
              << context.swapchain_extent.height << ", images=" << vk_swapchain_images.size() << "\n";
    return true;
}

bool rebuildSwapchain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        width  = context.swapchain_extent.width;
        height = context.swapchain_extent.height;
    }
    if (width == 0 || height == 0) return false;

    vkQueueWaitIdle(context.graphics_queue);

    const VkFormat old_format = context.swapchain_format;
    VkSwapchainKHR old_swapchain = vk_swapchain;

    // Уничтожаем всё, кроме самого старого swapchain (он нужен как oldSwapchain).
    for (VkFramebuffer fb : vk_framebuffers) vkDestroyFramebuffer(context.device, fb, nullptr);
    vk_framebuffers.clear();
    for (VkSemaphore s : vk_semaphores_image_finished) vkDestroySemaphore(context.device, s, nullptr);
    vk_semaphores_image_finished.clear();
    if (vk_depth_view)  vkDestroyImageView(context.device, vk_depth_view, nullptr);
    if (vk_depth_image) vmaDestroyImage(context.allocator, vk_depth_image, vk_depth_alloc);
    vk_depth_view = VK_NULL_HANDLE; vk_depth_image = VK_NULL_HANDLE; vk_depth_alloc = VK_NULL_HANDLE;
    for (VkImageView v : vk_swapchain_image_views) vkDestroyImageView(context.device, v, nullptr);
    vk_swapchain_image_views.clear();
    vk_swapchain_images.clear();
    vk_swapchain = VK_NULL_HANDLE;

    if (!buildSwapchain(width, height, old_swapchain)) {
        if (old_swapchain) vkDestroySwapchainKHR(context.device, old_swapchain, nullptr);
        return false;
    }
    if (old_swapchain) vkDestroySwapchainKHR(context.device, old_swapchain, nullptr);

    if (context.swapchain_format != old_format && old_format != VK_FORMAT_UNDEFINED) {
        // Крайне редкий случай: сменился формат поверхности — пересобираем render pass и ImGui.
        std::cout << "[vulkan] swapchain format changed, recreating render pass\n";
        ImGui_ImplVulkan_Shutdown();
        vkDestroyRenderPass(context.device, context.render_pass, nullptr);
        context.render_pass = VK_NULL_HANDLE;
        if (!createRenderPass()) return false;
        if (!createDepthResources()) return false;
        if (!createFramebuffers()) return false;
        if (!createImageFinishedSemaphores()) return false;
        if (!initializeImGui()) return false;
        vk_resize_required = false;
        return true;
    }

    if (!createDepthResources()) return false;
    if (!createFramebuffers()) return false;
    if (!createImageFinishedSemaphores()) return false;

    vk_resize_required = false;
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------

bool     dynamicRenderingViaKHR() { return vk_dynamic_rendering_via_khr; }
uint32_t apiVersion()             { return context.api_version; }

void cmdBeginRendering(VkCommandBuffer cmd, const VkRenderingInfo* info) {
    pfn_begin_rendering(cmd, info);
}
void cmdEndRendering(VkCommandBuffer cmd) {
    pfn_end_rendering(cmd);
}

void beginMainRenderPass(VkCommandBuffer cmd) {
    const VkClearValue clear_values[] = {
        { .color = { { 0.1f, 0.1f, 0.12f, 1.0f } } },
        { .depthStencil = { 1.0f, 0 } },
    };
    const VkRenderPassBeginInfo info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = context.render_pass,
        .framebuffer     = vk_framebuffers[vk_swapchain_current_image],
        .renderArea      = { { 0, 0 }, context.swapchain_extent },
        .clearValueCount = 2,
        .pClearValues    = clear_values,
    };
    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
}

bool initialize(GLFWwindow* const window) {
    // --- Версия загрузчика -------------------------------------------------
    uint32_t loader_version = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&loader_version);
    std::cout << "[vulkan] loader API " << VK_VERSION_MAJOR(loader_version) << "."
              << VK_VERSION_MINOR(loader_version) << "." << VK_VERSION_PATCH(loader_version) << "\n";

    // ВАЖНО: core-функции vkCmdBeginRendering/vkCmdEndRendering доступны только
    // если instance создан с apiVersion >= 1.3. Иначе dispatch-table содержит NULL.
    const uint32_t requested_api = std::min(loader_version, static_cast<uint32_t>(VK_API_VERSION_1_3));
    if (requested_api < VK_API_VERSION_1_3) {
        std::cerr << "[vulkan] Required Vulkan 1.3 loader (dynamic rendering). Update Vulkan SDK/driver.\n";
        return false;
    }

    vkb::InstanceBuilder ib;
    auto ibr = ib.require_api_version(requested_api)
                 .request_validation_layers()
                 .use_default_debug_messenger()
                 .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                 .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                 .build();
    if (!ibr) {
        std::cerr << "[vulkan] instance build failed: " << ibr.error().message() << "\n";
        return false;
    }
    vk_b_instance = ibr.value();
    vk_instance   = vk_b_instance.instance;
    context.api_version = requested_api;

    if (glfwCreateWindowSurface(vk_instance, window, nullptr, &vk_surface) != VK_SUCCESS) {
        std::cerr << "[vulkan] glfwCreateWindowSurface failed\n";
        return false;
    }

    // --- Физическое устройство ---------------------------------------------
    vkb::PhysicalDeviceSelector pds(vk_b_instance, vk_surface);
    auto pdsr = pds.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                  .require_present()
                  .set_minimum_version(1, 2)
                  .select();
    if (!pdsr) {
        std::cerr << "[vulkan] physical device selection failed: " << pdsr.error().message() << "\n";
        return false;
    }
    auto vkb_physical_device = pdsr.value();

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(vkb_physical_device.physical_device, &props);
    std::cout << "[vulkan] GPU: " << props.deviceName << ", API "
              << VK_VERSION_MAJOR(props.apiVersion) << "." << VK_VERSION_MINOR(props.apiVersion) << "\n";

    context.depth_format = selectDepthFormat(vkb_physical_device.physical_device);
    if (context.depth_format == VK_FORMAT_UNDEFINED) {
        std::cerr << "[vulkan] no supported depth format\n";
        return false;
    }

    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(vkb_physical_device.physical_device, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateDeviceExtensionProperties(vkb_physical_device.physical_device, nullptr, &ext_count, exts.data());
    bool has_khr_dynamic_rendering = false;
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
            has_khr_dynamic_rendering = true;
            break;
        }
    }

    const bool via_core_13 = props.apiVersion >= VK_API_VERSION_1_3;
    if (!via_core_13 && !has_khr_dynamic_rendering) {
        std::cerr << "[vulkan] dynamic rendering is NOT supported by this GPU/driver\n";
        return false;
    }
    vk_dynamic_rendering_via_khr = !via_core_13;

    // --- Поддерживаемые фичи ------------------------------------------------
    VkPhysicalDeviceVulkan13Features            supported13{};
    supported13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR supported_khr{};
    supported_khr.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 supported2{};
    supported2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported2.pNext = via_core_13 ? static_cast<void*>(&supported13) : static_cast<void*>(&supported_khr);
    vkGetPhysicalDeviceFeatures2(vkb_physical_device.physical_device, &supported2);

    if (!supported2.features.fillModeNonSolid) {
        std::cerr << "[vulkan] fillModeNonSolid is not supported (needed for line gizmos)\n";
        return false;
    }
    context.sampler_anisotropy = supported2.features.samplerAnisotropy == VK_TRUE;
    context.max_anisotropy     = context.sampler_anisotropy ? std::min(8.0f, props.limits.maxSamplerAnisotropy) : 1.0f;

    VkPhysicalDeviceVulkan13Features            requested13{};
    requested13.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    requested13.dynamicRendering   = VK_TRUE;
    requested13.synchronization2   = supported13.synchronization2;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR requested_khr{};
    requested_khr.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    requested_khr.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 requested2{};
    requested2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    requested2.features.fillModeNonSolid  = VK_TRUE;
    requested2.features.samplerAnisotropy = context.sampler_anisotropy ? VK_TRUE : VK_FALSE;
    requested2.pNext = via_core_13 ? static_cast<void*>(&requested13) : static_cast<void*>(&requested_khr);

    if (!via_core_13) {
        vkb_physical_device.enable_extension_if_present(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }

    vkb::DeviceBuilder db(vkb_physical_device);
    db.add_pNext(&requested2);
    auto dbr = db.build();
    if (!dbr) {
        std::cerr << "[vulkan] device build failed: " << dbr.error().message() << "\n";
        return false;
    }
    auto vkb_device = dbr.value();

    context.physical_device      = vkb_device.physical_device;
    context.device               = vkb_device.device;
    context.graphics_queue       = vkb_device.get_queue(vkb::QueueType::graphics).value();
    context.graphics_queue_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    std::cout << "[vulkan] dynamic rendering via "
              << (via_core_13 ? "Vulkan 1.3 core" : "VK_KHR_dynamic_rendering") << "\n";

    // --- Явная загрузка entry points (страховка от NULL-вызова) -------------
    pfn_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
        vkGetDeviceProcAddr(context.device, "vkCmdBeginRendering"));
    if (!pfn_begin_rendering) {
        pfn_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRendering>(
            vkGetDeviceProcAddr(context.device, "vkCmdBeginRenderingKHR"));
    }
    pfn_end_rendering = reinterpret_cast<PFN_vkCmdEndRendering>(
        vkGetDeviceProcAddr(context.device, "vkCmdEndRendering"));
    if (!pfn_end_rendering) {
        pfn_end_rendering = reinterpret_cast<PFN_vkCmdEndRendering>(
            vkGetDeviceProcAddr(context.device, "vkCmdEndRenderingKHR"));
    }
    if (!pfn_begin_rendering || !pfn_end_rendering) {
        std::cerr << "[vulkan] dynamic rendering entry points are NULL\n";
        return false;
    }

    // --- VMA ----------------------------------------------------------------
    const VmaAllocatorCreateInfo allocator_info = {
        .physicalDevice   = context.physical_device,
        .device           = context.device,
        .instance         = vk_instance,
        .vulkanApiVersion = context.api_version,
    };
    if (vmaCreateAllocator(&allocator_info, &context.allocator) != VK_SUCCESS) {
        std::cerr << "[vulkan] vmaCreateAllocator failed\n";
        return false;
    }

    // --- Swapchain + depth + render pass + framebuffers ----------------------
    int fb_width = 0, fb_height = 0;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    if (!buildSwapchain(static_cast<uint32_t>(fb_width), static_cast<uint32_t>(fb_height), VK_NULL_HANDLE)) return false;
    if (!createDepthResources()) return false;
    if (!createRenderPass()) {
        std::cerr << "[vulkan] render pass creation failed\n";
        return false;
    }
    if (!createFramebuffers()) return false;
    if (!createImageFinishedSemaphores()) return false;

    vk_swapchain_current_image = UINT32_MAX;

    const VkSemaphoreCreateInfo semaphore_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    if (vkCreateSemaphore(context.device, &semaphore_info, nullptr, &vk_semaphore_image_available) != VK_SUCCESS) return false;

    const VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    if (vkCreateFence(context.device, &fence_info, nullptr, &vk_fence_frame_in_flight) != VK_SUCCESS) return false;

    const VkCommandPoolCreateInfo pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphics_queue_index,
    };
    if (vkCreateCommandPool(context.device, &pool_info, nullptr, &vk_command_pool) != VK_SUCCESS) return false;

    const VkCommandBufferAllocateInfo cb_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = vk_command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vkAllocateCommandBuffers(context.device, &cb_info, &vk_command_buffer) != VK_SUCCESS) return false;

    if (!initializeImGui()) return false;

    std::cout << "[vulkan] initialize done\n";
    return true;
}

void shutdown() {
    if (context.graphics_queue) vkQueueWaitIdle(context.graphics_queue);

    if (vk_imgui_descriptor_pool) ImGui_ImplVulkan_Shutdown();
    if (vk_imgui_descriptor_pool) vkDestroyDescriptorPool(context.device, vk_imgui_descriptor_pool, nullptr);
    vk_imgui_descriptor_pool = VK_NULL_HANDLE;

    if (vk_command_pool) vkDestroyCommandPool(context.device, vk_command_pool, nullptr);
    if (vk_fence_frame_in_flight) vkDestroyFence(context.device, vk_fence_frame_in_flight, nullptr);
    if (vk_semaphore_image_available) vkDestroySemaphore(context.device, vk_semaphore_image_available, nullptr);

    destroySwapchainResources();

    if (context.render_pass) vkDestroyRenderPass(context.device, context.render_pass, nullptr);
    if (context.allocator)   vmaDestroyAllocator(context.allocator);
    context.allocator = VK_NULL_HANDLE;

    if (context.device) vkDestroyDevice(context.device, nullptr);
    context.device = VK_NULL_HANDLE;

    if (vk_surface) vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
    vk_surface = VK_NULL_HANDLE;

    if (vk_instance) vkb::destroy_instance(vk_b_instance);
    vk_instance = VK_NULL_HANDLE;
}

void resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    vk_resize_width    = width;
    vk_resize_height   = height;
    vk_resize_required = true;
}

FrameData prepare() {
    vkWaitForFences(context.device, 1, &vk_fence_frame_in_flight, VK_TRUE, UINT64_MAX);

    if (vk_resize_required) {
        if (!rebuildSwapchain(vk_resize_width, vk_resize_height)) return {};
    }

    for (int attempt = 0; attempt < 4; ++attempt) {
        const VkResult acquired = vkAcquireNextImageKHR(context.device, vk_swapchain, UINT64_MAX,
                                                        vk_semaphore_image_available, VK_NULL_HANDLE,
                                                        &vk_swapchain_current_image);
        if (acquired == VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR) {
            if (acquired == VK_SUBOPTIMAL_KHR) vk_resize_required = true;
            break;
        }
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
            if (!rebuildSwapchain(vk_resize_width, vk_resize_height)) return {};
            continue;
        }
        std::cerr << "[vulkan] vkAcquireNextImageKHR failed: " << acquired << "\n";
        return {};
    }

    if (vk_swapchain_current_image >= vk_framebuffers.size()) return {};

    vkResetFences(context.device, 1, &vk_fence_frame_in_flight);
    vkResetCommandBuffer(vk_command_buffer, 0);

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vkBeginCommandBuffer(vk_command_buffer, &begin_info) != VK_SUCCESS) return {};

    // Render pass НЕ начинается здесь: сначала shadow-проходы (dynamic rendering),
    // затем application::render() вызовет beginMainRenderPass().
    return { vk_framebuffers[vk_swapchain_current_image], vk_command_buffer, true };
}

void submitAndPresent() {
    if (ImDrawData* draw_data = ImGui::GetDrawData()) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, vk_command_buffer);
    }

    vkCmdEndRenderPass(vk_command_buffer);
    if (vkEndCommandBuffer(vk_command_buffer) != VK_SUCCESS) {
        std::cerr << "[vulkan] vkEndCommandBuffer failed\n";
        return;
    }

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submit = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &vk_semaphore_image_available,
        .pWaitDstStageMask    = &wait_stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &vk_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &vk_semaphores_image_finished[vk_swapchain_current_image],
    };
    if (vkQueueSubmit(context.graphics_queue, 1, &submit, vk_fence_frame_in_flight) != VK_SUCCESS) {
        std::cerr << "[vulkan] vkQueueSubmit failed\n";
        return;
    }

    const VkPresentInfoKHR present = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vk_semaphores_image_finished[vk_swapchain_current_image],
        .swapchainCount     = 1,
        .pSwapchains        = &vk_swapchain,
        .pImageIndices      = &vk_swapchain_current_image,
    };
    const VkResult result = vkQueuePresentKHR(context.graphics_queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vk_resize_required) {
        rebuildSwapchain(vk_resize_width, vk_resize_height);
    } else if (result != VK_SUCCESS) {
        std::cerr << "[vulkan] vkQueuePresentKHR failed: " << result << "\n";
    }
}

VkCommandBuffer beginSingleTimeCommands() {
    const VkCommandBufferAllocateInfo alloc = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = vk_command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(context.device, &alloc, &cmd);

    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void endSingleTimeCommands(VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);
    const VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &command_buffer,
    };
    vkQueueSubmit(context.graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.graphics_queue);
    vkFreeCommandBuffers(context.device, vk_command_pool, 1, &command_buffer);
}

} // namespace graphics::internal