#include "ui/CustomUi.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace vkui {

namespace {
ImVec2 reserve(UiContext& ctx, const ImVec2& size)
{
    ImVec2 pos = ctx.cursor;
    ctx.cursor.y += size.y + ctx.style.spacing;
    return pos;
}

bool isInside(const ImVec2& p, const ImVec2& min, const ImVec2& max)
{
    return p.x >= min.x && p.y >= min.y && p.x <= max.x && p.y <= max.y;
}
} // namespace

UiWindow BeginWindow(ImVec2 pos, ImVec2 size, const char* title, const UiStyle* style)
{
    UiWindow window{};
    window.pos = pos;
    window.size = size;
    window.content.style = style ? *style : UiStyle{};

    const UiStyle& st = window.content.style;
    float titleH = window.content.lineHeight + st.padding * 2.0f;
    ImVec2 min = pos;
    ImVec2 max = ImVec2{pos.x + size.x, pos.y + size.y};

    // Panel and title bar
    DrawRectFilled(min, max, st.bg, st.rounding, {true});
    ImVec2 titleMin = min;
    ImVec2 titleMax = ImVec2{max.x, min.y + titleH};
    DrawRectFilled(titleMin, titleMax, st.bgHover, st.rounding, {true});
    DrawRect(min, max, st.frame, st.rounding, 1.2f, {true});
    if (title) {
        ImVec2 textPos{titleMin.x + st.padding, titleMin.y + st.padding * 0.5f};
        DrawText(textPos, title, st.text, {true});
    }

    // Content area
    window.content.origin = ImVec2{min.x + st.padding, titleMax.y + st.spacing};
    window.content.cursor = window.content.origin;
    window.content.lineHeight = 20.0f;
    return window;
}

void EndWindow(UiWindow& /*window*/)
{
    // No-op for now; reserved for future stacking/clip handling.
}

void Label(UiContext& ctx, const char* text, const Color& color)
{
    if (!text) return;
    ImVec2 size{ImGui::CalcTextSize(text).x + ctx.style.padding * 2.0f, ctx.lineHeight + ctx.style.padding * 2.0f};
    ImVec2 pos = reserve(ctx, size);
    DrawText(ImVec2{pos.x + ctx.style.padding, pos.y + ctx.style.padding}, text, color, {true});
}

bool Button(UiContext& ctx, const char* label, ImVec2 size)
{
    if (!label) return false;
    if (size.y <= 0.0f) size.y = ctx.lineHeight + ctx.style.padding * 2.0f;

    ImVec2 pos = reserve(ctx, size);
    ImVec2 min = pos;
    ImVec2 max = ImVec2{pos.x + size.x, pos.y + size.y};

    MouseState ms = GetMouseState();
    bool hovered = ms.hasMouse && isInside(ms.position, min, max);
    static const void* activeId = nullptr;
    bool pressed = hovered && ms.buttons[0];

    Color fill = ctx.style.bg;
    if (pressed) fill = ctx.style.bgActive;
    else if (hovered) fill = ctx.style.bgHover;

    DrawRectFilled(min, max, fill, ctx.style.rounding, {true});
    DrawRect(min, max, ctx.style.frame, ctx.style.rounding, 1.2f, {true});

    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos{min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.5f};
    DrawText(textPos, label, ctx.style.text, {true});

    bool clicked = false;
    if (pressed && activeId == nullptr) {
        activeId = label; // simplistic id by pointer-to-string
    }
    if (!ms.buttons[0] && activeId == label && hovered) {
        clicked = true;
    }
    if (!ms.buttons[0]) {
        activeId = nullptr;
    }
    return clicked;
}

bool SliderFloat(UiContext& ctx, const char* label, float* value, float minVal, float maxVal, float width)
{
    if (!value || !label) return false;
    float h = ctx.lineHeight + ctx.style.padding * 2.0f;
    ImVec2 size{width, h};
    ImVec2 pos = reserve(ctx, size);
    ImVec2 min = pos;
    ImVec2 max = ImVec2{pos.x + size.x, pos.y + size.y};

    MouseState ms = GetMouseState();
    bool hovered = ms.hasMouse && isInside(ms.position, min, max);
    static const void* activeId = nullptr;

    float t = (*value - minVal) / std::max(0.0001f, (maxVal - minVal));
    t = std::clamp(t, 0.0f, 1.0f);

    if (hovered && ms.buttons[0]) {
        activeId = value;
    }
    if (ms.buttons[0] && activeId == value) {
        float rel = (ms.position.x - min.x) / std::max(1.0f, size.x);
        t = std::clamp(rel, 0.0f, 1.0f);
        *value = std::lerp(minVal, maxVal, t);
    } else if (!ms.buttons[0] && activeId == value) {
        activeId = nullptr;
    }

    Color track = ctx.style.bg;
    Color fill = ctx.style.bgHover;
    DrawRectFilled(min, max, track, ctx.style.rounding, {true});

    ImVec2 fillMax{min.x + size.x * t, max.y};
    DrawRectFilled(min, fillMax, fill, ctx.style.rounding, {true});
    DrawRect(min, max, ctx.style.frame, ctx.style.rounding, 1.1f, {true});

    // Thumb
    float thumbW = 8.0f;
    float thumbX = min.x + size.x * t - thumbW * 0.5f;
    ImVec2 thumbMin{thumbX, min.y};
    ImVec2 thumbMax{thumbX + thumbW, max.y};
    DrawRectFilled(thumbMin, thumbMax, ctx.style.bgActive, ctx.style.rounding, {true});

    // Label and value text
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s: %.2f", label, *value);
    ImVec2 textPos{min.x + ctx.style.padding, min.y + ctx.style.padding * 0.5f};
    DrawText(textPos, buf, ctx.style.text, {true});

    return hovered && ms.buttons[0];
}

} // namespace vkui