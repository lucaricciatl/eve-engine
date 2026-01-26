#include "core/WindowManager.hpp"

#include <algorithm>
#include <stdexcept>

namespace vkcore {

namespace {
constexpr int toGlfwBool(bool value) noexcept { return value ? GLFW_TRUE : GLFW_FALSE; }
}

WindowManager::~WindowManager()
{
    destroy();
}

void WindowManager::applyHints() const
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const bool fixedSize = config.mode == WindowMode::FixedSize;
    glfwWindowHint(GLFW_RESIZABLE, toGlfwBool(!fixedSize && config.resizable));
    glfwWindowHint(GLFW_DECORATED, toGlfwBool(config.mode != WindowMode::Borderless));
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, toGlfwBool(config.transparentFramebuffer));
    glfwWindowHint(GLFW_VISIBLE, toGlfwBool(!config.headless));
}

GLFWmonitor* WindowManager::chooseMonitor(WindowMode mode) const
{
    if (mode != WindowMode::Fullscreen) {
        return nullptr;
    }
    return glfwGetPrimaryMonitor();
}

GLFWwindow* WindowManager::createWindow()
{
    if (!glfwInitialized) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwInitialized = true;
    }

    applyHints();

    GLFWmonitor* monitor = chooseMonitor(config.mode);
    int width = static_cast<int>(config.width);
    int height = static_cast<int>(config.height);

    if (monitor != nullptr) {
        if (const GLFWvidmode* videoMode = glfwGetVideoMode(monitor)) {
            width = videoMode->width;
            height = videoMode->height;
        }
    }

    window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
    if (!window) {
        destroy();
        throw std::runtime_error("Failed to create GLFW window");
    }

    setOpacity(config.opacity);

    captureWindowedRect();

    return window;
}

void WindowManager::destroy()
{
    if (window != nullptr) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    if (glfwInitialized) {
        glfwTerminate();
        glfwInitialized = false;
    }
}

void WindowManager::captureWindowedRect()
{
    if (window == nullptr) {
        return;
    }
    glfwGetWindowPos(window, &savedX, &savedY);
    glfwGetWindowSize(window, &savedWidth, &savedHeight);
    hasSavedWindowedRect = true;
}

void WindowManager::setMode(WindowMode mode)
{
    if (config.mode == mode) {
        return;
    }

    config.mode = mode;

    if (window == nullptr) {
        return;
    }

    if (mode == WindowMode::Fullscreen) {
        captureWindowedRect();
        GLFWmonitor* monitor = chooseMonitor(mode);
        const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
        if (videoMode != nullptr) {
            glfwSetWindowMonitor(window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
        }
        return;
    }

    // Switching to a windowed variant
    int width = static_cast<int>(config.width);
    int height = static_cast<int>(config.height);
    int posX = 100;
    int posY = 100;

    if (hasSavedWindowedRect) {
        posX = savedX;
        posY = savedY;
        width = savedWidth;
        height = savedHeight;
    }

    glfwSetWindowMonitor(window, nullptr, posX, posY, width, height, 0);

    const bool fixedSize = mode == WindowMode::FixedSize;
    glfwSetWindowAttrib(window, GLFW_DECORATED, mode != WindowMode::Borderless ? GLFW_TRUE : GLFW_FALSE);
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, (!fixedSize && config.resizable) ? GLFW_TRUE : GLFW_FALSE);
}

bool WindowManager::shouldClose() const
{
    if (window == nullptr) {
        return true;
    }
    return glfwWindowShouldClose(window) == GLFW_TRUE;
}

void WindowManager::requestClose() const
{
    if (window != nullptr) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void WindowManager::pollEvents() const
{
    glfwPollEvents();
}

void WindowManager::setOpacity(float value)
{
    config.opacity = std::clamp(value, 0.0f, 1.0f);
    if (window != nullptr) {
        glfwSetWindowOpacity(window, config.opacity);
    }
}

} // namespace vkcore
