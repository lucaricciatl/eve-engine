#pragma once

#include "ui/Draw.hpp"
#include "ui/Mouse.hpp"

#include <functional>

namespace vkui {

struct UiStyle {
    Color bg{0.10f, 0.10f, 0.12f, 0.92f};
    Color bgHover{0.16f, 0.16f, 0.18f, 0.96f};
    Color bgActive{0.24f, 0.24f, 0.27f, 0.98f};
    Color frame{0.45f, 0.45f, 0.50f, 1.0f};
    Color text{0.95f, 0.97f, 1.0f, 1.0f};
    float rounding{6.0f};
    float padding{6.0f};
    float spacing{6.0f};
};

struct UiContext {
    ImVec2 origin{0.0f, 0.0f};
    ImVec2 cursor{0.0f, 0.0f};
    float lineHeight{20.0f};
    UiStyle style{};
};

// Initialize a lightweight immediate-mode UI anchored at origin.
inline void BeginUi(UiContext& ctx, ImVec2 origin, const UiStyle* style = nullptr)
{
    ctx.origin = origin;
    ctx.cursor = origin;
    ctx.lineHeight = 20.0f;
    ctx.style = style ? *style : UiStyle{};
}

// Basic window: draws a panel with title and returns a child context for contents.
struct UiWindow {
    ImVec2 pos{0, 0};
    ImVec2 size{0, 0};
    UiContext content{};
};

UiWindow BeginWindow(ImVec2 pos, ImVec2 size, const char* title, const UiStyle* style = nullptr);
void EndWindow(UiWindow& window);

// Simple label
void Label(UiContext& ctx, const char* text, const Color& color = {1, 1, 1, 1});

// Button: returns true on click release over the button.
bool Button(UiContext& ctx, const char* label, ImVec2 size = ImVec2{140.0f, 32.0f});

// Slider for a float value in [minVal, maxVal]. Returns true when value changes.
bool SliderFloat(UiContext& ctx, const char* label, float* value, float minVal, float maxVal, float width = 180.0f);

} // namespace vkui