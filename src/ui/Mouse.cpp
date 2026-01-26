#include "ui/Mouse.hpp"

namespace vkui {

MouseState GetMouseState()
{
    MouseState state{};
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::GetCurrentContext() == nullptr) {
        return state;
    }

    state.position = io.MousePos;
    state.scroll = io.MouseWheelH == 0.0f && io.MouseWheel == 0.0f ? ImVec2{0.0f, 0.0f} : ImVec2{io.MouseWheelH, io.MouseWheel};
    state.wantCaptureMouse = io.WantCaptureMouse;
    state.hasMouse = io.MousePos.x != -FLT_MAX && io.MousePos.y != -FLT_MAX;

    static ImVec2 lastPos = state.position;
    if (state.hasMouse) {
        state.delta = ImVec2{state.position.x - lastPos.x, state.position.y - lastPos.y};
        lastPos = state.position;
    }

    for (int i = 0; i < 5; ++i) {
        state.buttons[i] = io.MouseDown[i];
    }

    return state;
}

} // namespace vkui
