#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(set = 0, binding = 0) uniform VisualizerUBO {
    mat4 mvp;
    vec3 color;
} visData;

layout(location = 0) out vec3 outColor;

void main() {
    gl_Position = visData.mvp * vec4(inPosition, 1.0);
    outColor = visData.color;
}