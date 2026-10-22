#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    int pointLightCount;
    int spotLightCount;
} scene;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec3 albedo;
    vec3 specularColor;
    float shininess;
} modelData;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec4 fragColor;

void main() {
    vec4 worldPos = modelData.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    // Корректная трансформация нормалей через обратную транспонированную матрицу модели
    fragNormal = mat3(transpose(inverse(modelData.model))) * inNormal;
    fragColor = inColor;

    gl_Position = scene.proj * scene.view * worldPos;
}