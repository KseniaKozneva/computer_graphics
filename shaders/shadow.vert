#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform LightUBO {
    mat4 lightViewProj;
} light;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec3 albedoTint;
    float shininess;
    vec3 specularTint;
    float uvScale;
} ubo;

void main() {
    gl_Position = light.lightViewProj * ubo.model * vec4(inPosition, 1.0);
}