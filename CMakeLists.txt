cmake_minimum_required(VERSION 3.20)
project(vulkan-starter-app LANGUAGES C CXX)

add_executable(${PROJECT_NAME}
    source/main.cpp
    source/graphics_internal.cpp
    source/application.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Vulkan требует глубину [0,1]; glm по умолчанию делает OpenGL-шный [-1,1].
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    NOMINMAX
)

if(WIN32)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8 /wd4201 /wd4324)
        target_link_options(${PROJECT_NAME} PRIVATE /subsystem:console)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
        target_link_options(${PROJECT_NAME} PRIVATE -mconsole)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
elseif(UNIX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

include(FetchContent)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.5.1)
FetchContent_Declare(vk-bootstrap GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git GIT_TAG v1.4.321)
FetchContent_Declare(VulkanMemoryAllocator GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git GIT_TAG v3.4.0)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1)
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG master GIT_SHALLOW TRUE)

find_package(Vulkan REQUIRED)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw vk-bootstrap VulkanMemoryAllocator imgui glm stb)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glm::glm
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${stb_SOURCE_DIR})

target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

find_program(GLSLC_EXECUTABLE glslc HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/bin)
if(GLSLC_EXECUTABLE)
    set(_SHADER_BINARIES)
    macro(compile_shader SHADER_FILE)
        set(SHADER_SOURCE ${CMAKE_SOURCE_DIR}/shaders/${SHADER_FILE})
        set(SHADER_BINARY_PATH ${CMAKE_SOURCE_DIR}/shaders/${SHADER_FILE}.spv)
        add_custom_command(
            OUTPUT ${SHADER_BINARY_PATH}
            COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.2 ${SHADER_SOURCE} -o ${SHADER_BINARY_PATH}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_FILE} -> ${SHADER_FILE}.spv"
        )
        list(APPEND _SHADER_BINARIES ${SHADER_BINARY_PATH})
    endmacro()

    compile_shader(shader.vert)
    compile_shader(shader.frag)
    compile_shader(visualize.vert)
    compile_shader(visualize.frag)
    compile_shader(line.vert)
    compile_shader(shadow.vert)

    add_custom_target(shaders DEPENDS ${_SHADER_BINARIES})
    add_dependencies(${PROJECT_NAME} shaders)
else()
    message(WARNING "glslc not found! Shaders will not be compiled automatically.")
endif()