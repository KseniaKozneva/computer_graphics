#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 dirLightDir;
    vec3 dirLightColor;
    vec3 ambientColor;
    float dirLightIntensity;
    int pointLightCount;
    int spotLightCount;
} scene;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec3 albedoTint;
    float shininess;
    vec3 specularTint;
    float uvScale;
} ubo;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragUV = inUV * ubo.uvScale;
    gl_Position = scene.proj * scene.view * worldPos;
}