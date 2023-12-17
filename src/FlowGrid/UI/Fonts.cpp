#include "Fonts.h"

#include "Project/FileDialog/FileDialog.h" // xxx only used for loading fonts

#include "imgui.h"

void Fonts::Init() {
    auto &io = ImGui::GetIO();
    Main = io.Fonts->AddFontFromFileTTF("../res/fonts/AbletonSansMedium.otf", 16 * AtlasScale);
    FixedWidth = io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/Cousine-Regular.ttf", 15 * AtlasScale);
    io.Fonts->AddFontFromFileTTF("../lib/imgui/misc/fonts/ProggyClean.ttf", 14 * AtlasScale);
    IGFD::AddFonts();
}
