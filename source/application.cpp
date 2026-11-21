#include "application.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4189 4244 4245 4456 4457 4996)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace application {

namespace fs = std::filesystem;
using graphics::internal::FrameData;

// ---------------------------------------------------------------------------
// Геометрия: параллелепипед a x b x c
// ---------------------------------------------------------------------------
constexpr float a = 0.8f;
constexpr float b = 0.6f;
constexpr float c = 0.5f;

const std::vector<Vertex> vertices = {
    {{-a, -b, -c}, {0, -1, 0}, {0, 0}}, {{ a, -b, -c}, {0, -1, 0}, {1, 0}},
    {{ a, -b,  c}, {0, -1, 0}, {1, 1}}, {{-a, -b,  c}, {0, -1, 0}, {0, 1}},
    {{-a,  b, -c}, {0, 1, 0}, {0, 0}},  {{ a,  b, -c}, {0, 1, 0}, {1, 0}},
    {{ a,  b,  c}, {0, 1, 0}, {1, 1}},  {{-a,  b,  c}, {0, 1, 0}, {0, 1}},
    {{-a, -b,  c}, {0, 0, 1}, {0, 0}},  {{ a, -b,  c}, {0, 0, 1}, {1, 0}},
    {{ a,  b,  c}, {0, 0, 1}, {1, 1}},  {{-a,  b,  c}, {0, 0, 1}, {0, 1}},
    {{-a, -b, -c}, {0, 0, -1}, {0, 0}}, {{ a, -b, -c}, {0, 0, -1}, {1, 0}},
    {{ a,  b, -c}, {0, 0, -1}, {1, 1}}, {{-a,  b, -c}, {0, 0, -1}, {0, 1}},
    {{-a, -b, -c}, {-1, 0, 0}, {0, 0}}, {{-a, -b,  c}, {-1, 0, 0}, {1, 0}},
    {{-a,  b,  c}, {-1, 0, 0}, {1, 1}}, {{-a,  b, -c}, {-1, 0, 0}, {0, 1}},
    {{ a, -b, -c}, {1, 0, 0}, {0, 0}},  {{ a, -b,  c}, {1, 0, 0}, {1, 0}},
    {{ a,  b,  c}, {1, 0, 0}, {1, 1}},  {{ a,  b, -c}, {1, 0, 0}, {0, 1}},
};

const std::vector<uint16_t> indices = {
    0, 2, 1,   0, 3, 2,
    4, 5, 6,   4, 6, 7,
    8, 10, 9,  8, 11, 10,
    12, 13, 14, 12, 14, 15,
    16, 18, 17, 16, 19, 18,
    20, 21, 22, 20, 22, 23
};

struct LineVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct LineUBO {
    alignas(16) glm::mat4 mvp;
    alignas(16) glm::vec3 tint;
};

struct TextureImage {
    VkImage       image     = VK_NULL_HANDLE;
    VmaAllocation alloc     = VK_NULL_HANDLE;
    VkImageView   view      = VK_NULL_HANDLE;
    uint32_t      mipLevels = 1;
};

struct Material {
    std::string  name;
    TextureImage albedo;
    TextureImage specular;
    TextureImage emissive;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

constexpr uint32_t SHADOW_MAP_SIZE      = 2048;
constexpr size_t   LINE_VERTEX_CAPACITY = 50000;
constexpr int      VISUALIZER_COUNT     = MAX_LIGHTS * 2;

// --- Ресурсы ---------------------------------------------------------------
VkFormat      shadowDepthFormat   = VK_FORMAT_UNDEFINED;
VkImage       shadowMapImage      = VK_NULL_HANDLE;
VmaAllocation shadowMapAlloc      = VK_NULL_HANDLE;
VkImageView   shadowMapView       = VK_NULL_HANDLE;
VkSampler     shadowMapSampler    = VK_NULL_HANDLE;
VkImageLayout shadowMapLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

VkImage       spotShadowMapImage  = VK_NULL_HANDLE;
VmaAllocation spotShadowMapAlloc  = VK_NULL_HANDLE;
VkImageView   spotShadowMapView   = VK_NULL_HANDLE;
VkImageLayout spotShadowMapLayout = VK_IMAGE_LAYOUT_UNDEFINED;

VkBuffer      vertexBuffer = VK_NULL_HANDLE;      VmaAllocation vertexBufferAlloc = VK_NULL_HANDLE;
VkBuffer      indexBuffer  = VK_NULL_HANDLE;      VmaAllocation indexBufferAlloc  = VK_NULL_HANDLE;
VkBuffer      sceneUboBuffer = VK_NULL_HANDLE;    VmaAllocation sceneUboBufferAlloc = VK_NULL_HANDLE;
VkBuffer      pointLightBuffer = VK_NULL_HANDLE;  VmaAllocation pointLightBufferAlloc = VK_NULL_HANDLE;
VkBuffer      spotLightBuffer = VK_NULL_HANDLE;   VmaAllocation spotLightBufferAlloc = VK_NULL_HANDLE;
VkBuffer      modelUboBuffer = VK_NULL_HANDLE;    VmaAllocation modelUboBufferAlloc = VK_NULL_HANDLE;
size_t        modelUboAlignedSize = 0;
VkBuffer      lightUboBuffer = VK_NULL_HANDLE;     VmaAllocation lightUboBufferAlloc = VK_NULL_HANDLE;
VkBuffer      spotLightUboBuffer = VK_NULL_HANDLE; VmaAllocation spotLightUboBufferAlloc = VK_NULL_HANDLE;

VkBuffer        visualizerUboBuffers[VISUALIZER_COUNT] = {};
VmaAllocation   visualizerUboBufferAllocs[VISUALIZER_COUNT] = {};
VkDescriptorSet visualizerDescriptorSets[VISUALIZER_COUNT] = {};

VkBuffer        lineVertexBuffer = VK_NULL_HANDLE; VmaAllocation lineVertexBufferAlloc = VK_NULL_HANDLE;
VkBuffer        lineUboBuffer = VK_NULL_HANDLE;    VmaAllocation lineUboBufferAlloc = VK_NULL_HANDLE;
VkDescriptorSet lineDescriptorSet = VK_NULL_HANDLE;

VkSampler    textureSampler = VK_NULL_HANDLE;
TextureImage blackTexture;
std::vector<Material> materials;

VkDescriptorSetLayout sceneDescriptorSetLayout      = VK_NULL_HANDLE;
VkDescriptorSetLayout modelDescriptorSetLayout      = VK_NULL_HANDLE;
VkDescriptorSetLayout visualizerDescriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout lineDescriptorSetLayout       = VK_NULL_HANDLE;
VkDescriptorSetLayout shadowDescriptorSetLayout     = VK_NULL_HANDLE;

VkPipelineLayout pipelineLayout           = VK_NULL_HANDLE;
VkPipelineLayout visualizerPipelineLayout = VK_NULL_HANDLE;
VkPipelineLayout linePipelineLayout       = VK_NULL_HANDLE;
VkPipelineLayout shadowPipelineLayout     = VK_NULL_HANDLE;

VkPipeline graphicsPipeline   = VK_NULL_HANDLE;
VkPipeline visualizerPipeline = VK_NULL_HANDLE;
VkPipeline linePipeline       = VK_NULL_HANDLE;
VkPipeline shadowPipeline     = VK_NULL_HANDLE;

VkDescriptorPool descriptorPool          = VK_NULL_HANDLE;
VkDescriptorSet  sceneDescriptorSet      = VK_NULL_HANDLE;
VkDescriptorSet  shadowDescriptorSet     = VK_NULL_HANDLE;
VkDescriptorSet  spotShadowDescriptorSet = VK_NULL_HANDLE;

AppState state;

uint64_t g_frameCounter = 0;

// ---------------------------------------------------------------------------
// Утилиты
// ---------------------------------------------------------------------------
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

static VkResult createBufferChecked(VkDeviceSize size, VkBufferUsageFlags usage,
                                    VmaMemoryUsage memoryUsage, VkBuffer& buffer,
                                    VmaAllocation& alloc, const char* name) {
    const VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage };
    const VmaAllocationCreateInfo ai = { .usage = memoryUsage };
    const VkResult r = vmaCreateBuffer(graphics::internal::context.allocator, &bi, &ai, &buffer, &alloc, nullptr);
    if (r != VK_SUCCESS) std::cerr << "[app] vmaCreateBuffer failed for " << name << ": " << r << "\n";
    return r;
}

static VkResult createImageChecked(const VkImageCreateInfo* ci, VkImage& image,
                                   VmaAllocation& alloc, const char* name) {
    VmaAllocationCreateInfo ai = {};
    ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    const VkResult r = vmaCreateImage(graphics::internal::context.allocator, ci, &ai, &image, &alloc, nullptr);
    if (r != VK_SUCCESS) std::cerr << "[app] vmaCreateImage failed for " << name << ": " << r << "\n";
    return r;
}

static void writeBuffer(VmaAllocation alloc, const void* data, size_t size, size_t offset = 0) {
    if (!alloc) return;
    void* mapped = nullptr;
    if (vmaMapMemory(graphics::internal::context.allocator, alloc, &mapped) == VK_SUCCESS && mapped) {
        std::memcpy(static_cast<char*>(mapped) + offset, data, size);
        vmaUnmapMemory(graphics::internal::context.allocator, alloc);
    } else {
        std::cerr << "[app] vmaMapMemory failed\n";
    }
}

static void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = graphics::internal::beginSingleTimeCommands();
    const VkBufferCopy region = { 0, 0, size };
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    graphics::internal::endSingleTimeCommands(cmd);
}

static bool uploadBufferToGPU(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                              VkBuffer& buffer, VmaAllocation& alloc, const char* name) {
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    if (createBufferChecked(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            staging, stagingAlloc, name) != VK_SUCCESS) return false;
    writeBuffer(stagingAlloc, data, static_cast<size_t>(size));

    if (createBufferChecked(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                            buffer, alloc, name) != VK_SUCCESS) {
        vmaDestroyBuffer(graphics::internal::context.allocator, staging, stagingAlloc);
        return false;
    }
    copyBuffer(staging, buffer, size);
    vmaDestroyBuffer(graphics::internal::context.allocator, staging, stagingAlloc);
    return true;
}

static void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void generateMipmaps(VkCommandBuffer cmd, VkImage image, int w, int h, uint32_t levels) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    int mw = w, mh = h;
    for (uint32_t i = 1; i < levels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit = {};
        blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
        blit.srcOffsets[1]  = { mw, mh, 1 };
        blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
        blit.dstOffsets[1]  = { mw > 1 ? mw / 2 : 1, mh > 1 ? mh / 2 : 1, 1 };
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        mw = mw > 1 ? mw / 2 : 1;
        mh = mh > 1 ? mh / 2 : 1;
    }

    barrier.subresourceRange.baseMipLevel = levels - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static TextureImage createTextureImage(const unsigned char* pixels, int w, int h) {
    auto& ctx = graphics::internal::context;
    TextureImage tex;
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

    const VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    if (createBufferChecked(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            staging, stagingAlloc, "texture staging") != VK_SUCCESS) return tex;
    writeBuffer(stagingAlloc, pixels, static_cast<size_t>(size));

    VkImageCreateInfo ii = {};
    ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType     = VK_IMAGE_TYPE_2D;
    ii.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent        = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    ii.mipLevels     = tex.mipLevels;
    ii.arrayLayers   = 1;
    ii.samples       = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ii.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (createImageChecked(&ii, tex.image, tex.alloc, "texture image") != VK_SUCCESS) {
        vmaDestroyBuffer(ctx.allocator, staging, stagingAlloc);
        return tex;
    }

    VkCommandBuffer cmd = graphics::internal::beginSingleTimeCommands();
    transitionImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex.mipLevels);
    VkBufferImageCopy region = {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent      = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    generateMipmaps(cmd, tex.image, w, h, tex.mipLevels);
    graphics::internal::endSingleTimeCommands(cmd);
    vmaDestroyBuffer(ctx.allocator, staging, stagingAlloc);

    VkImageViewCreateInfo vi = {};
    vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image            = tex.image;
    vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vi.format           = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, tex.mipLevels, 0, 1 };
    if (vkCreateImageView(ctx.device, &vi, nullptr, &tex.view) != VK_SUCCESS) {
        std::cerr << "[app] texture image view failed\n";
    }
    return tex;
}

static void destroyTexture(TextureImage& t) {
    auto& ctx = graphics::internal::context;
    if (t.view)  vkDestroyImageView(ctx.device, t.view, nullptr);
    if (t.image) vmaDestroyImage(ctx.allocator, t.image, t.alloc);
    t = {};
}

static bool loadTextureFromFile(const std::string& path, TextureImage& out) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        std::cerr << "[app] failed to load texture: " << path << "\n";
        return false;
    }
    out = createTextureImage(pixels, w, h);
    stbi_image_free(pixels);
    return out.image != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Процедурные текстуры
// ---------------------------------------------------------------------------
static float hash2(int x, int y) {
    uint32_t n = static_cast<uint32_t>(x * 374761393 + y * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<float>((n ^ (n >> 16)) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

static float valueNoise(float x, float y) {
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float xf = x - static_cast<float>(xi), yf = y - static_cast<float>(yi);
    const float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    const float a00 = hash2(xi, yi),     a10 = hash2(xi + 1, yi);
    const float a01 = hash2(xi, yi + 1), a11 = hash2(xi + 1, yi + 1);
    return a00 * (1 - u) * (1 - v) + a10 * u * (1 - v) + a01 * (1 - u) * v + a11 * u * v;
}

static float fbm(float x, float y) {
    float s = 0.0f, amp = 0.5f, f = 1.0f;
    for (int i = 0; i < 4; ++i) { s += amp * valueNoise(x * f, y * f); f *= 2.0f; amp *= 0.5f; }
    return s;
}

static void generateDefaultTextures() {
    const int S = 256;
    auto writeIfMissing = [&](const char* name, auto pixelFn) {
        fs::path p = fs::path("textures") / name;
        if (fs::exists(p)) return;
        std::vector<unsigned char> px(static_cast<size_t>(S) * S * 4);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                const glm::vec3 col = glm::clamp(pixelFn(x, y), 0.0f, 1.0f);
                const size_t i = (static_cast<size_t>(y) * S + x) * 4;
                px[i + 0] = static_cast<unsigned char>(col.r * 255);
                px[i + 1] = static_cast<unsigned char>(col.g * 255);
                px[i + 2] = static_cast<unsigned char>(col.b * 255);
                px[i + 3] = 255;
            }
        }
        stbi_write_png(p.string().c_str(), S, S, 4, px.data(), S * 4);
        std::cout << "[app] generated texture: " << p.string() << "\n";
    };

    auto isFrame = [&](int x, int y) { return x < 18 || y < 18 || x >= S - 18 || y >= S - 18; };

    writeIfMissing("crate.png", [&](int x, int y) {
        const float n = fbm(x * 0.08f, y * 0.08f);
        if (isFrame(x, y)) return glm::vec3(0.42f + 0.15f * n);
        const glm::vec3 wood(0.55f + 0.20f * n, 0.36f + 0.14f * n, 0.18f + 0.08f * n);
        return (y % 64) < 3 ? wood * 0.45f : wood;
    });
    writeIfMissing("crate_spec.png", [&](int x, int y) {
        const float n = fbm(x * 0.08f, y * 0.08f);
        return glm::vec3(isFrame(x, y) ? 0.85f : 0.05f + 0.05f * n);
    });
    writeIfMissing("crate_emissive.png", [&](int x, int y) {
        const float g = fbm(x * 0.035f + 13.7f, y * 0.035f + 5.2f);
        const float k = glm::clamp((g - 0.58f) / 0.25f, 0.0f, 1.0f);
        return glm::vec3(0.05f, 0.9f, 0.75f) * (k * k);
    });
    writeIfMissing("bricks.png", [&](int x, int y) {
        const int row = y / 32;
        const int bx = (x + (row % 2) * 32) % 64;
        const int by = y % 32;
        const float n = fbm(x * 0.1f, y * 0.1f);
        if (by < 4 || bx < 4) return glm::vec3(0.55f + 0.1f * n);
        return glm::vec3(0.62f + 0.15f * n, 0.26f + 0.08f * n, 0.2f + 0.06f * n);
    });
    writeIfMissing("checker.png", [&](int x, int y) {
        const bool dark = ((x / 32) + (y / 32)) % 2;
        return dark ? glm::vec3(0.22f, 0.22f, 0.26f) : glm::vec3(0.9f, 0.88f, 0.82f);
    });
    writeIfMissing("marble.png", [&](int x, int y) {
        const float t = 0.5f + 0.5f * std::sin(x * 0.045f + fbm(x * 0.02f, y * 0.02f) * 7.0f);
        return glm::mix(glm::vec3(0.25f, 0.27f, 0.3f), glm::vec3(0.92f), t);
    });
}

static bool loadMaterials() {
    fs::create_directories("textures");
    generateDefaultTextures();
    stbi_set_flip_vertically_on_load(1);

    const unsigned char black[4] = { 0, 0, 0, 255 };
    blackTexture = createTextureImage(black, 1, 1);
    if (!blackTexture.image) {
        std::cerr << "[app] failed to create black texture\n";
        return false;
    }

    auto& ctx = graphics::internal::context;
    VkSamplerCreateInfo si = {};
    si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter    = VK_FILTER_LINEAR;
    si.minFilter    = VK_FILTER_LINEAR;
    si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod       = VK_LOD_CLAMP_NONE;
    si.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    if (ctx.sampler_anisotropy) {
        si.anisotropyEnable = VK_TRUE;
        si.maxAnisotropy    = ctx.max_anisotropy;
    }
    if (vkCreateSampler(ctx.device, &si, nullptr, &textureSampler) != VK_SUCCESS) {
        std::cerr << "[app] failed to create texture sampler\n";
        return false;
    }

    auto endsWith = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };

    std::vector<fs::path> albedoFiles;
    for (const auto& e : fs::directory_iterator("textures")) {
        if (!e.is_regular_file()) continue;
        const std::string stem = e.path().stem().string();
        if (endsWith(stem, "_spec") || endsWith(stem, "_emissive")) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
            albedoFiles.push_back(e.path());
        }
    }
    std::sort(albedoFiles.begin(), albedoFiles.end());

    auto findCompanion = [](const fs::path& albedo, const std::string& suffix) -> fs::path {
        fs::path same = albedo.parent_path() / (albedo.stem().string() + suffix + albedo.extension().string());
        if (fs::exists(same)) return same;
        fs::path png = albedo.parent_path() / (albedo.stem().string() + suffix + ".png");
        if (fs::exists(png)) return png;
        return {};
    };

    for (const auto& path : albedoFiles) {
        Material m;
        m.name = path.stem().string();
        if (!loadTextureFromFile(path.string(), m.albedo)) continue;

        const fs::path specPath = findCompanion(path, "_spec");
        if (specPath.empty() || !loadTextureFromFile(specPath.string(), m.specular)) m.specular = blackTexture;

        const fs::path emPath = findCompanion(path, "_emissive");
        if (emPath.empty() || !loadTextureFromFile(emPath.string(), m.emissive)) m.emissive = blackTexture;

        materials.push_back(m);
    }

    if (materials.empty()) {
        Material m;
        m.name = "white";
        const unsigned char white[4] = { 255, 255, 255, 255 };
        m.albedo   = createTextureImage(white, 1, 1);
        m.specular = blackTexture;
        m.emissive = blackTexture;
        materials.push_back(m);
    }

    std::cout << "[app] loaded " << materials.size() << " materials\n";
    return true;
}

static VkFormat selectShadowDepthFormat(VkPhysicalDevice pd) {
    const VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    for (VkFormat f : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(pd, f, &props);
        const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((props.optimalTilingFeatures & need) == need) return f;
    }
    return VK_FORMAT_UNDEFINED;
}

static bool allocateSet(VkDescriptorSetLayout layout, VkDescriptorSet& out, const char* name) {
    const VkDescriptorSetAllocateInfo ai = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout,
    };
    const VkResult r = vkAllocateDescriptorSets(graphics::internal::context.device, &ai, &out);
    if (r != VK_SUCCESS) std::cerr << "[app] vkAllocateDescriptorSets failed for " << name << ": " << r << "\n";
    return r == VK_SUCCESS;
}

// ---------------------------------------------------------------------------
// initialize
// ---------------------------------------------------------------------------
bool initialize() {
    auto& ctx    = graphics::internal::context;
    auto& device = ctx.device;

    state.pointLights.push_back({ {0.0f, 2.0f, 0.0f}, {1.0f, 0.8f, 0.6f}, 1.5f, 1.0f, 0.09f, 0.032f });
    state.spotLights.push_back({ {2.5f, 3.5f, -1.5f}, {-0.6f, -1.0f, 0.4f}, {0.4f, 0.7f, 1.0f}, 3.0f,
                                 glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(20.0f)), 1.0f, 0.09f, 0.032f });

    if (!uploadBufferToGPU(vertices.data(), sizeof(Vertex) * vertices.size(),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferAlloc, "vertex")) return false;
    if (!uploadBufferToGPU(indices.data(), sizeof(uint16_t) * indices.size(),
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferAlloc, "index")) return false;

    if (createBufferChecked(sizeof(SceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            sceneUboBuffer, sceneUboBufferAlloc, "sceneUbo") != VK_SUCCESS) return false;
    if (createBufferChecked(sizeof(PointLightData) * MAX_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            VMA_MEMORY_USAGE_CPU_TO_GPU, pointLightBuffer, pointLightBufferAlloc, "pointLight") != VK_SUCCESS) return false;
    if (createBufferChecked(sizeof(SpotLightData) * MAX_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            VMA_MEMORY_USAGE_CPU_TO_GPU, spotLightBuffer, spotLightBufferAlloc, "spotLight") != VK_SUCCESS) return false;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx.physical_device, &props);
    const size_t minAlign = static_cast<size_t>(props.limits.minUniformBufferOffsetAlignment);
    modelUboAlignedSize = (sizeof(ModelUBO) + minAlign - 1) & ~(minAlign - 1);

    if (createBufferChecked(modelUboAlignedSize * MAX_OBJECTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VMA_MEMORY_USAGE_CPU_TO_GPU, modelUboBuffer, modelUboBufferAlloc, "modelUbo") != VK_SUCCESS) return false;
    if (createBufferChecked(sizeof(LightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            lightUboBuffer, lightUboBufferAlloc, "lightUbo") != VK_SUCCESS) return false;
    if (createBufferChecked(sizeof(LightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            spotLightUboBuffer, spotLightUboBufferAlloc, "spotLightUbo") != VK_SUCCESS) return false;

    for (int i = 0; i < VISUALIZER_COUNT; ++i) {
        if (createBufferChecked(sizeof(VisualizerUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                                visualizerUboBuffers[i], visualizerUboBufferAllocs[i], "visualizerUbo") != VK_SUCCESS) return false;
    }

    if (createBufferChecked(sizeof(LineVertex) * LINE_VERTEX_CAPACITY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VMA_MEMORY_USAGE_CPU_TO_GPU, lineVertexBuffer, lineVertexBufferAlloc, "lineVertex") != VK_SUCCESS) return false;
    if (createBufferChecked(sizeof(LineUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            lineUboBuffer, lineUboBufferAlloc, "lineUbo") != VK_SUCCESS) return false;

    // === Shadow maps ===
    shadowDepthFormat = selectShadowDepthFormat(ctx.physical_device);
    if (shadowDepthFormat == VK_FORMAT_UNDEFINED) {
        std::cerr << "[app] no depth format suitable for shadow maps\n";
        return false;
    }

    VkImageCreateInfo shadowImageCI = {};
    shadowImageCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    shadowImageCI.imageType     = VK_IMAGE_TYPE_2D;
    shadowImageCI.format        = shadowDepthFormat;
    shadowImageCI.extent        = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1 };
    shadowImageCI.mipLevels     = 1;
    shadowImageCI.arrayLayers   = 1;
    shadowImageCI.samples       = VK_SAMPLE_COUNT_1_BIT;
    shadowImageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    shadowImageCI.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    shadowImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (createImageChecked(&shadowImageCI, shadowMapImage, shadowMapAlloc, "shadowMap") != VK_SUCCESS) return false;
    if (createImageChecked(&shadowImageCI, spotShadowMapImage, spotShadowMapAlloc, "spotShadowMap") != VK_SUCCESS) return false;

    VkImageViewCreateInfo shadowViewInfo = {};
    shadowViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    shadowViewInfo.image                           = shadowMapImage;
    shadowViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    shadowViewInfo.format                          = shadowDepthFormat;
    shadowViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    shadowViewInfo.subresourceRange.levelCount     = 1;
    shadowViewInfo.subresourceRange.layerCount     = 1;
    if (vkCreateImageView(device, &shadowViewInfo, nullptr, &shadowMapView) != VK_SUCCESS) {
        std::cerr << "[app] vkCreateImageView(shadowMap) failed\n";
        return false;
    }
    shadowViewInfo.image = spotShadowMapImage;
    if (vkCreateImageView(device, &shadowViewInfo, nullptr, &spotShadowMapView) != VK_SUCCESS) {
        std::cerr << "[app] vkCreateImageView(spotShadowMap) failed\n";
        return false;
    }

    VkSamplerCreateInfo shadowSamplerInfo = {};
    shadowSamplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    shadowSamplerInfo.magFilter     = VK_FILTER_LINEAR;
    shadowSamplerInfo.minFilter     = VK_FILTER_LINEAR;
    shadowSamplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    shadowSamplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    shadowSamplerInfo.compareEnable = VK_TRUE;
    shadowSamplerInfo.compareOp     = VK_COMPARE_OP_LESS;
    shadowSamplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowSamplerInfo.maxLod        = 1.0f;
    if (vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowMapSampler) != VK_SUCCESS) {
        std::cerr << "[app] vkCreateSampler(shadow) failed\n";
        return false;
    }

    {
        VkCommandBuffer cmd = graphics::internal::beginSingleTimeCommands();
        VkImageMemoryBarrier barrier = {};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask       = 0;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;

        barrier.image = shadowMapImage;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        barrier.image = spotShadowMapImage;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        graphics::internal::endSingleTimeCommands(cmd);

        shadowMapLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        spotShadowMapLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if (!loadMaterials()) return false;

    // === Descriptor set layouts ===
    const VkDescriptorSetLayoutBinding sceneBindings[] = {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    };
    const VkDescriptorSetLayoutCreateInfo sceneLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 5, .pBindings = sceneBindings
    };
    if (vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr, &sceneDescriptorSetLayout) != VK_SUCCESS) return false;

    const VkDescriptorSetLayoutBinding modelBindings[] = {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    };
    const VkDescriptorSetLayoutCreateInfo modelLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 4, .pBindings = modelBindings
    };
    if (vkCreateDescriptorSetLayout(device, &modelLayoutInfo, nullptr, &modelDescriptorSetLayout) != VK_SUCCESS) return false;

    const VkDescriptorSetLayoutBinding uboBinding = {
        0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
    };
    const VkDescriptorSetLayoutCreateInfo uboLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &uboBinding
    };
    if (vkCreateDescriptorSetLayout(device, &uboLayoutInfo, nullptr, &visualizerDescriptorSetLayout) != VK_SUCCESS) return false;
    if (vkCreateDescriptorSetLayout(device, &uboLayoutInfo, nullptr, &lineDescriptorSetLayout) != VK_SUCCESS) return false;

    const VkDescriptorSetLayoutBinding shadowBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
    const VkDescriptorSetLayoutCreateInfo shadowLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &shadowBinding
    };
    if (vkCreateDescriptorSetLayout(device, &shadowLayoutInfo, nullptr, &shadowDescriptorSetLayout) != VK_SUCCESS) return false;

    // === Pipeline layouts ===
    const VkPushConstantRange pcRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };

    const VkDescriptorSetLayout mainLayouts[]   = { sceneDescriptorSetLayout, modelDescriptorSetLayout };
    const VkPipelineLayoutCreateInfo mainLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2, .pSetLayouts = mainLayouts,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pcRange
    };
    if (vkCreatePipelineLayout(device, &mainLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) return false;

    const VkDescriptorSetLayout shadowLayouts[] = { shadowDescriptorSetLayout, modelDescriptorSetLayout };
    const VkPipelineLayoutCreateInfo shadowLayoutInfo2 = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 2, .pSetLayouts = shadowLayouts
    };
    if (vkCreatePipelineLayout(device, &shadowLayoutInfo2, nullptr, &shadowPipelineLayout) != VK_SUCCESS) return false;

    const VkPipelineLayoutCreateInfo visLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &visualizerDescriptorSetLayout
    };
    if (vkCreatePipelineLayout(device, &visLayoutInfo, nullptr, &visualizerPipelineLayout) != VK_SUCCESS) return false;

    const VkPipelineLayoutCreateInfo lineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &lineDescriptorSetLayout
    };
    if (vkCreatePipelineLayout(device, &lineLayoutInfo, nullptr, &linePipelineLayout) != VK_SUCCESS) return false;

    // === Shader modules ===
    const auto vertCode      = readFile("shaders/shader.vert.spv");
    const auto fragCode      = readFile("shaders/shader.frag.spv");
    const auto visVertCode   = readFile("shaders/visualize.vert.spv");
    const auto visFragCode   = readFile("shaders/visualize.frag.spv");
    const auto lineVertCode  = readFile("shaders/line.vert.spv");
    const auto shadowVertCode = readFile("shaders/shadow.vert.spv");

    auto makeModule = [&](const std::vector<char>& code, VkShaderModule& out, const char* name) -> bool {
        const VkShaderModuleCreateInfo ci = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size(),
            .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
        };
        if (vkCreateShaderModule(device, &ci, nullptr, &out) != VK_SUCCESS) {
            std::cerr << "[app] vkCreateShaderModule failed: " << name << "\n";
            return false;
        }
        return true;
    };

    VkShaderModule vertModule = VK_NULL_HANDLE, fragModule = VK_NULL_HANDLE;
    VkShaderModule visVertModule = VK_NULL_HANDLE, visFragModule = VK_NULL_HANDLE;
    VkShaderModule lineVertModule = VK_NULL_HANDLE, shadowVertModule = VK_NULL_HANDLE;

    if (!makeModule(vertCode, vertModule, "shader.vert")) return false;
    if (!makeModule(fragCode, fragModule, "shader.frag")) return false;
    if (!makeModule(visVertCode, visVertModule, "visualize.vert")) return false;
    if (!makeModule(visFragCode, visFragModule, "visualize.frag")) return false;
    if (!makeModule(lineVertCode, lineVertModule, "line.vert")) return false;
    if (!makeModule(shadowVertCode, shadowVertModule, "shadow.vert")) return false;

    // === Состояния пайплайнов ===
    const VkVertexInputBindingDescription bindingDesc = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    const VkVertexInputAttributeDescription attrDesc[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) },
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv) },
    };
    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attrDesc
    };

    const VkVertexInputBindingDescription lineBindingDesc = { 0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    const VkVertexInputAttributeDescription lineAttrDesc[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(LineVertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LineVertex, color) },
    };
    const VkPipelineVertexInputStateCreateInfo lineVertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &lineBindingDesc,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = lineAttrDesc
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };
    const VkPipelineInputAssemblyStateCreateInfo lineInputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    };
    const VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };
    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS
    };
    const VkPipelineDepthStencilStateCreateInfo gizmoDepthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_FALSE, .depthCompareOp = VK_COMPARE_OP_LESS
    };
    const VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    const VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &colorBlendAttachment
    };
    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS
    };
    const VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3, .pDynamicStates = dynamicStates
    };

    // --- Основной пайплайн ---
    const VkPipelineShaderStageCreateInfo mainStages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertModule, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragModule, .pName = "main" },
    };
    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = mainStages,
        .pVertexInputState = &vertexInputInfo, .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending, .pDynamicState = &dynamicState,
        .layout = pipelineLayout, .renderPass = ctx.render_pass, .subpass = 0,
    };
    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
    if (r != VK_SUCCESS) { std::cerr << "[app] graphicsPipeline failed: " << r << "\n"; return false; }

    // --- Shadow pipeline (dynamic rendering, depth only) ---
    const VkPipelineShaderStageCreateInfo shadowStages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = shadowVertModule, .pName = "main" },
    };
    const VkPipelineRasterizationStateCreateInfo shadowRasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_FRONT_BIT,          // борьба с shadow acne
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_TRUE,
        .lineWidth = 1.0f
    };
    const VkPipelineColorBlendStateCreateInfo shadowColorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 0, .pAttachments = nullptr
    };
    const VkPipelineRenderingCreateInfo shadowRenderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 0,
        .pColorAttachmentFormats = nullptr,
        .depthAttachmentFormat = shadowDepthFormat,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };
    const VkGraphicsPipelineCreateInfo shadowPipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &shadowRenderingInfo,
        .stageCount = 1, .pStages = shadowStages,
        .pVertexInputState = &vertexInputInfo, .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState, .pRasterizationState = &shadowRasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &depthStencil,
        .pColorBlendState = &shadowColorBlending, .pDynamicState = &dynamicState,
        .layout = shadowPipelineLayout, .renderPass = VK_NULL_HANDLE,
    };
    r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &shadowPipelineInfo, nullptr, &shadowPipeline);
    if (r != VK_SUCCESS) { std::cerr << "[app] shadowPipeline failed: " << r << "\n"; return false; }

    // --- Визуализаторы источников (кубики) ---
    const VkPipelineShaderStageCreateInfo visStages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = visVertModule, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = visFragModule, .pName = "main" },
    };
    const VkGraphicsPipelineCreateInfo visPipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = visStages,
        .pVertexInputState = &vertexInputInfo, .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &gizmoDepthStencil,
        .pColorBlendState = &colorBlending, .pDynamicState = &dynamicState,
        .layout = visualizerPipelineLayout, .renderPass = ctx.render_pass, .subpass = 0,
    };
    r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &visPipelineInfo, nullptr, &visualizerPipeline);
    if (r != VK_SUCCESS) { std::cerr << "[app] visualizerPipeline failed: " << r << "\n"; return false; }

    // --- Линии конусов прожекторов (свой вершинный шейдер с цветом) ---
    const VkPipelineShaderStageCreateInfo lineStages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = lineVertModule, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = visFragModule, .pName = "main" },
    };
    const VkGraphicsPipelineCreateInfo linePipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = lineStages,
        .pVertexInputState = &lineVertexInputInfo, .pInputAssemblyState = &lineInputAssembly,
        .pViewportState = &viewportState, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &gizmoDepthStencil,
        .pColorBlendState = &colorBlending, .pDynamicState = &dynamicState,
        .layout = linePipelineLayout, .renderPass = ctx.render_pass, .subpass = 0,
    };
    r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &linePipelineInfo, nullptr, &linePipeline);
    if (r != VK_SUCCESS) { std::cerr << "[app] linePipeline failed: " << r << "\n"; return false; }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, visVertModule, nullptr);
    vkDestroyShaderModule(device, visFragModule, nullptr);
    vkDestroyShaderModule(device, lineVertModule, nullptr);
    vkDestroyShaderModule(device, shadowVertModule, nullptr);

    // === Descriptor pool ===
    const uint32_t matCount = static_cast<uint32_t>(materials.size());
    const VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1u + VISUALIZER_COUNT + 1u + 2u }, // scene + vis + line + 2 shadow
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, matCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, matCount * 3 + 2 },
    };
    const VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = 1u + matCount + VISUALIZER_COUNT + 1u + 2u,
        .poolSizeCount = 4,
        .pPoolSizes    = poolSizes,
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "[app] vkCreateDescriptorPool failed\n";
        return false;
    }

    if (!allocateSet(sceneDescriptorSetLayout, sceneDescriptorSet, "scene")) return false;
    for (auto& m : materials) if (!allocateSet(modelDescriptorSetLayout, m.descriptorSet, m.name.c_str())) return false;
    for (int i = 0; i < VISUALIZER_COUNT; ++i)
        if (!allocateSet(visualizerDescriptorSetLayout, visualizerDescriptorSets[i], "visualizer")) return false;
    if (!allocateSet(lineDescriptorSetLayout, lineDescriptorSet, "line")) return false;
    if (!allocateSet(shadowDescriptorSetLayout, shadowDescriptorSet, "shadowDir")) return false;
    if (!allocateSet(shadowDescriptorSetLayout, spotShadowDescriptorSet, "shadowSpot")) return false;

    // === Запись дескрипторов ===
    VkDescriptorBufferInfo sceneInfo        = { sceneUboBuffer,     0, sizeof(SceneUBO) };
    VkDescriptorBufferInfo pointLightInfo   = { pointLightBuffer,   0, sizeof(PointLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo spotLightInfo    = { spotLightBuffer,    0, sizeof(SpotLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo lineInfo         = { lineUboBuffer,      0, sizeof(LineUBO) };
    VkDescriptorBufferInfo modelInfo        = { modelUboBuffer,     0, sizeof(ModelUBO) };
    VkDescriptorBufferInfo lightInfo        = { lightUboBuffer,     0, sizeof(LightUBO) };
    VkDescriptorBufferInfo spotLightUboInfo = { spotLightUboBuffer, 0, sizeof(LightUBO) };

    std::vector<VkWriteDescriptorSet> writes;
    auto addBufferWrite = [&](VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo* info) {
        VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        w.dstSet = set; w.dstBinding = binding; w.descriptorCount = 1;
        w.descriptorType = type; w.pBufferInfo = info;
        writes.push_back(w);
    };

    addBufferWrite(sceneDescriptorSet,      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &sceneInfo);
    addBufferWrite(sceneDescriptorSet,      1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pointLightInfo);
    addBufferWrite(sceneDescriptorSet,      2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &spotLightInfo);
    addBufferWrite(lineDescriptorSet,       0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lineInfo);
    addBufferWrite(shadowDescriptorSet,     0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightInfo);
    addBufferWrite(spotShadowDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &spotLightUboInfo);

    std::vector<VkDescriptorBufferInfo> visBufferInfos(VISUALIZER_COUNT);
    for (int i = 0; i < VISUALIZER_COUNT; ++i) {
        visBufferInfos[i] = { visualizerUboBuffers[i], 0, sizeof(VisualizerUBO) };
        addBufferWrite(visualizerDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &visBufferInfos[i]);
    }

    std::vector<VkDescriptorImageInfo> imageInfos(matCount * 3);
    for (uint32_t i = 0; i < matCount; ++i) {
        auto& m = materials[i];
        addBufferWrite(m.descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &modelInfo);
        imageInfos[i * 3 + 0] = { textureSampler, m.albedo.view,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[i * 3 + 1] = { textureSampler, m.specular.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[i * 3 + 2] = { textureSampler, m.emissive.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        for (uint32_t t = 0; t < 3; ++t) {
            VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            w.dstSet = m.descriptorSet; w.dstBinding = 1 + t; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &imageInfos[i * 3 + t];
            writes.push_back(w);
        }
    }

    const VkDescriptorImageInfo shadowMapDesc     = { shadowMapSampler, shadowMapView,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    const VkDescriptorImageInfo spotShadowMapDesc = { shadowMapSampler, spotShadowMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet shadowWrite = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    shadowWrite.dstSet = sceneDescriptorSet; shadowWrite.dstBinding = 3; shadowWrite.descriptorCount = 1;
    shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; shadowWrite.pImageInfo = &shadowMapDesc;
    writes.push_back(shadowWrite);
    VkWriteDescriptorSet spotShadowWrite = shadowWrite;
    spotShadowWrite.dstBinding = 4; spotShadowWrite.pImageInfo = &spotShadowMapDesc;
    writes.push_back(spotShadowWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // === Сцена ===
    auto matIndex = [&](const char* name) -> int {
        for (size_t i = 0; i < materials.size(); ++i)
            if (materials[i].name == name) return static_cast<int>(i);
        return 0;
    };

    ObjectState o1;
    o1.position      = { -1.45f, 0.0f, 0.0f };
    o1.rotation      = { 20.0f, 0.0f, 0.0f };
    o1.scale         = { 0.65f, 0.65f, 0.65f };
    o1.materialIndex = matIndex("crate");
    o1.shininess     = 48.0f;

    ObjectState o2;
    o2.position      = { 1.45f, 0.0f, 0.0f };
    o2.rotation      = { -22.0f, 0.0f, 0.0f };
    o2.scale         = { 0.65f, 0.65f, 0.65f };
    o2.materialIndex = matIndex("marble");
    o2.useTrajectory = true;
    o2.fancySampling = true;

    ObjectState floorObj;
    floorObj.position      = { 0.0f, -0.95f, 0.0f };
    floorObj.scale         = { 4.5f, 0.05f, 4.5f };
    floorObj.materialIndex = matIndex("bricks");
    floorObj.uvScale       = 4.0f;
    floorObj.shininess     = 8.0f;
    floorObj.specularTint  = { 0.3f, 0.3f, 0.3f };

    state.objects = { o1, o2, floorObj };

    std::cout << "[app] initialize done, materials=" << materials.size()
              << ", objects=" << state.objects.size()
              << ", modelUboStride=" << modelUboAlignedSize << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------
void shutdown() {
    auto& ctx = graphics::internal::context;
    if (ctx.graphics_queue) vkQueueWaitIdle(ctx.graphics_queue);

    if (descriptorPool) vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);

    if (graphicsPipeline)   vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
    if (visualizerPipeline) vkDestroyPipeline(ctx.device, visualizerPipeline, nullptr);
    if (linePipeline)       vkDestroyPipeline(ctx.device, linePipeline, nullptr);
    if (shadowPipeline)     vkDestroyPipeline(ctx.device, shadowPipeline, nullptr);

    if (pipelineLayout)           vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    if (visualizerPipelineLayout) vkDestroyPipelineLayout(ctx.device, visualizerPipelineLayout, nullptr);
    if (linePipelineLayout)       vkDestroyPipelineLayout(ctx.device, linePipelineLayout, nullptr);
    if (shadowPipelineLayout)     vkDestroyPipelineLayout(ctx.device, shadowPipelineLayout, nullptr);

    if (sceneDescriptorSetLayout)      vkDestroyDescriptorSetLayout(ctx.device, sceneDescriptorSetLayout, nullptr);
    if (modelDescriptorSetLayout)      vkDestroyDescriptorSetLayout(ctx.device, modelDescriptorSetLayout, nullptr);
    if (visualizerDescriptorSetLayout) vkDestroyDescriptorSetLayout(ctx.device, visualizerDescriptorSetLayout, nullptr);
    if (lineDescriptorSetLayout)       vkDestroyDescriptorSetLayout(ctx.device, lineDescriptorSetLayout, nullptr);
    if (shadowDescriptorSetLayout)     vkDestroyDescriptorSetLayout(ctx.device, shadowDescriptorSetLayout, nullptr);

    if (textureSampler)   vkDestroySampler(ctx.device, textureSampler, nullptr);
    if (shadowMapSampler) vkDestroySampler(ctx.device, shadowMapSampler, nullptr);

    for (auto& m : materials) {
        destroyTexture(m.albedo);
        if (m.specular.image != blackTexture.image) destroyTexture(m.specular);
        if (m.emissive.image != blackTexture.image) destroyTexture(m.emissive);
    }
    materials.clear();
    destroyTexture(blackTexture);

    if (shadowMapView)      vkDestroyImageView(ctx.device, shadowMapView, nullptr);
    if (shadowMapImage)     vmaDestroyImage(ctx.allocator, shadowMapImage, shadowMapAlloc);
    if (spotShadowMapView)  vkDestroyImageView(ctx.device, spotShadowMapView, nullptr);
    if (spotShadowMapImage) vmaDestroyImage(ctx.allocator, spotShadowMapImage, spotShadowMapAlloc);

    auto destroyBuf = [&](VkBuffer& buf, VmaAllocation& alloc) {
        if (buf) vmaDestroyBuffer(ctx.allocator, buf, alloc);
        buf = VK_NULL_HANDLE; alloc = VK_NULL_HANDLE;
    };
    destroyBuf(vertexBuffer, vertexBufferAlloc);
    destroyBuf(indexBuffer, indexBufferAlloc);
    destroyBuf(sceneUboBuffer, sceneUboBufferAlloc);
    destroyBuf(pointLightBuffer, pointLightBufferAlloc);
    destroyBuf(spotLightBuffer, spotLightBufferAlloc);
    destroyBuf(modelUboBuffer, modelUboBufferAlloc);
    destroyBuf(lightUboBuffer, lightUboBufferAlloc);
    destroyBuf(spotLightUboBuffer, spotLightUboBufferAlloc);
    destroyBuf(lineVertexBuffer, lineVertexBufferAlloc);
    destroyBuf(lineUboBuffer, lineUboBufferAlloc);
    for (int i = 0; i < VISUALIZER_COUNT; ++i) destroyBuf(visualizerUboBuffers[i], visualizerUboBufferAllocs[i]);
}

// ---------------------------------------------------------------------------
// update (ImGui)
// ---------------------------------------------------------------------------
void update([[maybe_unused]] double time, double deltaTime) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 820), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lab 4 - Shadow Mapping");

    ImGui::Text("FPS: %.1f  (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Camera Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Camera Position", &state.cameraPos.x, 0.05f);
        ImGui::DragFloat3("Camera Target", &state.cameraTarget.x, 0.05f);
        const char* projItems[] = { "Perspective", "Orthographic" };
        ImGui::Combo("Projection", &state.projectionType, projItems, IM_ARRAYSIZE(projItems));
        if (state.projectionType == 0) ImGui::SliderFloat("Field of View", &state.fov, 10.0f, 120.0f);
    }

    if (ImGui::CollapsingHeader("Directional Light & Ambient")) {
        ImGui::DragFloat3("Dir Light Direction", &state.dirLightDir.x, 0.05f);
        ImGui::ColorEdit3("Dir Light Color", &state.dirLightColor.x);
        ImGui::SliderFloat("Dir Light Intensity", &state.dirLightIntensity, 0.0f, 5.0f);
        ImGui::ColorEdit3("Ambient Color", &state.ambientLight.x);
    }

    if (ImGui::CollapsingHeader("Shadow Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Directional Shadows", &state.enableDirShadows);
        ImGui::Checkbox("Enable Spot Shadows", &state.enableSpotShadows);
        ImGui::Checkbox("PCF (3x3)", &state.pcfEnabled);
        ImGui::SliderFloat("Ortho Size", &state.shadowOrthoSize, 1.0f, 20.0f);
        ImGui::SliderFloat("Depth Bias Constant", &state.depthBiasConstant, 0.0f, 5.0f);
        ImGui::SliderFloat("Depth Bias Slope", &state.depthBiasSlope, 0.0f, 5.0f);
        ImGui::TextDisabled("Shadow maps: %ux%u, depth-only pass", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    }

    if (ImGui::CollapsingHeader("Point Lights (Storage Buffer)")) {
        if (ImGui::Button("Add Point Light") && state.pointLights.size() < MAX_LIGHTS) {
            state.pointLights.push_back({ {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 1.0f, 0.09f, 0.032f });
        }
        if (state.pointLights.size() >= MAX_LIGHTS) ImGui::TextDisabled("Limit reached (%d)", MAX_LIGHTS);

        for (size_t i = 0; i < state.pointLights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i + 100));
            const std::string label = "Point Light " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                ImGui::DragFloat3("Position", &state.pointLights[i].position.x, 0.05f);
                ImGui::ColorEdit3("Color", &state.pointLights[i].color.x);
                ImGui::SliderFloat("Intensity", &state.pointLights[i].intensity, 0.0f, 10.0f);
                ImGui::DragFloat("Constant", &state.pointLights[i].constant, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Linear", &state.pointLights[i].linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &state.pointLights[i].quadratic, 0.001f, 0.0f, 1.0f);
                if (ImGui::Button("Remove")) {
                    state.pointLights.erase(state.pointLights.begin() + static_cast<long long>(i));
                    ImGui::TreePop(); ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Spot Lights (Storage Buffer)")) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "FIRST spot light casts shadows!");
        if (ImGui::Button("Add Spot Light") && state.spotLights.size() < MAX_LIGHTS) {
            state.spotLights.push_back({ {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 2.0f,
                                         glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(17.5f)), 1.0f, 0.09f, 0.032f });
        }
        if (state.spotLights.size() >= MAX_LIGHTS) ImGui::TextDisabled("Limit reached (%d)", MAX_LIGHTS);

        for (size_t i = 0; i < state.spotLights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i + 200));
            const std::string label = "Spot Light " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                ImGui::DragFloat3("Position", &state.spotLights[i].position.x, 0.05f);
                ImGui::DragFloat3("Direction", &state.spotLights[i].direction.x, 0.05f);
                ImGui::ColorEdit3("Color", &state.spotLights[i].color.x);
                ImGui::SliderFloat("Intensity", &state.spotLights[i].intensity, 0.0f, 10.0f);

                float innerDeg = glm::degrees(std::acos(std::clamp(state.spotLights[i].cutOff, -1.0f, 1.0f)));
                float outerDeg = glm::degrees(std::acos(std::clamp(state.spotLights[i].outerCutOff, -1.0f, 1.0f)));
                if (ImGui::SliderFloat("Inner Angle", &innerDeg, 1.0f, 45.0f))
                    state.spotLights[i].cutOff = glm::cos(glm::radians(innerDeg));
                if (ImGui::SliderFloat("Outer Angle", &outerDeg, innerDeg, 60.0f))
                    state.spotLights[i].outerCutOff = glm::cos(glm::radians(outerDeg));
                if (state.spotLights[i].outerCutOff > state.spotLights[i].cutOff)
                    state.spotLights[i].outerCutOff = state.spotLights[i].cutOff;

                ImGui::DragFloat("Constant", &state.spotLights[i].constant, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Linear", &state.spotLights[i].linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &state.spotLights[i].quadratic, 0.001f, 0.0f, 1.0f);
                if (ImGui::Button("Remove")) {
                    state.spotLights.erase(state.spotLights.begin() + static_cast<long long>(i));
                    ImGui::TreePop(); ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    std::vector<const char*> matNames;
    matNames.reserve(materials.size());
    for (const auto& m : materials) matNames.push_back(m.name.c_str());

    const size_t objectCount = std::min(state.objects.size(), static_cast<size_t>(MAX_OBJECTS));
    for (size_t i = 0; i < objectCount; ++i) {
        ObjectState& obj = state.objects[i];
        const std::string name = (i < 2) ? ("Parallelepiped " + std::to_string(i + 1))
                               : (i == 2 ? std::string("Floor") : ("Object " + std::to_string(i)));
        if (ImGui::CollapsingHeader(name.c_str(), i < 2 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Combo("Texture", &obj.materialIndex, matNames.data(), static_cast<int>(matNames.size()));
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &obj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f);
            ImGui::ColorEdit3("Albedo Tint", &obj.albedoTint.x);
            ImGui::ColorEdit3("Specular Tint", &obj.specularTint.x);
            ImGui::SliderFloat("Shininess", &obj.shininess, 1.0f, 128.0f);
            ImGui::SliderFloat("UV Scale", &obj.uvScale, 0.25f, 8.0f);
            ImGui::Checkbox("Trajectory", &obj.useTrajectory);
            ImGui::Checkbox("Fancy Sampling", &obj.fancySampling);
            ImGui::PopID();
        }
    }

    ImGui::Checkbox("Show Light Visualizers", &state.showLightVisualizers);
    if (ImGui::Button(state.isPaused ? "Resume" : "Pause")) state.isPaused = !state.isPaused;
    ImGui::SameLine();
    if (ImGui::Button("Reset Time")) state.animTime = 0.0f;

    ImGui::End();

    if (!state.isPaused) state.animTime += static_cast<float>(deltaTime) * state.animSpeed;
}

// ---------------------------------------------------------------------------
// Shadow pass (один и тот же код для directional и spot)
// ---------------------------------------------------------------------------
static void renderShadowPass(const FrameData& fd, VkDescriptorSet lightSet,
                             VkImage image, VkImageView view, VkImageLayout& layout) {
    VkCommandBuffer cmd = fd.command_buffer;

    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = layout;
    barrier.newLayout           = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask       = (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ? VK_ACCESS_SHADER_READ_BIT : 0;
    barrier.dstAccessMask       = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    const VkPipelineStageFlags srcStage = (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType                = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView            = view;
    depthAttachment.imageLayout          = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp               = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp              = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea           = { { 0, 0 }, { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE } };
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = 0;
    renderingInfo.pColorAttachments    = nullptr;
    renderingInfo.pDepthAttachment     = &depthAttachment;

    graphics::internal::cmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
    const VkViewport viewport = { 0.0f, 0.0f, static_cast<float>(SHADOW_MAP_SIZE), static_cast<float>(SHADOW_MAP_SIZE), 0.0f, 1.0f };
    const VkRect2D   scissor  = { { 0, 0 }, { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE } };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetDepthBias(cmd, state.depthBiasConstant, 0.0f, state.depthBiasSlope);

    const VkBuffer     vBuffers[] = { vertexBuffer };
    const VkDeviceSize offsets[]  = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    const size_t objectCount = std::min(state.objects.size(), static_cast<size_t>(MAX_OBJECTS));
    for (size_t i = 0; i < objectCount; ++i) {
        int mi = state.objects[i].materialIndex;
        if (mi < 0 || mi >= static_cast<int>(materials.size())) mi = 0;
        const uint32_t dynamicOffset = static_cast<uint32_t>(i * modelUboAlignedSize);
        const VkDescriptorSet sets[] = { lightSet, materials[mi].descriptorSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipelineLayout, 0, 2, sets, 1, &dynamicOffset);
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }

    graphics::internal::cmdEndRendering(cmd);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void render(const FrameData& fd) {
    if (!fd.valid || !fd.command_buffer) return;

    auto& ctx = graphics::internal::context;
    VkCommandBuffer cmd = fd.command_buffer;
    ++g_frameCounter;
    const bool log = (g_frameCounter <= 2);
    auto logStep = [&](const char* s) { if (log) std::cout << "[app] render: " << s << "\n"; };

    logStep("start");

    const size_t objectCount     = std::min(state.objects.size(),    static_cast<size_t>(MAX_OBJECTS));
    const size_t pointLightCount = std::min(state.pointLights.size(), static_cast<size_t>(MAX_LIGHTS));
    const size_t spotLightCount  = std::min(state.spotLights.size(),  static_cast<size_t>(MAX_LIGHTS));

    // --- Матрица направленного света ---
    const glm::vec3 lightDir    = glm::normalize(state.dirLightDir);
    const glm::vec3 lightTarget = state.cameraTarget;
    const glm::vec3 lightPos    = lightTarget - lightDir * 10.0f;
    const glm::mat4 lightView   = glm::lookAt(lightPos, lightTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const float     orthoSize   = state.shadowOrthoSize;
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 50.0f);
    lightProj[1][1] *= -1.0f;
    const glm::mat4 lightViewProj = lightProj * lightView;

    const LightUBO lightUbo = { lightViewProj };
    writeBuffer(lightUboBufferAlloc, &lightUbo, sizeof(LightUBO));
    logStep("lightUbo uploaded");

    // --- Матрица первого прожектора ---
    glm::mat4 spotLightViewProj(1.0f);
    bool spotShadowActive = false;
    if (spotLightCount > 0) {
        const auto& sl = state.spotLights[0];
        const glm::vec3 dir = glm::normalize(sl.direction);
        const glm::vec3 up  = (glm::abs(dir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        const glm::mat4 spotView = glm::lookAt(sl.position, sl.position + dir, up);
        const float clampedCut = std::clamp(sl.outerCutOff, -1.0f, 1.0f);
        float fov = 2.0f * std::acos(clampedCut) * 1.1f;
        fov = glm::min(fov, glm::radians(160.0f));
        glm::mat4 spotProj = glm::perspective(fov, 1.0f, 0.1f, 50.0f);
        spotProj[1][1] *= -1.0f;
        spotLightViewProj = spotProj * spotView;
        spotShadowActive  = true;
    }
    const LightUBO spotLightUbo = { spotLightViewProj };
    writeBuffer(spotLightUboBufferAlloc, &spotLightUbo, sizeof(LightUBO));
    logStep("spotLightUbo uploaded");

    // --- Scene UBO ---
    SceneUBO sceneUbo = {};
    sceneUbo.view = glm::lookAt(state.cameraPos, state.cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const float aspect = (ctx.swapchain_extent.height != 0)
                       ? static_cast<float>(ctx.swapchain_extent.width) / static_cast<float>(ctx.swapchain_extent.height)
                       : 1.0f;
    if (state.projectionType == 0) {
        sceneUbo.proj = glm::perspective(glm::radians(state.fov), aspect, 0.1f, 100.0f);
    } else {
        constexpr float size = 3.0f;
        sceneUbo.proj = glm::ortho(-size * aspect, size * aspect, -size, size, -100.0f, 100.0f);
    }
    sceneUbo.proj[1][1] *= -1.0f;
    sceneUbo.lightViewProj     = lightViewProj;
    sceneUbo.spotLightViewProj = spotLightViewProj;
    sceneUbo.cameraPos         = state.cameraPos;
    sceneUbo.dirLightDir       = lightDir;
    sceneUbo.dirLightColor     = state.dirLightColor;
    sceneUbo.ambientColor      = state.ambientLight;
    sceneUbo.dirLightIntensity = state.dirLightIntensity;
    sceneUbo.pointLightCount   = static_cast<int>(pointLightCount);
    sceneUbo.spotLightCount    = static_cast<int>(spotLightCount);
    sceneUbo.shadowEnabled     = state.enableDirShadows ? 1 : 0;
    sceneUbo.spotShadowEnabled = (spotShadowActive && state.enableSpotShadows) ? 1 : 0;
    writeBuffer(sceneUboBufferAlloc, &sceneUbo, sizeof(SceneUBO));
    logStep("sceneUbo uploaded");

    if (pointLightCount > 0)
        writeBuffer(pointLightBufferAlloc, state.pointLights.data(), sizeof(PointLightData) * pointLightCount);
    if (spotLightCount > 0)
        writeBuffer(spotLightBufferAlloc, state.spotLights.data(), sizeof(SpotLightData) * spotLightCount);
    logStep("light buffers uploaded");

    // --- Model UBO (динамические оффсеты) ---
    for (size_t i = 0; i < objectCount; ++i) {
        const ObjectState& obj = state.objects[i];
        glm::vec3 pos = obj.position;
        if (obj.useTrajectory) {
            pos.x += std::cos(state.animTime) * state.animRadius;
            pos.y += std::sin(state.animTime) * state.animHeight;
        }
        glm::mat4 model(1.0f);
        model = glm::translate(model, pos);
        model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, obj.scale);

        const ModelUBO modelUbo = { model, obj.albedoTint, obj.shininess, obj.specularTint, obj.uvScale };
        writeBuffer(modelUboBufferAlloc, &modelUbo, sizeof(ModelUBO), i * modelUboAlignedSize);
    }
    logStep("modelUbo uploaded");

    // === Shadow passes ===
    if (state.enableDirShadows && shadowPipeline && !materials.empty()) {
        logStep("begin dir shadow");
        renderShadowPass(fd, shadowDescriptorSet, shadowMapImage, shadowMapView, shadowMapLayout);
        logStep("end dir shadow");
    }
    if (spotShadowActive && state.enableSpotShadows && shadowPipeline && !materials.empty()) {
        logStep("begin spot shadow");
        renderShadowPass(fd, spotShadowDescriptorSet, spotShadowMapImage, spotShadowMapView, spotShadowMapLayout);
        logStep("end spot shadow");
    }

    // === Основной проход ===
    logStep("begin main render pass");
    graphics::internal::beginMainRenderPass(cmd);

    const VkViewport viewport = { 0.0f, 0.0f, static_cast<float>(ctx.swapchain_extent.width),
                                  static_cast<float>(ctx.swapchain_extent.height), 0.0f, 1.0f };
    const VkRect2D scissor = { { 0, 0 }, ctx.swapchain_extent };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetDepthBias(cmd, 0.0f, 0.0f, 0.0f);

    const VkBuffer     vBuffers[] = { vertexBuffer };
    const VkDeviceSize offsets[]  = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    for (size_t i = 0; i < objectCount; ++i) {
        const ObjectState& obj = state.objects[i];
        int mi = obj.materialIndex;
        if (mi < 0 || mi >= static_cast<int>(materials.size())) mi = 0;
        const uint32_t dynamicOffset = static_cast<uint32_t>(i * modelUboAlignedSize);
        const VkDescriptorSet sets[] = { sceneDescriptorSet, materials[mi].descriptorSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 1, &dynamicOffset);

        const PushConstants pc = { obj.fancySampling ? 1 : 0, state.animTime, state.pcfEnabled ? 1 : 0, 0 };
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }
    logStep("main scene drawn");

    // === Гизмо источников света ===
    if (state.showLightVisualizers) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, visualizerPipeline);

        int visIndex = 0;
        auto drawVisualizer = [&](const glm::vec3& pos, const glm::vec3& color) {
            if (visIndex >= VISUALIZER_COUNT) return;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, glm::vec3(0.05f));
            const VisualizerUBO visUbo = { sceneUbo.proj * sceneUbo.view * model, color };
            writeBuffer(visualizerUboBufferAllocs[visIndex], &visUbo, sizeof(VisualizerUBO));
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, visualizerPipelineLayout,
                                    0, 1, &visualizerDescriptorSets[visIndex], 0, nullptr);
            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            ++visIndex;
        };

        for (size_t i = 0; i < pointLightCount; ++i) drawVisualizer(state.pointLights[i].position, state.pointLights[i].color);
        for (size_t i = 0; i < spotLightCount;  ++i) drawVisualizer(state.spotLights[i].position,  state.spotLights[i].color);
        logStep("light cubes drawn");

        // --- Конусы прожекторов ---
        std::vector<LineVertex> lineVerts;
        for (size_t i = 0; i < spotLightCount; ++i) {
            const auto& sl = state.spotLights[i];
            const glm::vec3 dir    = glm::normalize(sl.direction);
            const glm::vec3 helper = (glm::abs(dir.z) < 0.9f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            const glm::vec3 u = glm::normalize(glm::cross(dir, helper));
            const glm::vec3 v = glm::cross(dir, u);
            constexpr float length = 2.0f;
            const glm::vec3 baseCenter = sl.position + dir * length;
            const glm::vec4 color(sl.color, 1.0f);

            constexpr int SEGMENTS = 16;
            auto ring = [&](float cutOff) {
                std::vector<glm::vec3> points;
                const float angle  = std::acos(std::clamp(cutOff, -1.0f, 1.0f));
                const float radius = length * std::tan(angle);
                for (int k = 0; k < SEGMENTS; ++k) {
                    const float ang = (static_cast<float>(k) * 2.0f * 3.14159265f) / SEGMENTS;
                    points.push_back(baseCenter + (std::cos(ang) * u + std::sin(ang) * v) * radius);
                }
                return points;
            };

            const auto outerPoints = ring(sl.outerCutOff);
            for (int k = 0; k < SEGMENTS; ++k) {
                lineVerts.push_back({ sl.position,   color });
                lineVerts.push_back({ outerPoints[k], color });
            }
            for (int k = 0; k < SEGMENTS; ++k) {
                lineVerts.push_back({ outerPoints[k], color });
                lineVerts.push_back({ outerPoints[(k + 1) % SEGMENTS], color });
            }
            const auto innerPoints = ring(sl.cutOff);
            for (int k = 0; k < SEGMENTS; ++k) {
                lineVerts.push_back({ innerPoints[k], color });
                lineVerts.push_back({ innerPoints[(k + 1) % SEGMENTS], color });
            }
        }

        if (!lineVerts.empty() && lineVerts.size() <= LINE_VERTEX_CAPACITY) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
            writeBuffer(lineVertexBufferAlloc, lineVerts.data(), sizeof(LineVertex) * lineVerts.size());

            const LineUBO lineUbo = { sceneUbo.proj * sceneUbo.view, glm::vec3(1.0f) };
            writeBuffer(lineUboBufferAlloc, &lineUbo, sizeof(LineUBO));

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipelineLayout,
                                    0, 1, &lineDescriptorSet, 0, nullptr);
            const VkBuffer     lvBuffers[] = { lineVertexBuffer };
            const VkDeviceSize lOffsets[]  = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, lvBuffers, lOffsets);
            vkCmdDraw(cmd, static_cast<uint32_t>(lineVerts.size()), 1, 0, 0);
            logStep("lines drawn");
        }
    }

    logStep("end");
}

} // namespace application