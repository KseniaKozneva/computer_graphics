#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 uiColor;
} ubo;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 proceduralColor = vec4(abs(inPosition), 1.0);
    
    fragColor = proceduralColor; 
    
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
}