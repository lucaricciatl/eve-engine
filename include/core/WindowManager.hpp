#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <utility>

namespace vkcore {

enum class WindowMode {
    Windowed,
    Borderless,
    Fullscreen,
    FixedSize
};

struct WindowConfig {
    uint32_t width{1280};
    uint32_t height{720};
    std::string title{"Vulkan Renderer"};
    WindowMode mode{WindowMode::Windowed};
    bool headless{false};
    bool transparentFramebuffer{false};
    bool resizable{true};
    float opacity{1.0f}; // 0.0 transparent, 1.0 opaque
};

class WindowManager {
public:
    WindowManager() = default;
    ~WindowManager();

    void setConfig(WindowConfig cfg) noexcept { config = std::move(cfg); }
    [[nodiscard]] const WindowConfig& getConfig() const noexcept { return config; }

    GLFWwindow* createWindow();
    void destroy();
    void setMode(WindowMode mode);
    void setOpacity(float value);
    [[nodiscard]] GLFWwindow* handle() const noexcept { return window; }
    [[nodiscard]] bool shouldClose() const;
    void requestClose() const;
    void pollEvents() const;

    void setTransparent(bool transparent) noexcept { config.transparentFramebuffer = transparent; }
    void setResizable(bool resizable) noexcept { config.resizable = resizable; }

private:
    void applyHints() const;
    GLFWmonitor* chooseMonitor(WindowMode mode) const;
    void captureWindowedRect();

private:
    WindowConfig config{};
    GLFWwindow* window{nullptr};
    bool glfwInitialized{false};
    int savedX{0};
    int savedY{0};
    int savedWidth{0};
    int savedHeight{0};
    bool hasSavedWindowedRect{false};
};

} // namespace vkcore
