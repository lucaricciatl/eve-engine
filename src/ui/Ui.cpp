#include "ui/Ui.hpp"

namespace vkui {

WindowGuard::~WindowGuard()
{
    if (active_) {
        ImGui::End();
    }
}

static bool HasContext()
{
    return ImGui::GetCurrentContext() != nullptr;
}

bool BeginWindow(const char* title, bool* open, ImGuiWindowFlags flags)
{
    if (!HasContext()) {
        return false;
    }
    return ImGui::Begin(title, open, flags);
}

void EndWindow()
{
    if (!HasContext()) {
        return;
    }
    ImGui::End();
}

bool Button(const char* label, const ImVec2& size)
{
    if (!HasContext()) {
        return false;
    }
    return ImGui::Button(label, size);
}

bool Checkbox(const char* label, bool* v)
{
    if (!HasContext()) {
        return false;
    }
    return ImGui::Checkbox(label, v);
}

bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    if (!HasContext()) {
        return false;
    }
    return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
}

bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    if (!HasContext()) {
        return false;
    }
    return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
}

void TextUnformatted(const char* text)
{
    if (!HasContext()) {
        return;
    }
    ImGui::TextUnformatted(text);
}

void Text(const std::string& text)
{
    TextUnformatted(text.c_str());
}

void SameLine(float offset_from_start_x, float spacing)
{
    if (!HasContext()) {
        return;
    }
    ImGui::SameLine(offset_from_start_x, spacing);
}

void Separator()
{
    if (!HasContext()) {
        return;
    }
    ImGui::Separator();
}

void Spacing()
{
    if (!HasContext()) {
        return;
    }
    ImGui::Spacing();
}

} // namespace vkui
