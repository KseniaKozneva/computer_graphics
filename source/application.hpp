#pragma once
#include "graphics_internal.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace application {

struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 uiColor;
};


struct ObjectState {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    glm::vec3 color = {1.0f, 1.0f, 1.0f}; // RGB цвет
    bool useTrajectory = false;
};


struct AppState {
    int projectionType = 0; 
    float fov = 50.0f;
    
    ObjectState obj1 = { {-1.45f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {1.0f, 0.75f, 0.45f}, false };
    ObjectState obj2 = { { 1.45f, 0.0f, 0.0f}, {-22.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {0.5f, 0.85f, 1.0f}, true };
    
    bool isPaused = false;
    float animSpeed = 0.88f;
    float animRadius = 0.35f;
    float animHeight = 0.30f;
    float animTime = 0.0f;
    
    bool showDemoWindow = false;
};

bool initialize();
void shutdown();
void update(double time, double deltaTime);
void render(const graphics::internal::FrameData& fd);

} // namespace application