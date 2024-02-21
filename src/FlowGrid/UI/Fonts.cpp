#include "Fonts.h"

#include "Project/FileDialog/FileDialogImpl.h"

#include "imgui.h"

void Fonts::Init() {
    const auto &io = ImGui::GetIO();
    Main = io.Fonts->AddFontFromFileTTF("res/fonts/Inter-Regular.ttf", 15 * AtlasScale);
    Monospace = io.Fonts->AddFontFromFileTTF("res/fonts/JetBrainsMono-Regular.ttf", 17 * AtlasScale);
    FileDialogImp.AddFonts();
}
