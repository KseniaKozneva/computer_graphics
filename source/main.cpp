#include <cstdint>
#include <iostream>
#include <exception>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "graphics_internal.hpp"
#include "application.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {

constexpr int32_t default_window_width  = 1280;
constexpr int32_t default_window_height = 720;
constexpr char    default_window_title[] = "Vulkan Lab 4 - Shadow Mapping";

#ifdef _WIN32
// Молчаливый access violation больше не будет молчаливым.
LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep) {
    std::cerr << "[main] FATAL: SEH code=0x" << std::hex
              << static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode)
              << " at address " << ep->ExceptionRecord->ExceptionAddress << std::dec << "\n";
    std::cerr.flush();
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

#ifdef _WIN32
    SetUnhandledExceptionFilter(crashFilter);
#endif

    try {
        std::cout << "[main] glfwInit...\n";
        if (!glfwInit()) {
            std::cerr << "[main] glfwInit failed\n";
            return EXIT_FAILURE;
        }

        GLFWwindow* glfw_window = nullptr;
        bool imgui_context_created  = false;
        bool imgui_glfw_initialized = false;
        bool graphics_initialized   = false;
        bool app_initialized        = false;

        auto cleanup = [&]() {
            if (app_initialized)        application::shutdown();
            if (graphics_initialized)   graphics::internal::shutdown();
            if (imgui_glfw_initialized) ImGui_ImplGlfw_Shutdown();
            if (imgui_context_created)  ImGui::DestroyContext();
            if (glfw_window)            glfwDestroyWindow(glfw_window);
            glfwTerminate();
        };

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfw_window = glfwCreateWindow(default_window_width, default_window_height,
                                       default_window_title, nullptr, nullptr);
        if (!glfw_window) {
            std::cerr << "[main] glfwCreateWindow failed\n";
            cleanup();
            return EXIT_FAILURE;
        }

        glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow*, int width, int height) {
            graphics::internal::resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        });

        std::cout << "[main] ImGui::CreateContext...\n";
        if (!ImGui::CreateContext()) {
            std::cerr << "[main] ImGui::CreateContext failed\n";
            cleanup();
            return EXIT_FAILURE;
        }
        imgui_context_created = true;
        ImGui::GetIO().IniFilename = "imgui.ini";

        std::cout << "[main] ImGui_ImplGlfw_InitForVulkan...\n";
        if (!ImGui_ImplGlfw_InitForVulkan(glfw_window, true)) {
            std::cerr << "[main] ImGui_ImplGlfw_InitForVulkan failed\n";
            cleanup();
            return EXIT_FAILURE;
        }
        imgui_glfw_initialized = true;

        std::cout << "[main] graphics::internal::initialize...\n";
        if (!graphics::internal::initialize(glfw_window)) {
            std::cerr << "[main] graphics initialize failed\n";
            cleanup();
            return EXIT_FAILURE;
        }
        graphics_initialized = true;

        std::cout << "[main] application::initialize...\n";
        if (!application::initialize()) {
            std::cerr << "[main] application initialize failed\n";
            cleanup();
            return EXIT_FAILURE;
        }
        app_initialized = true;

        std::cout << "[main] entering main loop\n";
        double   lastTime   = glfwGetTime();
        uint64_t frameIndex = 0;

        while (!glfwWindowShouldClose(glfw_window)) {
            // Свернутое окно: не рендерим, спим на событиях.
            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(glfw_window, &fbw, &fbh);
            if (fbw == 0 || fbh == 0) {
                glfwWaitEvents();
                lastTime = glfwGetTime();
                continue;
            }

            const double currentTime = glfwGetTime();
            const double deltaTime   = currentTime - lastTime;
            lastTime = currentTime;

            glfwPollEvents();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            application::update(currentTime, deltaTime);
            ImGui::Render();

            const graphics::internal::FrameData fd = graphics::internal::prepare();
            if (!fd.valid) {
                continue; // swapchain пересоздаётся / acquire не удался
            }

            application::render(fd);
            graphics::internal::submitAndPresent();

            if (frameIndex < 3) std::cout << "[main] frame " << frameIndex << " done\n";
            ++frameIndex;
        }

        std::cout << "[main] exiting main loop, frames=" << frameIndex << "\n";
        cleanup();
        std::cout << "[main] done\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "[main] EXCEPTION: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "[main] UNKNOWN EXCEPTION\n";
        return EXIT_FAILURE;
    }
}