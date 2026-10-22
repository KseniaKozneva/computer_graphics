#include "application.hpp"
#include <imgui.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

namespace application {

constexpr float a = 0.8f;
constexpr float b = 0.6f;
constexpr float c = 0.5f;

// Процедурная генерация вершин и нормалей для параллелепипеда
const std::vector<Vertex> vertices = {
    // Нижняя грань (y = -b)
    {{-a, -b, -c}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a, -b, -c}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a, -b,  c}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a, -b,  c}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    // Верхняя грань (y = +b)
    {{-a,  b, -c}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b, -c}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b,  c}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a,  b,  c}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    // Передняя грань (z = +c)
    {{-a, -b,  c}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a, -b,  c}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b,  c}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a,  b,  c}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    // Задняя грань (z = -c)
    {{-a, -b, -c}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a, -b, -c}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b, -c}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a,  b, -c}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    // Левая грань (x = -a)
    {{-a, -b, -c}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a, -b,  c}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a,  b,  c}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-a,  b, -c}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    // Правая грань (x = +a)
    {{ a, -b, -c}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a, -b,  c}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b,  c}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ a,  b, -c}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 2, 1,  0, 3, 2,
    4, 5, 6,  4, 6, 7,
    8, 10, 9,  8, 11, 10,
    12, 13, 14,  12, 14, 15,
    16, 18, 17,  16, 19, 18,
    20, 21, 22,  20, 22, 23
};

// --- Структуры для линий конуса ---
struct LineVertex {
    glm::vec3 pos;
    glm::vec4 color;
};
struct LineUBO {
    alignas(16) glm::mat4 mvp;
};

VkBuffer vertexBuffer;
VmaAllocation vertexBufferAlloc;
VkBuffer indexBuffer;
VmaAllocation indexBufferAlloc;

// Буферы сцены и освещения (Set 0)
VkBuffer sceneUboBuffer;
VmaAllocation sceneUboBufferAlloc;
VkBuffer pointLightBuffer;
VmaAllocation pointLightBufferAlloc;
VkBuffer spotLightBuffer;
VmaAllocation spotLightBufferAlloc;

// Буферы моделей (Set 1)
VkBuffer modelUboBuffer1, modelUboBuffer2;
VmaAllocation modelUboBufferAlloc1, modelUboBufferAlloc2;

// Буферы для визуализации света
constexpr int MAX_LIGHTS = 10;
VkBuffer visualizerUboBuffers[MAX_LIGHTS * 2];
VmaAllocation visualizerUboBufferAllocs[MAX_LIGHTS * 2];
VkDescriptorSet visualizerDescriptorSets[MAX_LIGHTS * 2];

// Буферы для линий (конусы прожекторов)
VkBuffer lineVertexBuffer;
VmaAllocation lineVertexBufferAlloc;
VkBuffer lineUboBuffer;
VmaAllocation lineUboBufferAlloc;
VkDescriptorSet lineDescriptorSet;
VkDescriptorSetLayout lineDescriptorSetLayout;
VkPipeline linePipeline;
VkPipelineLayout linePipelineLayout;

VkDescriptorSetLayout sceneDescriptorSetLayout;
VkDescriptorSetLayout modelDescriptorSetLayout;
VkDescriptorSetLayout visualizerDescriptorSetLayout;

VkPipelineLayout pipelineLayout;
VkPipelineLayout visualizerPipelineLayout;

VkPipeline graphicsPipeline;
VkPipeline visualizerPipeline;

VkDescriptorPool descriptorPool;

VkDescriptorSet sceneDescriptorSet;
VkDescriptorSet modelDescriptorSet1;
VkDescriptorSet modelDescriptorSet2;

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

bool initialize() {
    auto& ctx = graphics::internal::context;
    auto& device = ctx.device;
    auto& allocator = ctx.allocator;

    state.pointLights.push_back({ {0.0f, 2.0f, 0.0f}, {1.0f, 0.8f, 0.6f}, 1.5f, 1.0f, 0.09f, 0.032f });
    state.spotLights.push_back({ {0.0f, 3.0f, -2.0f}, {0.0f, -1.0f, 1.0f}, {0.4f, 0.7f, 1.0f}, 2.0f, glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(17.5f)), 1.0f, 0.09f, 0.032f });

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bufferSize, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT };
    VmaAllocationCreateInfo allocInfo = { .usage = VMA_MEMORY_USAGE_CPU_TO_GPU };
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &vertexBuffer, &vertexBufferAlloc, nullptr);
    void* data;
    vmaMapMemory(allocator, vertexBufferAlloc, &data);
    memcpy(data, vertices.data(), bufferSize);
    vmaUnmapMemory(allocator, vertexBufferAlloc);

    bufferSize = sizeof(indices[0]) * indices.size();
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &indexBuffer, &indexBufferAlloc, nullptr);
    vmaMapMemory(allocator, indexBufferAlloc, &data);
    memcpy(data, indices.data(), bufferSize);
    vmaUnmapMemory(allocator, indexBufferAlloc);

    bufferInfo.size = sizeof(SceneUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &sceneUboBuffer, &sceneUboBufferAlloc, nullptr);

    bufferInfo.size = sizeof(PointLightData) * MAX_LIGHTS;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &pointLightBuffer, &pointLightBufferAlloc, nullptr);

    bufferInfo.size = sizeof(SpotLightData) * MAX_LIGHTS;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &spotLightBuffer, &spotLightBufferAlloc, nullptr);

    bufferInfo.size = sizeof(ModelUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &modelUboBuffer1, &modelUboBufferAlloc1, nullptr);
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &modelUboBuffer2, &modelUboBufferAlloc2, nullptr);

    bufferInfo.size = sizeof(VisualizerUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; 
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &visualizerUboBuffers[i], &visualizerUboBufferAllocs[i], nullptr);
    }

    // Буферы для линий конусов
    bufferInfo.size = sizeof(LineVertex) * 50000; 
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &lineVertexBuffer, &lineVertexBufferAlloc, nullptr);

    bufferInfo.size = sizeof(LineUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &lineUboBuffer, &lineUboBufferAlloc, nullptr);

    VkDescriptorSetLayoutBinding sceneBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };
    VkDescriptorSetLayoutCreateInfo sceneLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 3, sceneBindings };
    vkCreateDescriptorSetLayout(device, &sceneLayoutInfo, nullptr, &sceneDescriptorSetLayout);

    VkDescriptorSetLayoutBinding modelBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo modelLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &modelBinding };
    vkCreateDescriptorSetLayout(device, &modelLayoutInfo, nullptr, &modelDescriptorSetLayout);

    VkDescriptorSetLayoutBinding visBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo visLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &visBinding };
    vkCreateDescriptorSetLayout(device, &visLayoutInfo, nullptr, &visualizerDescriptorSetLayout);

    VkDescriptorSetLayoutBinding lineBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo lineLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &lineBinding };
    vkCreateDescriptorSetLayout(device, &lineLayoutInfo, nullptr, &lineDescriptorSetLayout);

    VkDescriptorSetLayout setLayouts[] = { sceneDescriptorSetLayout, modelDescriptorSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 2, setLayouts, 0, nullptr };
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
        { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color) }
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

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 15 + MAX_LIGHTS * 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 }
    };
    VkDescriptorPoolCreateInfo poolInfo = { 
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, 
        .pNext = nullptr, 
        .flags = 0, 
        .maxSets = 15 + MAX_LIGHTS * 2, 
        .poolSizeCount = 2, 
        .pPoolSizes = poolSizes 
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    VkDescriptorSetAllocateInfo allocInfoSet = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, 1, &sceneDescriptorSetLayout };
    vkAllocateDescriptorSets(device, &allocInfoSet, &sceneDescriptorSet);

    allocInfoSet.pSetLayouts = &modelDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfoSet, &modelDescriptorSet1);
    vkAllocateDescriptorSets(device, &allocInfoSet, &modelDescriptorSet2);

    allocInfoSet.pSetLayouts = &visualizerDescriptorSetLayout;
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vkAllocateDescriptorSets(device, &allocInfoSet, &visualizerDescriptorSets[i]);
    }

    allocInfoSet.pSetLayouts = &lineDescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfoSet, &lineDescriptorSet);

    VkDescriptorBufferInfo sceneInfo = { sceneUboBuffer, 0, sizeof(SceneUBO) };
    VkDescriptorBufferInfo pointLightInfo = { pointLightBuffer, 0, sizeof(PointLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo spotLightInfo = { spotLightBuffer, 0, sizeof(SpotLightData) * MAX_LIGHTS };
    VkDescriptorBufferInfo modelInfo1 = { modelUboBuffer1, 0, sizeof(ModelUBO) };
    VkDescriptorBufferInfo modelInfo2 = { modelUboBuffer2, 0, sizeof(ModelUBO) };
    VkDescriptorBufferInfo lineInfo = { lineUboBuffer, 0, sizeof(LineUBO) };

    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = sceneDescriptorSet, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &sceneInfo, .pTexelBufferView = nullptr },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = sceneDescriptorSet, .dstBinding = 1, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &pointLightInfo, .pTexelBufferView = nullptr },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = sceneDescriptorSet, .dstBinding = 2, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &spotLightInfo, .pTexelBufferView = nullptr },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = modelDescriptorSet1, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &modelInfo1, .pTexelBufferView = nullptr },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = modelDescriptorSet2, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &modelInfo2, .pTexelBufferView = nullptr },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = lineDescriptorSet, .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pImageInfo = nullptr, .pBufferInfo = &lineInfo, .pTexelBufferView = nullptr }
    };
    vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);

    std::vector<VkDescriptorBufferInfo> visBufferInfos(MAX_LIGHTS * 2);
    std::vector<VkWriteDescriptorSet> visWrites(MAX_LIGHTS * 2);
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        visBufferInfos[i] = { visualizerUboBuffers[i], 0, sizeof(VisualizerUBO) };
        visWrites[i] = { 
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, 
            .dstSet = visualizerDescriptorSets[i], .dstBinding = 0, .dstArrayElement = 0, 
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
            .pImageInfo = nullptr, .pBufferInfo = &visBufferInfos[i], .pTexelBufferView = nullptr 
        };
    }
    vkUpdateDescriptorSets(device, MAX_LIGHTS * 2, visWrites.data(), 0, nullptr);

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
    vmaDestroyBuffer(ctx.allocator, vertexBuffer, vertexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, indexBuffer, indexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, sceneUboBuffer, sceneUboBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, pointLightBuffer, pointLightBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, spotLightBuffer, spotLightBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, modelUboBuffer1, modelUboBufferAlloc1);
    vmaDestroyBuffer(ctx.allocator, modelUboBuffer2, modelUboBufferAlloc2);
    vmaDestroyBuffer(ctx.allocator, lineVertexBuffer, lineVertexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, lineUboBuffer, lineUboBufferAlloc);
    
    for (int i = 0; i < MAX_LIGHTS * 2; ++i) {
        vmaDestroyBuffer(ctx.allocator, visualizerUboBuffers[i], visualizerUboBufferAllocs[i]);
    }
}

void update([[maybe_unused]] double time, double deltaTime) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 750), ImGuiCond_FirstUseEver);

    ImGui::Begin("Lab 2 - Camera & Lighting");

    if (ImGui::CollapsingHeader("Camera Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Camera Position", &state.cameraPos.x, 0.05f);
        ImGui::DragFloat3("Camera Target", &state.cameraTarget.x, 0.05f);
        const char* projItems[] = { "Perspective", "Orthographic" };
        ImGui::Combo("Projection", &state.projectionType, projItems, IM_ARRAYSIZE(projItems));
        if (state.projectionType == 0) {
            ImGui::SliderFloat("Field of View", &state.fov, 10.0f, 120.0f);
        }
    }

    if (ImGui::CollapsingHeader("Directional Light & Ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Dir Light Direction", &state.dirLightDir.x, 0.05f);
        ImGui::ColorEdit3("Dir Light Color", &state.dirLightColor.x);
        ImGui::SliderFloat("Dir Light Intensity", &state.dirLightIntensity, 0.0f, 5.0f);
        ImGui::ColorEdit3("Ambient Color", &state.ambientLight.x);
    }

    if (ImGui::CollapsingHeader("Point Lights (Storage Buffer)", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                if (ImGui::Button("Remove") && state.pointLights.size() > 0) {
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

    if (ImGui::CollapsingHeader("Spot Lights (Storage Buffer)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Add Spot Light") && state.spotLights.size() < MAX_LIGHTS) {
            state.spotLights.push_back({ {0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 2.0f, glm::cos(glm::radians(12.5f)), glm::cos(glm::radians(17.5f)), 1.0f, 0.09f, 0.032f });
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

                if (ImGui::Button("Remove") && state.spotLights.size() > 0) {
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

    auto renderObjectUI = [](const char* headerName, ObjectState& obj) {
        if (ImGui::CollapsingHeader(headerName)) {
            ImGui::PushID(headerName);
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &obj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f);
            ImGui::ColorEdit3("Albedo", &obj.albedo.x);
            ImGui::ColorEdit3("Specular", &obj.specularColor.x);
            ImGui::SliderFloat("Shininess", &obj.shininess, 1.0f, 128.0f);
            ImGui::Checkbox("Trajectory", &obj.useTrajectory);
            ImGui::PopID();
        }
    };
    renderObjectUI("Parallelepiped 1", state.obj1);
    renderObjectUI("Parallelepiped 2", state.obj2);

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

    VkViewport vp = { 0.0f, 0.0f, static_cast<float>(ctx.swapchain_extent.width), static_cast<float>(ctx.swapchain_extent.height), 0.0f, 1.0f };
    vkCmdSetViewport(fd.command_buffer, 0, 1, &vp);
    VkRect2D scissor = { {0, 0}, ctx.swapchain_extent };
    vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
    VkBuffer vBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, vBuffers, offsets);
    vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    auto drawObject = [&](const ObjectState& obj, VkDescriptorSet modelDescSet, VmaAllocation modelAlloc) {
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 pos = obj.position;
        if (obj.useTrajectory) {
            pos.x += cos(state.animTime) * state.animRadius;
            pos.y += sin(state.animTime) * state.animHeight;
        }
        model = glm::translate(model, pos);
        model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1,0,0));
        model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0,1,0));
        model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0,0,1));
        model = glm::scale(model, obj.scale);

        ModelUBO modelUbo = { model, obj.albedo, obj.specularColor, obj.shininess };
        vmaMapMemory(ctx.allocator, modelAlloc, &mapped);
        memcpy(mapped, &modelUbo, sizeof(ModelUBO));
        vmaUnmapMemory(ctx.allocator, modelAlloc);

        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        
        VkDescriptorSet sets[] = { sceneDescriptorSet, modelDescSet };
        vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, sets, 0, nullptr);

        vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    };

    drawObject(state.obj1, modelDescriptorSet1, modelUboBufferAlloc1);
    drawObject(state.obj2, modelDescriptorSet2, modelUboBufferAlloc2);

    // --- Визуализация источников света ---
    if (state.showLightVisualizers) {
        // 1. Кубики источников
        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visualizerPipeline);

        int visIndex = 0;
        auto drawVisualizer = [&](glm::vec3 pos, glm::vec3 color) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
            model = glm::scale(model, glm::vec3(0.05f));
            VisualizerUBO visUbo = { sceneUbo.proj * sceneUbo.view * model, color };
            
            vmaMapMemory(ctx.allocator, visualizerUboBufferAllocs[visIndex], &mapped);
            memcpy(mapped, &visUbo, sizeof(VisualizerUBO));
            vmaUnmapMemory(ctx.allocator, visualizerUboBufferAllocs[visIndex]);

            vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, visualizerPipelineLayout, 0, 1, &visualizerDescriptorSets[visIndex], 0, nullptr);
            vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
            
            visIndex++;
        };

        for (const auto& pl : state.pointLights) drawVisualizer(pl.position, pl.color);
        for (const auto& sl : state.spotLights) drawVisualizer(sl.position, sl.color);

        // 2. Конусы прожекторов (как у одногруппника: 4 луча + внутреннее кольцо)
        std::vector<LineVertex> lineVerts;
        for (const auto& sl : state.spotLights) {
            glm::vec3 dir = glm::normalize(sl.direction);
            glm::vec3 helper = (glm::abs(dir.z) < 0.9f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            glm::vec3 u = glm::normalize(glm::cross(dir, helper));
            glm::vec3 v = glm::cross(dir, u);

            float length = 2.0f; // Длина конуса
            glm::vec3 baseCenter = sl.position + dir * length;
            glm::vec4 whiteColor(1.0f, 1.0f, 1.0f, 1.0f); 

            // Внешний угол (16 сегментов для красивого круга)
            {
                float radius = length * glm::tan(glm::acos(sl.outerCutOff));
                std::vector<glm::vec3> outerPoints;
                for (int i = 0; i < 16; ++i) {
                    float a = (i * 2.0f * 3.14159f) / 16.0f;
                    outerPoints.push_back(baseCenter + (glm::cos(a) * u + glm::sin(a) * v) * radius);
                }
                // Лучи от вершины к внешним точкам
                for (int i = 0; i < 16; ++i) {
                    lineVerts.push_back({ sl.position, whiteColor });
                    lineVerts.push_back({ outerPoints[i], whiteColor });
                }
                // Круг основания
                for (int i = 0; i < 16; ++i) {
                    lineVerts.push_back({ outerPoints[i], whiteColor });
                    lineVerts.push_back({ outerPoints[(i + 1) % 16], whiteColor });
                }
            }

            // Внутренний угол (для мягкого края, как требует лаба)
            {
                float radius = length * glm::tan(glm::acos(sl.cutOff));
                std::vector<glm::vec3> innerPoints;
                for (int i = 0; i < 16; ++i) {
                    float a = (i * 2.0f * 3.14159f) / 16.0f;
                    innerPoints.push_back(baseCenter + (glm::cos(a) * u + glm::sin(a) * v) * radius);
                }
                // Внутренний круг
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

            LineUBO lineUbo = { sceneUbo.proj * sceneUbo.view };
            vmaMapMemory(ctx.allocator, lineUboBufferAlloc, &mapped);
            memcpy(mapped, &lineUbo, sizeof(LineUBO));
            vmaUnmapMemory(ctx.allocator, lineUboBufferAlloc);

            vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipelineLayout, 0, 1, &lineDescriptorSet, 0, nullptr);
            
            VkBuffer lvBuffers[] = { lineVertexBuffer };
            VkDeviceSize lOffsets[] = { 0 };
            vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, lvBuffers, lOffsets);
            vkCmdDraw(fd.command_buffer, static_cast<uint32_t>(lineVerts.size()), 1, 0, 0);
        }
    }
}

} // namespace application