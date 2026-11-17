#include "application.hpp"
#include <imgui.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstddef>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace application {

namespace fs = std::filesystem;

constexpr float a = 0.8f;
constexpr float b = 0.6f;
constexpr float c = 0.5f;

const std::vector<Vertex> vertices = {
    {{-a, -b, -c}, {0, -1, 0}, {0, 0}}, {{ a, -b, -c}, {0, -1, 0}, {1, 0}},
    {{ a, -b,  c}, {0, -1, 0}, {1, 1}}, {{-a, -b,  c}, {0, -1, 0}, {0, 1}},
    {{-a,  b, -c}, {0, 1, 0}, {0, 0}}, {{ a,  b, -c}, {0, 1, 0}, {1, 0}},
    {{ a,  b,  c}, {0, 1, 0}, {1, 1}}, {{-a,  b,  c}, {0, 1, 0}, {0, 1}},
    {{-a, -b,  c}, {0, 0, 1}, {0, 0}}, {{ a, -b,  c}, {0, 0, 1}, {1, 0}},
    {{ a,  b,  c}, {0, 0, 1}, {1, 1}}, {{-a,  b,  c}, {0, 0, 1}, {0, 1}},
    {{-a, -b, -c}, {0, 0, -1}, {0, 0}}, {{ a, -b, -c}, {0, 0, -1}, {1, 0}},
    {{ a,  b, -c}, {0, 0, -1}, {1, 1}}, {{-a,  b, -c}, {0, 0, -1}, {0, 1}},
    {{-a, -b, -c}, {-1, 0, 0}, {0, 0}}, {{-a, -b,  c}, {-1, 0, 0}, {1, 0}},
    {{-a,  b,  c}, {-1, 0, 0}, {1, 1}}, {{-a,  b, -c}, {-1, 0, 0}, {0, 1}},
    {{ a, -b, -c}, {1, 0, 0}, {0, 0}}, {{ a, -b,  c}, {1, 0, 0}, {1, 0}},
    {{ a,  b,  c}, {1, 0, 0}, {1, 1}}, {{ a,  b, -c}, {1, 0, 0}, {0, 1}},
};

const std::vector<uint16_t> indices = {
    0, 2, 1,  0, 3, 2,
    4, 5, 6,  4, 6, 7,
    8, 10, 9,  8, 11, 10,
    12, 13, 14,  12, 14, 15,
    16, 18, 17,  16, 19, 18,
    20, 21, 22,  20, 22, 23
};

struct LineVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct LineUBO {
    alignas(16) glm::mat4 mvp;
    alignas(16) glm::vec3 color;
};

struct TextureImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;
};

struct Material {
    std::string name;
    TextureImage albedo;
    TextureImage specular;  
    TextureImage emissive;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

constexpr int MAX_LIGHTS = 10;
constexpr int MAX_OBJECTS = 8;

VkBuffer vertexBuffer;
VmaAllocation vertexBufferAlloc;
VkBuffer indexBuffer;
VmaAllocation indexBufferAlloc;


VkBuffer sceneUboBuffer;
VmaAllocation sceneUboBufferAlloc;
VkBuffer pointLightBuffer;
VmaAllocation pointLightBufferAlloc;
VkBuffer spotLightBuffer;
VmaAllocation spotLightBufferAlloc;


VkBuffer modelUboBuffer;
VmaAllocation modelUboBufferAlloc;
size_t modelUboAlignedSize = 0; 


VkBuffer visualizerUboBuffers[MAX_LIGHTS * 2];
VmaAllocation visualizerUboBufferAllocs[MAX_LIGHTS * 2];
VkDescriptorSet visualizerDescriptorSets[MAX_LIGHTS * 2];


VkBuffer lineVertexBuffer;
VmaAllocation lineVertexBufferAlloc;
VkBuffer lineUboBuffer;
VmaAllocation lineUboBufferAlloc;
VkDescriptorSet lineDescriptorSet;

VkSampler textureSampler;
TextureImage blackTexture; 
std::vector<Material> materials;

VkDescriptorSetLayout sceneDescriptorSetLayout;
VkDescriptorSetLayout modelDescriptorSetLayout;
VkDescriptorSetLayout visualizerDescriptorSetLayout;
VkDescriptorSetLayout lineDescriptorSetLayout;
VkPipelineLayout pipelineLayout;
VkPipelineLayout visualizerPipelineLayout;
VkPipelineLayout linePipelineLayout;
VkPipeline graphicsPipeline;
VkPipeline visualizerPipeline;
VkPipeline linePipeline;
VkDescriptorPool descriptorPool;
VkDescriptorSet sceneDescriptorSet;

AppState state;


static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                         VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& alloc) {
    VkBufferCreateInfo bi = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage };
    VmaAllocationCreateInfo ai = { .usage = memoryUsage };
    vmaCreateBuffer(graphics::internal::context.allocator, &bi, &ai, &buffer, &alloc, nullptr);
}

static void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = graphics::internal::beginSingleTimeCommands();
    VkBufferCopy region = { 0, 0, size };
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    graphics::internal::endSingleTimeCommands(cmd);
}


static void uploadBufferToGPU(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                              VkBuffer& buffer, VmaAllocation& alloc) {
    VkBuffer staging;
    VmaAllocation stagingAlloc;
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, staging, stagingAlloc);
    void* mapped;
    vmaMapMemory(graphics::internal::context.allocator, stagingAlloc, &mapped);
    memcpy(mapped, data, size);
    vmaUnmapMemory(graphics::internal::context.allocator, stagingAlloc);

    createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, buffer, alloc);
    copyBuffer(staging, buffer, size);
    vmaDestroyBuffer(graphics::internal::context.allocator, staging, stagingAlloc);
}

static void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };

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
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    int mw = w, mh = h;
    for (uint32_t i = 1; i < levels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit = {};
        blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mw, mh, 1 };
        blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mw > 1 ? mw / 2 : 1, mh > 1 ? mh / 2 : 1, 1 };
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        mw = mw > 1 ? mw / 2 : 1;
        mh = mh > 1 ? mh / 2 : 1;
    }
    barrier.subresourceRange.baseMipLevel = levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static TextureImage createTextureImage(const unsigned char* pixels, int w, int h) {
    auto& ctx = graphics::internal::context;
    TextureImage tex;
    tex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

    VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4;
    VkBuffer staging;
    VmaAllocation stagingAlloc;
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, staging, stagingAlloc);
    void* mapped;
    vmaMapMemory(ctx.allocator, stagingAlloc, &mapped);
    memcpy(mapped, pixels, size);
    vmaUnmapMemory(ctx.allocator, stagingAlloc);

    VkImageCreateInfo ii = {};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    ii.mipLevels = tex.mipLevels;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo ai = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateImage(ctx.allocator, &ii, &ai, &tex.image, &tex.alloc, nullptr);

    VkCommandBuffer cmd = graphics::internal::beginSingleTimeCommands();
    transitionImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex.mipLevels);
    VkBufferImageCopy region = {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    generateMipmaps(cmd, tex.image, w, h, tex.mipLevels);
    graphics::internal::endSingleTimeCommands(cmd);
    vmaDestroyBuffer(ctx.allocator, staging, stagingAlloc);

    VkImageViewCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = tex.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, tex.mipLevels, 0, 1 };
    vkCreateImageView(ctx.device, &vi, nullptr, &tex.view);
    return tex;
}

static void destroyTexture(TextureImage& t) {
    auto& ctx = graphics::internal::context;
    if (t.view) vkDestroyImageView(ctx.device, t.view, nullptr);
    if (t.image) vmaDestroyImage(ctx.allocator, t.image, t.alloc);
    t = {};
}

static bool loadTextureFromFile(const std::string& path, TextureImage& out) {
    int w, h, channels;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return false;
    }
    out = createTextureImage(pixels, w, h);
    stbi_image_free(pixels);
    return true;
}


static float hash2(int x, int y) {
    uint32_t n = uint32_t(x * 374761393 + y * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    return float((n ^ (n >> 16)) & 0xFFFFFF) / float(0xFFFFFF);
}

static float valueNoise(float x, float y) {
    int xi = int(std::floor(x)), yi = int(std::floor(y));
    float xf = x - xi, yf = y - yi;
    float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
    float a00 = hash2(xi, yi), a10 = hash2(xi + 1, yi);
    float a01 = hash2(xi, yi + 1), a11 = hash2(xi + 1, yi + 1);
    return a00 * (1 - u) * (1 - v) + a10 * u * (1 - v) + a01 * (1 - u) * v + a11 * u * v;
}

static float fbm(float x, float y) {
    float s = 0, amp = 0.5f, f = 1;
    for (int i = 0; i < 4; ++i) { s += amp * valueNoise(x * f, y * f); f *= 2; amp *= 0.5f; }
    return s;
}

static void generateDefaultTextures() {
    const int S = 256;
    auto writeIfMissing = [&](const char* name, auto pixelFn) {
        fs::path p = fs::path("textures") / name;
        if (fs::exists(p)) return;
        std::vector<unsigned char> px(S * S * 4);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                glm::vec3 col = glm::clamp(pixelFn(x, y), 0.0f, 1.0f);
                size_t i = (size_t(y) * S + x) * 4;
                px[i + 0] = (unsigned char)(col.r * 255);
                px[i + 1] = (unsigned char)(col.g * 255);
                px[i + 2] = (unsigned char)(col.b * 255);
                px[i + 3] = 255;
            }
        }
        stbi_write_png(p.string().c_str(), S, S, 4, px.data(), S * 4);
        std::cout << "Generated texture: " << p.string() << "\n";
    };

    auto isFrame = [&](int x, int y) { return x < 18 || y < 18 || x >= S - 18 || y >= S - 18; };

    writeIfMissing("crate.png", [&](int x, int y) {
        float n = fbm(x * 0.08f, y * 0.08f);
        if (isFrame(x, y)) return glm::vec3(0.42f + 0.15f * n);
        glm::vec3 wood(0.55f + 0.20f * n, 0.36f + 0.14f * n, 0.18f + 0.08f * n);
        return (y % 64) < 3 ? wood * 0.45f : wood;
    });

    writeIfMissing("crate_spec.png", [&](int x, int y) {
        float n = fbm(x * 0.08f, y * 0.08f);
        return glm::vec3(isFrame(x, y) ? 0.85f : 0.05f + 0.05f * n);
    });

    writeIfMissing("crate_emissive.png", [&](int x, int y) {
        float g = fbm(x * 0.035f + 13.7f, y * 0.035f + 5.2f);
        float k = glm::clamp((g - 0.58f) / 0.25f, 0.0f, 1.0f);
        return glm::vec3(0.05f, 0.9f, 0.75f) * (k * k);
    });

    writeIfMissing("bricks.png", [&](int x, int y) {
        int row = y / 32;
        int bx = (x + (row % 2) * 32) % 64;
        int by = y % 32;
        float n = fbm(x * 0.1f, y * 0.1f);
        if (by < 4 || bx < 4) return glm::vec3(0.55f + 0.1f * n);
        return glm::vec3(0.62f + 0.15f * n, 0.26f + 0.08f * n, 0.2f + 0.06f * n);
    });

    writeIfMissing("checker.png", [&](int x, int y) {
        bool dark = ((x / 32) + (y / 32)) % 2;
        return dark ? glm::vec3(0.22f, 0.22f, 0.26f) : glm::vec3(0.9f, 0.88f, 0.82f);
    });

    writeIfMissing("marble.png", [&](int x, int y) {
        float t = 0.5f + 0.5f * sin(x * 0.045f + fbm(x * 0.02f, y * 0.02f) * 7.0f);
        return glm::mix(glm::vec3(0.25f, 0.27f, 0.3f), glm::vec3(0.92f), t);
    });
}

static void loadMaterials() {
    fs::create_directories("textures");
    generateDefaultTextures();
    stbi_set_flip_vertically_on_load(1);

    const unsigned char black[4] = { 0, 0, 0, 255 };
    blackTexture = createTextureImage(black, 1, 1);

    VkSamplerCreateInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod = 16.0f;
    si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vkCreateSampler(graphics::internal::context.device, &si, nullptr, &textureSampler);

    auto endsWith = [](const std::string& s, const std::string& suf) {
        return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };


    std::vector<fs::path> albedoFiles;
    for (const auto& e : fs::directory_iterator("textures")) {
        if (!e.is_regular_file()) continue;
        std::string stem = e.path().stem().string();
        if (endsWith(stem, "_spec") || endsWith(stem, "_emissive")) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif") {
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

        fs::path specPath = findCompanion(path, "_spec");
        if (specPath.empty() || !loadTextureFromFile(specPath.string(), m.specular)) {
            m.specular = blackTexture; 
        }
        fs::path emPath = findCompanion(path, "_emissive");
        if (emPath.empty() || !loadTextureFromFile(emPath.string(), m.emissive)) {
            m.emissive = blackTexture;
        }
        materials.push_back(m);
    }

    if (materials.empty()) {
        Material m;
        m.name = "white";
        const unsigned char white[4] = { 255, 255, 255, 255 };
        m.albedo = createTextureImage(white, 1, 1);
        m.specular = blackTexture;
        m.emissive = blackTexture;
        materials.push_back(m);
    }
}


bool initialize() {
    auto& ctx = graphics::internal::context;
    auto& device = ctx.device;
    auto& allocator = ctx.allocator;

    state.pointLights.push_back({ {0.0f, 2.0f, 0.0f}, {1.0f, 0.8f, 0.6f}, 1.5f, 1.0f, 0.09f, 0.032f });
    state.spotLights.push_back({ {0.0f, 3.0f, -2.0f}, {0.0f, -1.0f, 1.0f}, {0.4f, 0.7f, 1.0f}, 2.0f,
        glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(17.5f)), 1.0f, 0.09f, 0.032f });


    uploadBufferToGPU(vertices.data(), sizeof(Vertex) * vertices.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferAlloc);
    uploadBufferToGPU(indices.data(), sizeof(uint16_t) * indices.size(),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferAlloc);

    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VmaAllocationCreateInfo allocInfo = { .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };

    bufferInfo.size = sizeof(SceneUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &sceneUboBuffer, &sceneUboBufferAlloc, nullptr);

    bufferInfo.size = sizeof(PointLightData) * MAX_LIGHTS;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &pointLightBuffer, &pointLightBufferAlloc, nullptr);
    bufferInfo.size = sizeof(SpotLightData) * MAX_LIGHTS;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &spotLightBuffer, &spotLightBufferAlloc, nullptr);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(ctx.physical_device, &props);
    size_t minAlign = props.limits.minUniformBufferOffsetAlignment;
    modelUboAlignedSize = (sizeof(ModelUBO) + minAlign - 1) & ~(minAlign - 1);
    bufferInfo.size = modelUboAlignedSize * MAX_OBJECTS;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &modelUboBuffer, &modelUboBufferAlloc, nullptr);

    bufferInfo.size = sizeof(VisualizerUBO);
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &visualizerUboBuffers[i], &visualizerUboBufferAllocs[i], nullptr);
    }
    bufferInfo.size = sizeof(LineVertex) * 50000;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &lineVertexBuffer, &lineVertexBufferAlloc, nullptr);
    bufferInfo.size = sizeof(LineUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &lineUboBuffer, &lineUboBufferAlloc, nullptr);
    loadMaterials();
    VkDescriptorSetLayoutBinding sceneBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 3, sceneBindings };
    vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr, &sceneDescriptorSetLayout);

    VkDescriptorSetLayoutBinding modelBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo modelLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 4, modelBindings };
    vkCreateDescriptorSetLayout(device, &modelLayoutInfo, nullptr, &modelDescriptorSetLayout);

    VkDescriptorSetLayoutBinding visBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo visLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &visBinding };
    vkCreateDescriptorSetLayout(device, &visLayoutInfo, nullptr, &visualizerDescriptorSetLayout);

    VkDescriptorSetLayoutBinding lineBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo lineLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &lineBinding };
    vkCreateDescriptorSetLayout(device, &lineLayoutInfo, nullptr, &lineDescriptorSetLayout);

    VkPushConstantRange pcRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };
    VkDescriptorSetLayout setLayouts[] = { sceneDescriptorSetLayout, modelDescriptorSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 2, setLayouts, 1, &pcRange };
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkDescriptorSetLayout visSetLayouts[] = { visualizerDescriptorSetLayout };
    VkPipelineLayoutCreateInfo visPipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, visSetLayouts, 0, nullptr };
    vkCreatePipelineLayout(device, &visPipelineLayoutInfo, nullptr, &visualizerPipelineLayout);

    VkDescriptorSetLayout lineSetLayouts[] = { lineDescriptorSetLayout };
    VkPipelineLayoutCreateInfo linePipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, lineSetLayouts, 0, nullptr };
    vkCreatePipelineLayout(device, &linePipelineLayoutInfo, nullptr, &linePipelineLayout);

    auto vertShaderCode = readFile("shaders/shader.vert.spv");
    auto fragShaderCode = readFile("shaders/shader.frag.spv");
    auto visVertShaderCode = readFile("shaders/visualize.vert.spv");
    auto visFragShaderCode = readFile("shaders/visualize.frag.spv");

    VkShaderModule vertShaderModule, fragShaderModule, visVertModule, visFragModule;
    VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, vertShaderCode.size(), reinterpret_cast<const uint32_t*>(vertShaderCode.data()) };
    vkCreateShaderModule(device, &smci, nullptr, &vertShaderModule);
    smci.codeSize = fragShaderCode.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    vkCreateShaderModule(device, &smci, nullptr, &fragShaderModule);
    smci.codeSize = visVertShaderCode.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(visVertShaderCode.data());
    vkCreateShaderModule(device, &smci, nullptr, &visVertModule);
    smci.codeSize = visFragShaderCode.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(visFragShaderCode.data());
    vkCreateShaderModule(device, &smci, nullptr, &visFragModule);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main", nullptr }
    };

    VkVertexInputBindingDescription bindingDesc = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrDesc[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) },
        { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) }
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bindingDesc, 3, attrDesc };

    VkVertexInputBindingDescription lineBindingDesc = { 0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription lineAttrDesc[] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LineVertex, pos) },
        { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LineVertex, color) }
    };
    VkPipelineVertexInputStateCreateInfo lineVertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &lineBindingDesc, 2, lineAttrDesc };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
    VkPipelineInputAssemblyStateCreateInfo lineInputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE };
    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr };
    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f };
    VkPipelineRasterizationStateCreateInfo lineRasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f };
    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.0f, nullptr, VK_FALSE, VK_FALSE };
    VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE, {}, {}, 0.0f, 0.0f };
    VkPipelineColorBlendAttachmentState colorBlendAttachment = { VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &colorBlendAttachment, {0.0f, 0.0f, 0.0f, 0.0f} };
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dynamicStates };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr,
        &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicState, pipelineLayout, ctx.render_pass, 0, VK_NULL_HANDLE, -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

    VkPipelineShaderStageCreateInfo visStages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, visVertModule, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, visFragModule, "main", nullptr }
    };
    VkGraphicsPipelineCreateInfo visPipelineInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, visStages, &vertexInputInfo, &inputAssembly, nullptr,
        &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicState, visualizerPipelineLayout, ctx.render_pass, 0, VK_NULL_HANDLE, -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &visPipelineInfo, nullptr, &visualizerPipeline);

    VkGraphicsPipelineCreateInfo linePipelineInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, visStages, &lineVertexInputInfo, &lineInputAssembly, nullptr,
        &viewportState, &lineRasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicState, linePipelineLayout, ctx.render_pass, 0, VK_NULL_HANDLE, -1
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &linePipelineInfo, nullptr, &linePipeline);

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, visFragModule, nullptr);
    vkDestroyShaderModule(device, visVertModule, nullptr);

    const uint32_t matCount = static_cast<uint32_t>(materials.size());
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 + MAX_LIGHTS * 2 },          
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, matCount },           
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, matCount * 3 }       
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2 + MAX_LIGHTS * 2 + matCount,
        .poolSizeCount = 4,
        .pPoolSizes = poolSizes
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo allocInfoSet = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1, &sceneDescriptorSetLayout };
    vkAllocateDescriptorSets(device, &allocInfoSet, &sceneDescriptorSet);
    allocInfoSet.pSetLayouts = &modelDescriptorSetLayout;
    for (auto& m : materials) {
        vkAllocateDescriptorSets(device, &allocInfoSet, &m.descriptorSet);
    }
    allocInfoSet.pSetLayouts = &visualizerDescriptorSetLayout;
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vkAllocateDescriptorSets(device, &allocInfoSet, &visualizerDescriptorSets[i]);
    }
    allocInfoSet.pSetLayouts = &lineDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfoSet, &lineDescriptorSet);

  
    VkDescriptorBufferInfo sceneInfo = { sceneUboBuffer, 0, sizeof(SceneUBO) };
    VkDescriptorBufferInfo pointLightInfo = { pointLightBuffer, 0, sizeof(PointLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo spotLightInfo = { spotLightBuffer, 0, sizeof(SpotLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo lineInfo = { lineUboBuffer, 0, sizeof(LineUBO) };
    VkDescriptorBufferInfo modelInfo = { modelUboBuffer, 0, sizeof(ModelUBO) };

    std::vector<VkWriteDescriptorSet> writes;
    auto addBufferWrite = [&](VkDescriptorSet set, uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo* info) {
        VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        w.dstSet = set; w.dstBinding = binding; w.descriptorCount = 1;
        w.descriptorType = type; w.pBufferInfo = info;
        writes.push_back(w);
    };
    addBufferWrite(sceneDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &sceneInfo);
    addBufferWrite(sceneDescriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pointLightInfo);
    addBufferWrite(sceneDescriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &spotLightInfo);
    addBufferWrite(lineDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lineInfo);

    std::vector<VkDescriptorBufferInfo> visBufferInfos(MAX_LIGHTS * 2);
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        visBufferInfos[i] = { visualizerUboBuffers[i], 0, sizeof(VisualizerUBO) };
        addBufferWrite(visualizerDescriptorSets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &visBufferInfos[i]);
    }

    std::vector<VkDescriptorImageInfo> imageInfos(matCount * 3);
    for (uint32_t i = 0; i < matCount; ++i) {
        auto& m = materials[i];
        addBufferWrite(m.descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &modelInfo);
        imageInfos[i * 3 + 0] = { textureSampler, m.albedo.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[i * 3 + 1] = { textureSampler, m.specular.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[i * 3 + 2] = { textureSampler, m.emissive.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        for (uint32_t binding = 0; binding < 3; ++binding) {
            VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            w.dstSet = m.descriptorSet; w.dstBinding = 1 + binding; w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &imageInfos[i * 3 + binding];
            writes.push_back(w);
        }
    }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);


    auto matIndex = [&](const char* name) -> int {
        for (size_t i = 0; i < materials.size(); ++i) {
            if (materials[i].name == name) return static_cast<int>(i);
        }
        return 0;
    };
    ObjectState o1;
    o1.position = {-1.45f, 0.0f, 0.0f};
    o1.rotation = {20.0f, 0.0f, 0.0f};
    o1.scale = {0.65f, 0.65f, 0.65f};
    o1.materialIndex = matIndex("crate");
    o1.shininess = 48.0f;

    ObjectState o2;
    o2.position = {1.45f, 0.0f, 0.0f};
    o2.rotation = {-22.0f, 0.0f, 0.0f};
    o2.scale = {0.65f, 0.65f, 0.65f};
    o2.materialIndex = matIndex("marble");
    o2.useTrajectory = true;
    o2.fancySampling = true; 

    ObjectState floorObj;
    floorObj.position = {0.0f, -0.95f, 0.0f};
    floorObj.scale = {4.5f, 0.05f, 4.5f};
    floorObj.materialIndex = matIndex("bricks");
    floorObj.uvScale = 4.0f;
    floorObj.shininess = 8.0f;
    floorObj.specularTint = {0.3f, 0.3f, 0.3f};

    state.objects = { o1, o2, floorObj };
    return true;
}

void shutdown() {
    auto& ctx = graphics::internal::context;
    vkQueueWaitIdle(ctx.graphics_queue);
    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
    vkDestroyPipeline(ctx.device, visualizerPipeline, nullptr);
    vkDestroyPipeline(ctx.device, linePipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, visualizerPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, linePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, sceneDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, modelDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, visualizerDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, lineDescriptorSetLayout, nullptr);

    vkDestroySampler(ctx.device, textureSampler, nullptr);
    for (auto& m : materials) {
        destroyTexture(m.albedo);
        if (m.specular.image != blackTexture.image) destroyTexture(m.specular);
        if (m.emissive.image != blackTexture.image) destroyTexture(m.emissive);
    }
    destroyTexture(blackTexture);

    vmaDestroyBuffer(ctx.allocator, vertexBuffer, vertexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, indexBuffer, indexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, sceneUboBuffer, sceneUboBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, pointLightBuffer, pointLightBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, spotLightBuffer, spotLightBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, modelUboBuffer, modelUboBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, lineVertexBuffer, lineVertexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, lineUboBuffer, lineUboBufferAlloc);
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vmaDestroyBuffer(ctx.allocator, visualizerUboBuffers[i], visualizerUboBufferAllocs[i]);
    }
}


void update([[maybe_unused]] double time, double deltaTime) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 780), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lab 3 - Texturing");

    if (ImGui::CollapsingHeader("Camera Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Camera Position", &state.cameraPos.x, 0.05f);
        ImGui::DragFloat3("Camera Target", &state.cameraTarget.x, 0.05f);
        const char* projItems[] = { "Perspective", "Orthographic" };
        ImGui::Combo("Projection", &state.projectionType, projItems, IM_ARRAYSIZE(projItems));
        if (state.projectionType == 0) {
            ImGui::SliderFloat("Field of View", &state.fov, 10.0f, 120.0f);
        }
    }

    if (ImGui::CollapsingHeader("Directional Light & Ambient")) {
        ImGui::DragFloat3("Dir Light Direction", &state.dirLightDir.x, 0.05f);
        ImGui::ColorEdit3("Dir Light Color", &state.dirLightColor.x);
        ImGui::SliderFloat("Dir Light Intensity", &state.dirLightIntensity, 0.0f, 5.0f);
        ImGui::ColorEdit3("Ambient Color", &state.ambientLight.x);
    }

    if (ImGui::CollapsingHeader("Point Lights (Storage Buffer)")) {
        if (ImGui::Button("Add Point Light") && state.pointLights.size() < MAX_LIGHTS) {
            state.pointLights.push_back({ {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 1.0f, 1.0f, 0.09f, 0.032f });
        }
        for (size_t i = 0; i < state.pointLights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i + 100));
            std::string label = "Point Light " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                ImGui::DragFloat3("Position", &state.pointLights[i].position.x, 0.05f);
                ImGui::ColorEdit3("Color", &state.pointLights[i].color.x);
                ImGui::SliderFloat("Intensity", &state.pointLights[i].intensity, 0.0f, 10.0f);
                ImGui::DragFloat("Constant", &state.pointLights[i].constant, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Linear", &state.pointLights[i].linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &state.pointLights[i].quadratic, 0.001f, 0.0f, 1.0f);
                if (ImGui::Button("Remove")) {
                    state.pointLights.erase(state.pointLights.begin() + i);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Spot Lights (Storage Buffer)")) {
        if (ImGui::Button("Add Spot Light") && state.spotLights.size() < MAX_LIGHTS) {
            state.spotLights.push_back({ {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 2.0f,
                glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(17.5f)), 1.0f, 0.09f, 0.032f });
        }
        for (size_t i = 0; i < state.spotLights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i + 200));
            std::string label = "Spot Light " + std::to_string(i);
            if (ImGui::TreeNode(label.c_str())) {
                ImGui::DragFloat3("Position", &state.spotLights[i].position.x, 0.05f);
                ImGui::DragFloat3("Direction", &state.spotLights[i].direction.x, 0.05f);
                ImGui::ColorEdit3("Color", &state.spotLights[i].color.x);
                ImGui::SliderFloat("Intensity", &state.spotLights[i].intensity, 0.0f, 10.0f);
                float innerDeg = glm::degrees(glm::acos(state.spotLights[i].cutOff));
                float outerDeg = glm::degrees(glm::acos(state.spotLights[i].outerCutOff));
                if (ImGui::SliderFloat("Inner Angle", &innerDeg, 1.0f, 45.0f)) {
                    state.spotLights[i].cutOff = glm::cos(glm::radians(innerDeg));
                }
                if (ImGui::SliderFloat("Outer Angle", &outerDeg, innerDeg, 60.0f)) {
                    state.spotLights[i].outerCutOff = glm::cos(glm::radians(outerDeg));
                }
                ImGui::DragFloat("Constant", &state.spotLights[i].constant, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Linear", &state.spotLights[i].linear, 0.001f, 0.0f, 1.0f);
                ImGui::DragFloat("Quadratic", &state.spotLights[i].quadratic, 0.001f, 0.0f, 1.0f);
                if (ImGui::Button("Remove")) {
                    state.spotLights.erase(state.spotLights.begin() + i);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    std::vector<const char*> matNames;
    for (const auto& m : materials) matNames.push_back(m.name.c_str());

    const char* objectNames[] = { "Parallelepiped 1", "Parallelepiped 2", "Floor" };
    for (size_t i = 0; i < state.objects.size(); ++i) {
        ObjectState& obj = state.objects[i];
        if (ImGui::CollapsingHeader(objectNames[i], i < 2 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
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
            ImGui::Checkbox("Fancy Sampling (push const)", &obj.fancySampling);
            ImGui::PopID();
        }
    }

    ImGui::Checkbox("Show Light Visualizers", &state.showLightVisualizers);
    if (ImGui::Button(state.isPaused ? "Resume" : "Pause")) { state.isPaused = !state.isPaused; }
    ImGui::SameLine();
    if (ImGui::Button("Reset Time")) { state.animTime = 0.0f; }
    ImGui::End();

    if (!state.isPaused) {
        state.animTime += static_cast<float>(deltaTime) * state.animSpeed;
    }
}


void render(const graphics::internal::FrameData& fd) {
    auto& ctx = graphics::internal::context;
    void* mapped;

    SceneUBO sceneUbo = {};
    sceneUbo.view = glm::lookAt(state.cameraPos, state.cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = static_cast<float>(ctx.swapchain_extent.width) / static_cast<float>(ctx.swapchain_extent.height);
    if (state.projectionType == 0) {
        sceneUbo.proj = glm::perspective(glm::radians(state.fov), aspect, 0.1f, 100.0f);
    } else {
        constexpr float size = 3.0f;
        sceneUbo.proj = glm::ortho(-size * aspect, size * aspect, -size, size, -100.0f, 100.0f);
    }
    sceneUbo.proj[1][1] *= -1.0f;
    sceneUbo.cameraPos = state.cameraPos;
    sceneUbo.dirLightDir = state.dirLightDir;
    sceneUbo.dirLightColor = state.dirLightColor;
    sceneUbo.ambientColor = state.ambientLight;
    sceneUbo.dirLightIntensity = state.dirLightIntensity;
    sceneUbo.pointLightCount = static_cast<int>(state.pointLights.size());
    sceneUbo.spotLightCount = static_cast<int>(state.spotLights.size());
    vmaMapMemory(ctx.allocator, sceneUboBufferAlloc, &mapped);
    memcpy(mapped, &sceneUbo, sizeof(SceneUBO));
    vmaUnmapMemory(ctx.allocator, sceneUboBufferAlloc);

    if (!state.pointLights.empty()) {
        vmaMapMemory(ctx.allocator, pointLightBufferAlloc, &mapped);
        memcpy(mapped, state.pointLights.data(), sizeof(PointLightData) * state.pointLights.size());
        vmaUnmapMemory(ctx.allocator, pointLightBufferAlloc);
    }
    if (!state.spotLights.empty()) {
        vmaMapMemory(ctx.allocator, spotLightBufferAlloc, &mapped);
        memcpy(mapped, state.spotLights.data(), sizeof(SpotLightData) * state.spotLights.size());
        vmaUnmapMemory(ctx.allocator, spotLightBufferAlloc);
    }


    vmaMapMemory(ctx.allocator, modelUboBufferAlloc, &mapped);
    for (size_t i = 0; i < state.objects.size(); ++i) {
        const ObjectState& obj = state.objects[i];
        glm::vec3 pos = obj.position;
        if (obj.useTrajectory) {
            pos.x += cos(state.animTime) * state.animRadius;
            pos.y += sin(state.animTime) * state.animHeight;
        }
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, obj.scale);
        ModelUBO modelUbo = { model, obj.albedoTint, obj.shininess, obj.specularTint, obj.uvScale };
        memcpy(static_cast<char*>(mapped) + i * modelUboAlignedSize, &modelUbo, sizeof(ModelUBO));
    }
    vmaUnmapMemory(ctx.allocator, modelUboBufferAlloc);

    VkViewport vp = { 0.0f, 0.0f, static_cast<float>(ctx.swapchain_extent.width), static_cast<float>(ctx.swapchain_extent.height), 0.0f, 1.0f };
    vkCmdSetViewport(fd.command_buffer, 0, 1, &vp);
    VkRect2D scissor = { {0, 0}, ctx.swapchain_extent };
    vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

    VkBuffer vBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, vBuffers, offsets);
    vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    for (size_t i = 0; i < state.objects.size(); ++i) {
        const ObjectState& obj = state.objects[i];
        uint32_t dynamicOffset = static_cast<uint32_t>(i * modelUboAlignedSize);
        VkDescriptorSet sets[] = { sceneDescriptorSet, materials[obj.materialIndex].descriptorSet };
        vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 2, sets, 1, &dynamicOffset);

        PushConstants pc = { obj.fancySampling ? 1 : 0, state.animTime };
        vkCmdPushConstants(fd.command_buffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }

    if (state.showLightVisualizers) {
        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visualizerPipeline);
        int visIndex = 0;
        auto drawVisualizer = [&](glm::vec3 pos, glm::vec3 color) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, glm::vec3(0.05f));
            VisualizerUBO visUbo = { sceneUbo.proj * sceneUbo.view * model, color };
            vmaMapMemory(ctx.allocator, visualizerUboBufferAllocs[visIndex], &mapped);
            memcpy(mapped, &visUbo, sizeof(VisualizerUBO));
            vmaUnmapMemory(ctx.allocator, visualizerUboBufferAllocs[visIndex]);
            vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                visualizerPipelineLayout, 0, 1, &visualizerDescriptorSets[visIndex], 0, nullptr);
            vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            visIndex++;
        };
        for (const auto& pl : state.pointLights) drawVisualizer(pl.position, pl.color);
        for (const auto& sl : state.spotLights) drawVisualizer(sl.position, sl.color);
        std::vector<LineVertex> lineVerts;
        for (const auto& sl : state.spotLights) {
            glm::vec3 dir = glm::normalize(sl.direction);
            glm::vec3 helper = (glm::abs(dir.z) < 0.9f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            glm::vec3 u = glm::normalize(glm::cross(dir, helper));
            glm::vec3 v = glm::cross(dir, u);
            float length = 2.0f;
            glm::vec3 baseCenter = sl.position + dir * length;
            glm::vec4 whiteColor(1.0f);
            {
                float radius = length * glm::tan(glm::acos(sl.outerCutOff));
                std::vector<glm::vec3> outerPoints;
                for (int i = 0; i < 16; ++i) {
                    float ang = (i * 2.0f * 3.14159f) / 16.0f;
                    outerPoints.push_back(baseCenter + (glm::cos(ang) * u + glm::sin(ang) * v) * radius);
                }
                for (int i = 0; i < 16; ++i) {
                    lineVerts.push_back({ sl.position, whiteColor });
                    lineVerts.push_back({ outerPoints[i], whiteColor });
                }
                for (int i = 0; i < 16; ++i) {
                    lineVerts.push_back({ outerPoints[i], whiteColor });
                    lineVerts.push_back({ outerPoints[(i + 1) % 16], whiteColor });
                }
            }
            {
                float radius = length * glm::tan(glm::acos(sl.cutOff));
                std::vector<glm::vec3> innerPoints;
                for (int i = 0; i < 16; ++i) {
                    float ang = (i * 2.0f * 3.14159f) / 16.0f;
                    innerPoints.push_back(baseCenter + (glm::cos(ang) * u + glm::sin(ang) * v) * radius);
                }
                for (int i = 0; i < 16; ++i) {
                    lineVerts.push_back({ innerPoints[i], whiteColor });
                    lineVerts.push_back({ innerPoints[(i + 1) % 16], whiteColor });
                }
            }
        }
        if (!lineVerts.empty()) {
            vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
            vmaMapMemory(ctx.allocator, lineVertexBufferAlloc, &mapped);
            memcpy(mapped, lineVerts.data(), sizeof(LineVertex) * lineVerts.size());
            vmaUnmapMemory(ctx.allocator, lineVertexBufferAlloc);
            LineUBO lineUbo = { sceneUbo.proj * sceneUbo.view, glm::vec3(1.0f) };
            vmaMapMemory(ctx.allocator, lineUboBufferAlloc, &mapped);
            memcpy(mapped, &lineUbo, sizeof(LineUBO));
            vmaUnmapMemory(ctx.allocator, lineUboBufferAlloc);
            vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                linePipelineLayout, 0, 1, &lineDescriptorSet, 0, nullptr);
            VkBuffer lvBuffers[] = { lineVertexBuffer };
            VkDeviceSize lOffsets[] = { 0 };
            vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, lvBuffers, lOffsets);
            vkCmdDraw(fd.command_buffer, static_cast<uint32_t>(lineVerts.size()), 1, 0, 0);
        }
    }
}

} 