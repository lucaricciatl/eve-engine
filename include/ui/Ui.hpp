#pragma once

#include <imgui.h>
#include <string>

namespace vkui {

// RAII helper to ensure EndWindow is always called when BeginWindow succeeds.
class WindowGuard {
public:
    explicit WindowGuard(bool active) : active_(active) {}
    ~WindowGuard();
    WindowGuard(const WindowGuard&) = delete;
    WindowGuard& operator=(const WindowGuard&) = delete;
    WindowGuard(WindowGuard&&) = delete;
    WindowGuard& operator=(WindowGuard&&) = delete;
private:
    bool active_{false};
};

// Window helpers
bool BeginWindow(const char* title, bool* open = nullptr, ImGuiWindowFlags flags = 0);
void EndWindow();

// Basic widgets
bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
bool Checkbox(const char* label, bool* v);
bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
void TextUnformatted(const char* text);
void Text(const std::string& text);
void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
void Separator();
void Spacing();

} // namespace vkui
