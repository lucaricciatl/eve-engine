#pragma once

#include <imgui.h>
#include <vector>

namespace vkui {

struct Color {
    float r{1.0f}, g{1.0f}, b{1.0f}, a{1.0f};
    ImU32 toU32() const { return ImGui::GetColorU32(ImVec4{r, g, b, a}); }
};

struct DrawOptions {
    bool inForeground{false}; // true uses foreground draw list, otherwise background
};

// Draw a line between two points
void DrawLine(const ImVec2& p1, const ImVec2& p2, const Color& color, float thickness = 1.0f, const DrawOptions& opts = {});

// Draw multiple connected segments
void DrawPolyline(const std::vector<ImVec2>& points, const Color& color, bool closed = false, float thickness = 1.0f, const DrawOptions& opts = {});

// Filled convex polygon
void DrawPolygonFilled(const std::vector<ImVec2>& points, const Color& color, const DrawOptions& opts = {});

// Axis-aligned rectangle helpers
void DrawRect(const ImVec2& min, const ImVec2& max, const Color& color, float rounding = 0.0f, float thickness = 1.0f, const DrawOptions& opts = {});
void DrawRectFilled(const ImVec2& min, const ImVec2& max, const Color& color, float rounding = 0.0f, const DrawOptions& opts = {});

// Draw text at the given screen position using the current ImGui font and size.
void DrawText(const ImVec2& pos, const char* text, const Color& color, const DrawOptions& opts = {});

} // namespace vkui
