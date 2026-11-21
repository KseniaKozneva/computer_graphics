# 🌋 CG Lab #4 — Shadow Mapping (Vulkan)

##  Требования

- **C++20** компилятор (GCC 10+, Clang 10+, MSVC 2019+)
- **Vulkan SDK** (версия 1.3+ для поддержки Dynamic Rendering)
- **CMake** 3.20+
- **Ninja** или **Visual Studio 2022** / **MinGW**

##  Сборка

```bash
# Windows (Visual Studio 2022)
cmake --preset msvc-debug
cmake --build build-debug --config Debug --parallel

# Linux / MinGW (Ninja)
cmake --preset debug
cmake --build build-debug --parallel
```

**Доступные пресеты:** `debug`, `release`, `msvc-debug`, `msvc-release`, `mingw-debug`, `mingw-release`.

>  При смене пресета нужно удалить `build-debug/` или `build-release/`:
> ```powershell
> Remove-Item -Recurse -Force build-debug   # Windows
> rm -rf build-debug                        # Linux
> ```

## Компиляция шейдеров

Шейдеры компилируются **автоматически** через `glslc` при сборке CMake. Если компилятор не найден - скомпилируйте вручную из корня проекта:

```bash
glslc shaders/shader.vert     -o shaders/shader.vert.spv
glslc shaders/shader.frag     -o shaders/shader.frag.spv
glslc shaders/visualize.vert  -o shaders/visualize.vert.spv
glslc shaders/visualize.frag  -o shaders/visualize.frag.spv
glslc shaders/shadow.vert     -o shaders/shadow.vert.spv
```

## 🚀 Запуск

Рабочая директория **должна быть корнем проекта** (иначе не найдутся `shaders/*.spv` и `textures/`). При запуске из Visual Studio рабочая директория настроена автоматически.

```bash
# Windows
.\build-debug\Debug\vulkan-starter-app.exe

# Linux
./build-debug/vulkan-starter-app
```
