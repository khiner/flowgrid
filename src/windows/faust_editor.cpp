/**
 * Based on https://github.com/cmaughan/zep_imgui/blob/main/demo/src/editor.cpp
 */

#include "faust_editor.h"
#include "../config.h"

using namespace Zep;

// Initialize the editor and watch for changes
void FaustEditor::zep_init(const Zep::NVec2f &pixelScale) {
    spZep = std::make_unique<ZepWrapper>(
        config.app_root,
        Zep::NVec2f(pixelScale.x, pixelScale.y),
        [](const std::shared_ptr<ZepMessage> &_) -> void {}
    );

    auto &display = spZep->GetEditor().GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pImFont->FontSize * 1.125)));
}

void FaustEditor::zep_load(const Zep::ZepPath &file) {
    auto pBuffer = spZep->GetEditor().InitWithFileOrDir(file);
}

void FaustEditor::show() {
    if (!initialized) {
        // Called once after the fonts are initialized
        zep_init(Zep::NVec2f(1.0f, 1.0f));
        zep_load(Zep::ZepPath(config.app_root) / "src" / "main.cpp");
        initialized = true;
    }

    spZep->GetEditor().RefreshRequired(); // Required for CTRL+P and flashing cursor.

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    if (max.x <= 0) max.x = 1;
    if (max.y <= 0) max.y = 1;
    ImGui::InvisibleButton("ZepContainer", max);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;

    spZep->zepEditor.SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));
    spZep->zepEditor.Display();
    if (ImGui::IsWindowFocused()) spZep->zepEditor.HandleInput();
}

void FaustEditor::destroy() {
    spZep.reset();
}
