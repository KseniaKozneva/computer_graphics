# 🌋 CG Lab #1 — Вариант 3: Параллелепипед

Лабораторная по компьютерной графике: параллелепипед, перспективная и ортографическая проекции, аффинные преобразования, анимация по траектории, интерфейс на ImGui.

## Требования

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

**Важно:** у всех `*-debug` пресетов одна директория - `build-debug`. При смене пресета удалить её:

    Remove-Item -Recurse -Force build-debug   # Windows
    rm -rf build-debug                         # Linux

Шейдеры компилируются автоматически через `glslc`. Если его нет - CMake выдаст предупреждение, и `.spv` не создадутся. Тогда вручную:

    glslc shaders/shader.vert -o shaders/shader.vert.spv
    glslc shaders/shader.frag -o shaders/shader.frag.spv

## Запуск

Рабочая директория должна быть **корнем проекта** — иначе не найдутся шейдеры (`shaders/*.spv`).

    # Windows (Visual Studio)
    .\build-debug\Debug\vulkan-starter-app.exe

    # Linux
    ./build-debug/vulkan-starter-app/vulkan-starter-app
