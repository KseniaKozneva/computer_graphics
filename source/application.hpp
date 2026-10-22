#pragma once
#include "graphics_internal.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace application {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 color;
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
    alignas(16) glm::vec3 albedo;
    alignas(16) glm::vec3 specularColor;
    float shininess;
};

struct VisualizerUBO {
    alignas(16) glm::mat4 mvp;
    alignas(16) glm::vec3 color;
};

struct ObjectState {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    glm::vec3 albedo = {1.0f, 0.75f, 0.45f};
    glm::vec3 specularColor = {1.0f, 1.0f, 1.0f};
    float shininess = 32.0f;
    bool useTrajectory = false;
};

struct AppState {
    glm::vec3 cameraPos = {0.0f, 0.0f, 5.0f};
    glm::vec3 cameraTarget = {0.0f, 0.0f, 0.0f};
    int projectionType = 0;
    float fov = 50.0f;

    glm::vec3 dirLightDir = {-0.2f, -1.0f, -0.3f};
    glm::vec3 dirLightColor = {1.0f, 1.0f, 1.0f};
    float dirLightIntensity = 1.0f;
    glm::vec3 ambientLight = {0.1f, 0.1f, 0.15f};

    std::vector<PointLightData> pointLights;
    std::vector<SpotLightData> spotLights;

    ObjectState obj1 = { {-1.45f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {1.0f, 0.75f, 0.45f}, {1.0f, 1.0f, 1.0f}, 32.0f, false };
    ObjectState obj2 = { { 1.45f, 0.0f, 0.0f}, {-22.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {0.5f, 0.85f, 1.0f}, {1.0f, 1.0f, 1.0f}, 32.0f, true };

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

} // namespace application