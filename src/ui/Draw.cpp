#include "ui/Draw.hpp"

namespace vkui {

static ImDrawList* targetList(bool foreground)
{
    if (ImGui::GetCurrentContext() == nullptr) {
        return nullptr;
    }
    return foreground ? ImGui::GetForegroundDrawList() : ImGui::GetBackgroundDrawList();
}

void DrawLine(const ImVec2& p1, const ImVec2& p2, const Color& color, float thickness, const DrawOptions& opts)
{
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddLine(p1, p2, color.toU32(), thickness);
    }
}

void DrawPolyline(const std::vector<ImVec2>& points, const Color& color, bool closed, float thickness, const DrawOptions& opts)
{
    if (points.size() < 2) {
        return;
    }
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddPolyline(points.data(), static_cast<int>(points.size()), color.toU32(), closed ? ImDrawFlags_Closed : 0, thickness);
    }
}

void DrawPolygonFilled(const std::vector<ImVec2>& points, const Color& color, const DrawOptions& opts)
{
    if (points.size() < 3) {
        return;
    }
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), color.toU32());
    }
}

void DrawRect(const ImVec2& min, const ImVec2& max, const Color& color, float rounding, float thickness, const DrawOptions& opts)
{
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddRect(min, max, color.toU32(), rounding, 0, thickness);
    }
}

void DrawRectFilled(const ImVec2& min, const ImVec2& max, const Color& color, float rounding, const DrawOptions& opts)
{
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddRectFilled(min, max, color.toU32(), rounding);
    }
}

void DrawText(const ImVec2& pos, const char* text, const Color& color, const DrawOptions& opts)
{
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    if (auto* dl = targetList(opts.inForeground)) {
        dl->AddText(pos, color.toU32(), text);
    }
}

} // namespace vkui
