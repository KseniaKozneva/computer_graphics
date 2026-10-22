cmake_minimum_required(VERSION 3.20)
project(vulkan-starter-app LANGUAGES C CXX)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME}
    source/main.cpp
    source/graphics_internal.cpp
    source/application.cpp
)

# Настройки стандарта C++
set_target_properties(${PROJECT_NAME} PROPERTIES 
    CXX_STANDARD 20 
    CXX_STANDARD_REQUIRED TRUE
)

# Настройки компилятора для разных платформ
if(LINUX OR APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(WIN32)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4201)
        # /subsystem:windows убирает консольное окно при запуске. 
        target_link_options(${PROJECT_NAME} PRIVATE /subsystem:console) 
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
        target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE -D_USE_MATH_DEFINES)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

# Подключение зависимостей через FetchContent
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
)

FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG v1.4.321
)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.4.0
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
)


FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

find_package(Vulkan REQUIRED)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

FetchContent_MakeAvailable(
    glfw
    vk-bootstrap
    VulkanMemoryAllocator
    imgui
    glm 
)

# Линковка библиотек
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glm::glm 
)

# Include директории для ImGui
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# Исходные файлы ImGui
target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

# Компиляция шейдеров
# Ищем glslc сначала в PATH, потом в переменной окружения VULKAN_SDK
find_program(GLSLC_EXECUTABLE glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)

if(GLSLC_EXECUTABLE)
    set(_SHADER_BINARIES)
    
    macro(compile_shader SHADER_FILE)
        set(SHADER_SOURCE ${CMAKE_SOURCE_DIR}/shaders/${SHADER_FILE})
        set(SHADER_BINARY ${SHADER_FILE}.spv)
        set(SHADER_BINARY_PATH ${CMAKE_SOURCE_DIR}/shaders/${SHADER_BINARY})
        
        add_custom_command(
            OUTPUT ${SHADER_BINARY_PATH}
            COMMAND ${GLSLC_EXECUTABLE} ${SHADER_SOURCE} -o ${SHADER_BINARY_PATH}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_FILE} shader to ${SHADER_BINARY}"
        )
        list(APPEND _SHADER_BINARIES ${SHADER_BINARY_PATH})
    endmacro()

    compile_shader(shader.vert)
    compile_shader(shader.frag)
    compile_shader(visualize.vert)
    compile_shader(visualize.frag)

    add_custom_target(shaders DEPENDS ${_SHADER_BINARIES})
    add_dependencies(${PROJECT_NAME} shaders)
else()
    message(WARNING "glslc not found! Shaders will not be compiled automatically. Please compile them manually or check your VULKAN_SDK environment variable.")
endif()

# Специфичные настройки для MinGW 
if (MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-stack-protector)
    target_link_options(${PROJECT_NAME} PRIVATE -lucrt)
endif()