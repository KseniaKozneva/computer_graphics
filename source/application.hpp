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

struct AppState {
    bool isPerspective = true;
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
    
    bool isAnimating = true;
    float animSpeed = 1.0f;
    float animRadius = 1.5f;
    float animTime = 0.0f;
    
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

bool initialize();
void shutdown();
void update(double time, double deltaTime);
void render(const graphics::internal::FrameData& fd);

} // namespace application