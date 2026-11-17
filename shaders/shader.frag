#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

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

layout(set = 0, binding = 1) readonly buffer PointLightBuffer { PointLight pointLights[]; };
layout(set = 0, binding = 2) readonly buffer SpotLightBuffer { SpotLight spotLights[]; };

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec3 albedoTint;
    float shininess;
    vec3 specularTint;
    float uvScale;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D albedoTex;
layout(set = 1, binding = 2) uniform sampler2D specularTex;
layout(set = 1, binding = 3) uniform sampler2D emissiveTex;

layout(push_constant) uniform PushConstants {
    int fancySampling;
    float time;
} pc;

layout(location = 0) out vec4 outColor;

vec3 sampleAlbedo() {
    vec3 base = texture(albedoTex, fragUV).rgb;

    if (pc.fancySampling != 0) {
        vec2 uv1 = fragUV + 0.06 * vec2(sin(pc.time * 1.4), cos(pc.time * 1.1));
        vec2 uv2 = fragUV * 1.8 + vec2(pc.time * 0.05, -pc.time * 0.03);
        vec3 s1 = texture(albedoTex, uv1).rgb;
        vec3 s2 = texture(albedoTex, uv2).rgb;
        float k = 0.5 + 0.5 * sin(pc.time * 0.8);
        base = mix(s1, s2, 0.25 + 0.35 * k);

        vec3 modulation = 0.7 + 0.3 * vec3(
            sin(pc.time),
            sin(pc.time + 2.094),
            sin(pc.time + 4.188));
        base *= modulation;
    }
    return base * ubo.albedoTint;
}

vec3 blinnPhong(vec3 L, vec3 radiance, vec3 albedo, vec3 specMap, vec3 N, vec3 V) {
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), ubo.shininess);
    return radiance * (diff * albedo + spec * specMap * ubo.specularTint);
}

void main() {
    vec3 albedo = sampleAlbedo();
    vec3 specMap = texture(specularTex, fragUV).rgb;
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos - fragWorldPos);

    vec3 color = scene.ambientColor * albedo;

    vec3 L = normalize(-scene.dirLightDir);
    color += scene.dirLightIntensity * blinnPhong(L, scene.dirLightColor, albedo, specMap, N, V);

    for (int i = 0; i < scene.pointLightCount; ++i) {
        PointLight pl = pointLights[i];
        vec3 d = pl.position - fragWorldPos;
        float dist = length(d);
        float att = pl.intensity / (pl.constant + pl.linear * dist + pl.quadratic * dist * dist);
        color += att * blinnPhong(normalize(d), pl.color, albedo, specMap, N, V);
    }

    for (int i = 0; i < scene.spotLightCount; ++i) {
        SpotLight sl = spotLights[i];
        vec3 d = sl.position - fragWorldPos;
        float dist = length(d);
        vec3 Ld = normalize(d);
        float theta = dot(Ld, normalize(-sl.direction));
        float eps = max(sl.cutOff - sl.outerCutOff, 1e-4);
        float edge = clamp((theta - sl.outerCutOff) / eps, 0.0, 1.0);
        float att = sl.intensity * edge / (sl.constant + sl.linear * dist + sl.quadratic * dist * dist);
        color += att * blinnPhong(Ld, sl.color, albedo, specMap, N, V);
    }

    vec3 emissive = texture(emissiveTex, fragUV).rgb;
    if (pc.fancySampling != 0) {
        emissive *= 0.65 + 0.35 * sin(pc.time * 2.2);
    }
    color += emissive;

    outColor = vec4(color, 1.0);
}