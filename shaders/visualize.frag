#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Просто выводим цвет источника, без освещения
    outColor = vec4(fragColor, 1.0);
}