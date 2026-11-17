# 🌋 CG Lab #3 — Текстурирование (Вариант 3: Параллелепипед)


## Требования для сборки

- C++ компилятор (GCC 10+, Clang 10+, MSVC 2019+), C++20.
- Vulkan SDK.
- CMake 3.20+.

## Сборка

```bash
# Конфигурация (Windows + Visual Studio)
cmake --preset msvc-debug

# Сборка
cmake --build build-debug --config Debug --parallel
```

Пресеты: `debug`, `release`, `msvc-debug`, `msvc-release`, `mingw-debug`, `mingw-release`.

У всех `*-debug` пресетов одна директория - `build-debug`. При смене пресета удалить её:

```bash
Remove-Item -Recurse -Force build-debug   # Windows
rm -rf build-debug                        # Linux
```

## Компиляция шейдеров

Шейдеры компилируются автоматически через `glslc`. Если его нет - CMake выдаст
предупреждение, и `.spv` не создадутся. Тогда скомпилировать вручную:

```bash
glslc shaders/shader.vert -o shaders/shader.vert.spv
glslc shaders/shader.frag -o shaders/shader.frag.spv
glslc shaders/visualize.vert -o shaders/visualize.vert.spv
glslc shaders/visualize.frag -o shaders/visualize.frag.spv
```

## Запуск

Рабочая директория должна быть **корнем проекта** - иначе не найдутся `shaders/*.spv`
и `textures/`. При запуске из Visual Studio рабочая директория уже настроена
(`VS_DEBUGGER_WORKING_DIRECTORY`).

```bash
# Windows (Visual Studio)
.\build-debug\Debug\vulkan-starter-app.exe

# Linux
./build-debug/vulkan-starter-app/vulkan-starter-app
```

## Управление

В окне ImGui:
- **Camera** — позиция, цель, тип проекции, FOV.
- **Lights** — направленный свет, ambient, добавление/удаление точечных и прожекторных
  источников, настройка затухания и углов конуса.
- **Объекты** — выбор текстуры (Texture), позиция/поворот/масштаб, оттенки альбедо и
  блика, shininess, UV Scale (тайлинг), Trajectory (облёт по траектории),
  **Fancy Sampling** — включение нетривиального сэмплирования (push-константа).
- **Pause / Reset Time** — остановка и сброс анимации (время передаётся в шейдер
  через push-константы, при паузе замирает и траектория, и сэмплирование).