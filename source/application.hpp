#pragma once
#include "graphics_internal.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

namespace application {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct SceneUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
    alignas(16) glm::vec3 dirLightDir;
    alignas(16) glm::vec3 dirLightColor;
    alignas(16) glm::vec3 ambientColor;
    float dirLightIntensity;
    int pointLightCount;
    int spotLightCount;
};

struct PointLightData {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLightData {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
    float intensity;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};

struct ModelUBO {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 albedoTint;    
    float shininess;
    alignas(16) glm::vec3 specularTint;
    float uvScale;                    
};


struct PushConstants {
    int32_t fancySampling;
    float time;
};

struct VisualizerUBO {
    alignas(16) glm::mat4 mvp;
    alignas(16) glm::vec3 color;
};

struct ObjectState {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    glm::vec3 albedoTint = {1.0f, 1.0f, 1.0f};
    glm::vec3 specularTint = {1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;
    float uvScale = 1.0f;
    int materialIndex = 0;     
    bool useTrajectory = false;
    bool fancySampling = false; 
};

struct AppState {
    glm::vec3 cameraPos = {0.0f, 1.8f, 6.5f};
    glm::vec3 cameraTarget = {0.0f, 0.0f, 0.0f};
    int projectionType = 0;
    float fov = 50.0f;

    glm::vec3 dirLightDir = {-0.2f, -1.0f, -0.3f};
    glm::vec3 dirLightColor = {1.0f, 1.0f, 1.0f};
    float dirLightIntensity = 1.0f;
    glm::vec3 ambientLight = {0.1f, 0.1f, 0.15f};

    std::vector<PointLightData> pointLights;
    std::vector<SpotLightData> spotLights;
    std::vector<ObjectState> objects;

    bool showLightVisualizers = true;
    bool isPaused = false;
    float animSpeed = 0.88f;
    float animRadius = 0.35f;
    float animHeight = 0.30f;
    float animTime = 0.0f;
};

bool initialize();
void shutdown();
void update(double time, double deltaTime);
void render(const graphics::internal::FrameData& fd);

} 