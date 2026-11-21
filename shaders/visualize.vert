#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform VisualizerUBO {
    mat4 mvp;
    vec3 color;
} ubo;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    fragColor   = ubo.color;
}