#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec4 fragColor;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    vec3 dirLightDir;
    vec3 dirLightColor;
    vec3 ambientColor;
    float dirLightIntensity; // НОВОЕ
    int pointLightCount;
    int spotLightCount;
} scene;

layout(set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec3 albedo;
    vec3 specularColor;
    float shininess;
} modelData;

layout(location = 0) out vec4 outColor;

vec3 calcBlinnPhong(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float intensity, vec3 baseColor) {
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * intensity * baseColor;
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), modelData.shininess);
    vec3 specular = spec * lightColor * intensity * modelData.specularColor;
    return diffuse + specular;
}

void main() {
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(scene.cameraPos - fragPos);
    vec3 baseColor = modelData.albedo * fragColor.rgb;

    vec3 result = scene.ambientColor * baseColor;

    // 1. Направленный свет (с интенсивностью из UBO)
    vec3 dirLightDir = normalize(scene.dirLightDir);
    result += calcBlinnPhong(norm, viewDir, -dirLightDir, scene.dirLightColor, scene.dirLightIntensity, baseColor);

    // 2. Точечные источники
    for (int i = 0; i < scene.pointLightCount; i++) {
        PointLight light = pointLights[i];
        vec3 lightDir = light.position - fragPos;
        float distance = length(lightDir);
        lightDir = normalize(lightDir);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        vec3 lighting = calcBlinnPhong(norm, viewDir, lightDir, light.color, light.intensity, baseColor);
        result += lighting * attenuation;
    }

    // 3. Прожекторы
    for (int i = 0; i < scene.spotLightCount; i++) {
        SpotLight light = spotLights[i];
        vec3 lightDir = normalize(light.position - fragPos);
        float distance = length(light.position - fragPos);
        float theta = dot(lightDir, normalize(-light.direction));
        float epsilon = light.cutOff - light.outerCutOff;
        float intensityFactor = clamp((theta - light.outerCutOff) / max(epsilon, 0.001), 0.0, 1.0);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        vec3 lighting = calcBlinnPhong(norm, viewDir, lightDir, light.color, light.intensity, baseColor);
        result += lighting * intensityFactor * attenuation;
    }

    outColor = vec4(result, 1.0);
}