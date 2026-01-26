#pragma once

#include <imgui.h>

namespace vkui {

struct MouseState {
    ImVec2 position{0.0f, 0.0f};
    ImVec2 delta{0.0f, 0.0f};
    ImVec2 scroll{0.0f, 0.0f};
    bool buttons[5]{false, false, false, false, false};
    bool wantCaptureMouse{false};
    bool hasMouse{false};
};

// Query current mouse state based on ImGui IO.
MouseState GetMouseState();

} // namespace vkui
