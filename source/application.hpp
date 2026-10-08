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
    alignas(16) glm::mat4 proj; //Может быть перспективной или ортографической
    alignas(16) glm::vec4 uiColor; //множитель, передаваемый во фрагментный шейдер.
};


struct ObjectState {
    glm::vec3 position = {0.0f, 0.0f, 0.0f}; //Позиция объекта в мировом пространстве
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f}; //Поворот в градусах вокруг осей X, Y, Z
    glm::vec3 scale = {1.0f, 1.0f, 1.0f}; //Масштаб по осям
    glm::vec3 color = {1.0f, 1.0f, 1.0f}; // RGB цвет
    bool useTrajectory = false;
};


struct AppState {
    int projectionType = 0; //Тип проекции: 0 — перспективная, 1 — ортографическая
    float fov = 50.0f; // Field of View — угол обзора камеры в градусах
    
    ObjectState obj1 = { {-1.45f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {1.0f, 0.75f, 0.45f}, false };
    ObjectState obj2 = { { 1.45f, 0.0f, 0.0f}, {-22.0f, 0.0f, 0.0f}, {0.65f, 0.65f, 0.65f}, {0.5f, 0.85f, 1.0f}, true };
    
    bool isPaused = false;
    float animSpeed = 0.88f;
    float animRadius = 0.35f; //Радиус орбиты — насколько далеко объект отходит от центра.
    float animHeight = 0.30f;
    float animTime = 0.0f;
    
    bool showDemoWindow = false;
};

bool initialize();
void shutdown();
void update(double time, double deltaTime);
void render(const graphics::internal::FrameData& fd);

} // namespace application