#include "application.hpp"
#include <imgui.h>
#include <fstream>
#include <iostream>
#include <vector>

namespace application {

const std::vector<Vertex> vertices = {
    {{ 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
    {{ 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2,  0, 2, 3,  0, 3, 4,  0, 4, 1,
    5, 2, 1,  5, 3, 2,  5, 4, 3,  5, 1, 4
};

VkBuffer vertexBuffer;
VmaAllocation vertexBufferAlloc;
VkBuffer indexBuffer;
VmaAllocation indexBufferAlloc;

VkBuffer uboBuffer1;
VmaAllocation uboBufferAlloc1;
VkBuffer uboBuffer2;
VmaAllocation uboBufferAlloc2;

VkDescriptorSetLayout descriptorSetLayout;
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;
VkDescriptorPool descriptorPool;
std::vector<VkDescriptorSet> descriptorSets;

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

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    };
    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU
    };
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &vertexBuffer, &vertexBufferAlloc, nullptr);
    
    void* data;
    vmaMapMemory(allocator, vertexBufferAlloc, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vmaUnmapMemory(allocator, vertexBufferAlloc);

    bufferSize = sizeof(indices[0]) * indices.size();
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &indexBuffer, &indexBufferAlloc, nullptr);
    
    vmaMapMemory(allocator, indexBufferAlloc, &data);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vmaUnmapMemory(allocator, indexBufferAlloc);

    bufferSize = sizeof(UniformBufferObject);
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &uboBuffer1, &uboBufferAlloc1, nullptr);
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &uboBuffer2, &uboBufferAlloc2, nullptr);

    VkDescriptorSetLayoutBinding uboLayoutBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding
    };
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout
    };
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    auto vertShaderCode = readFile("shaders/shader.vert.spv");
    auto fragShaderCode = readFile("shaders/shader.frag.spv");

    VkShaderModule vertShaderModule, fragShaderModule;
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertShaderCode.size(),
        .pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data())
    };
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &vertShaderModule);
    shaderModuleCreateInfo.codeSize = fragShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &fragShaderModule);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main"
        }
    };

    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos)
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, color)
        }
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };
    
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = ctx.render_pass,
        .subpass = 0
    };
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 2
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);

    std::vector<VkDescriptorSetLayout> layouts(2, descriptorSetLayout);
    VkDescriptorSetAllocateInfo descAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts.data()
    };
    descriptorSets.resize(2);
    vkAllocateDescriptorSets(device, &descAllocInfo, descriptorSets.data());

    VkDescriptorBufferInfo descriptorBufferInfo1 = {
        .buffer = uboBuffer1,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };
    VkDescriptorBufferInfo descriptorBufferInfo2 = {
        .buffer = uboBuffer2,
        .offset = 0,
        .range = sizeof(UniformBufferObject)
    };
    
    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[0],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptorBufferInfo1
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[1],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptorBufferInfo2
        }
    };
    vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);

    return true;
}

void shutdown() {
    auto& ctx = graphics::internal::context;
    vkQueueWaitIdle(ctx.graphics_queue);
    
    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, descriptorSetLayout, nullptr);
    
    vmaDestroyBuffer(ctx.allocator, vertexBuffer, vertexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, indexBuffer, indexBufferAlloc);
    vmaDestroyBuffer(ctx.allocator, uboBuffer1, uboBufferAlloc1);
    vmaDestroyBuffer(ctx.allocator, uboBuffer2, uboBufferAlloc2);
}

void update([[maybe_unused]] double time, double deltaTime) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 550), ImGuiCond_FirstUseEver);

    ImGui::Begin("Lab 1 - Octahedron");

    ImGui::Text("Variant 7: 1225 vertices, 2304 triangles, 2 objects");

    const char* projItems[] = { "Perspective", "Orthographic" };
    ImGui::Combo("Projection", &state.projectionType, projItems, IM_ARRAYSIZE(projItems));
    if (state.projectionType == 0) {
        ImGui::SliderFloat("Field of view", &state.fov, 10.0f, 120.0f);
    }

    auto renderObjectUI = [](const char* headerName, ObjectState& obj) {
        if (ImGui::CollapsingHeader(headerName, ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &obj.position.x, 0.05f);
            ImGui::DragFloat3("Rotation (deg)", &obj.rotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f);
            ImGui::ColorEdit3("Color multiplier", &obj.color.x);
            ImGui::Checkbox("Trajectory", &obj.useTrajectory);
        }
    };

    renderObjectUI("Octahedron 1", state.obj1);
    renderObjectUI("Octahedron 2", state.obj2);

    if (ImGui::Button(state.isPaused ? "Resume" : "Pause")) {
        state.isPaused = !state.isPaused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        state.animTime = 0.0f;
    }

    ImGui::SliderFloat("Speed", &state.animSpeed, 0.0f, 5.0f);
    ImGui::SliderFloat("Radius", &state.animRadius, 0.0f, 5.0f);
    ImGui::SliderFloat("Height", &state.animHeight, 0.0f, 5.0f);

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo", &state.showDemoWindow);

    if (state.showDemoWindow) {
        ImGui::ShowDemoWindow(&state.showDemoWindow);
    }

    ImGui::End();

    if (!state.isPaused) {
        state.animTime += static_cast<float>(deltaTime) * state.animSpeed;
    }
}

void render(const graphics::internal::FrameData& fd) {
    auto& ctx = graphics::internal::context;
    
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    
    glm::mat4 proj;
    float aspectRatio = static_cast<float>(ctx.swapchain_extent.width) / 
                        static_cast<float>(ctx.swapchain_extent.height);
    
    if (state.projectionType == 0) {
        proj = glm::perspective(glm::radians(state.fov), aspectRatio, 0.1f, 100.0f);
    } else {
        proj = glm::ortho(-2.0f * aspectRatio, 2.0f * aspectRatio, -2.0f, 2.0f, 0.1f, 100.0f);
    }
    proj[1][1] *= -1.0f;

    auto drawObject = [&](const ObjectState& obj, VkDescriptorSet descSet) {
        glm::mat4 model = glm::mat4(1.0f);
        
        glm::vec3 currentPos = obj.position;
        
        if (obj.useTrajectory) {
            currentPos.x += cos(state.animTime) * state.animRadius;
            currentPos.y += sin(state.animTime) * state.animHeight;
            currentPos.z += sin(state.animTime) * state.animRadius;
        }
        
        model = glm::translate(model, currentPos);
        model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, obj.scale);

        UniformBufferObject ubo = {};
        ubo.model = model;
        ubo.view = view;
        ubo.proj = proj;
        ubo.uiColor = glm::vec4(obj.color, 1.0f);

        void* mappedData;
        VmaAllocation alloc = (descSet == descriptorSets[0]) ? uboBufferAlloc1 : uboBufferAlloc2;
        vmaMapMemory(ctx.allocator, alloc, &mappedData);
        memcpy(mappedData, &ubo, sizeof(ubo));
        vmaUnmapMemory(ctx.allocator, alloc);

        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            pipelineLayout, 0, 1, &descSet, 0, nullptr);
        
        VkViewport viewport = {
            0.0f, 0.0f,
            static_cast<float>(ctx.swapchain_extent.width),
            static_cast<float>(ctx.swapchain_extent.height),
            0.0f, 1.0f
        };
        vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
        VkRect2D scissor = { {0, 0}, ctx.swapchain_extent };
        vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        
        vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    };

    drawObject(state.obj1, descriptorSets[0]);
    drawObject(state.obj2, descriptorSets[1]);
}

} // namespace application