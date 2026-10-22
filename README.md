# 🌋 CG Lab #2 — Вариант 3: Параллелепипед (Камера и освещение)

Лабораторная по компьютерной графике: динамическая камера, перспективная и ортографическая проекции, освещение по модели Блинн-Фонга, направленный свет, массивы точечных и прожекторных источников через Storage-буферы, визуализация источников света и интерфейс на ImGui.

## Требования для сборки

- C++ компилятор (GCC 10+, Clang 10+, MSVC 2019+).
- Vulkan SDK.
- CMake 3.20+.
- C++20.

## Сборка

    # Конфигурация (Windows + Visual Studio)
    cmake --preset msvc-debug

    # Сборка
    cmake --build build-debug --config Debug --parallel

Пресеты: `debug`, `release`, `msvc-debug`, `msvc-release`, `mingw-debug`, `mingw-release`.

**Важно:** у всех `*-debug` пресетов одна директория — `build-debug`. При смене пресета удалить её:

    Remove-Item -Recurse -Force build-debug   # Windows
    rm -rf build-debug                         # Linux

### Компиляция шейдеров
Шейдеры компилируются автоматически через `glslc`. Если его нет — CMake выдаст предупреждение, и `.spv` не создадутся. Тогда скомпилировать вручную:

    glslc shaders/shader.vert -o shaders/shader.vert.spv
    glslc shaders/shader.frag -o shaders/shader.frag.spv
    glslc shaders/visualize.vert -o shaders/visualize.vert.spv
    glslc shaders/visualize.frag -o shaders/visualize.frag.spv

## Запуск

Рабочая директория должна быть **корнем проекта** — иначе не найдутся шейдеры (`shaders/*.spv`).

    # Windows (Visual Studio)
    .\build-debug\Debug\vulkan-starter-app.exe

    # Linux
    ./build-debug/vulkan-starter-app/vulkan-starter-app
