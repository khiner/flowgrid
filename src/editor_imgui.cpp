#include "editor_imgui.h"

namespace Zep {

bool ZepEditor_ImGui::sendImGuiKeyPressToBuffer(ImGuiKey imGuiKey, uint32_t key, uint32_t mod) {
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(imGuiKey))) {
        const auto *buffer = activeTabWindow->GetActiveWindow()->buffer;
        buffer->GetMode()->AddKeyPress(key, mod);
        return true;
    }
    return false;
}

void ZepEditor_ImGui::handleMouseEventAndHideFromImGui(size_t mouseButtonIndex, ZepMouseButton zepMouseButton, bool down) {
    auto &io = ImGui::GetIO();
    if (down) {
        if (io.MouseClicked[mouseButtonIndex] && OnMouseDown(toNVec2f(io.MousePos), zepMouseButton)) io.MouseClicked[mouseButtonIndex] = false;
    }
    if (io.MouseReleased[mouseButtonIndex] && OnMouseUp(toNVec2f(io.MousePos), zepMouseButton)) io.MouseReleased[mouseButtonIndex] = false;
}

}
