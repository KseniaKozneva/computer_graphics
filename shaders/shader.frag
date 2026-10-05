#version 450

layout(location = 0) in vec4 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 uiColor;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor * ubo.uiColor;
}