#include "Fonts.h"

#include <filesystem>
#include <string>
#include <unordered_map>

#include "imgui.h"

namespace fs = std::filesystem;

static ImFont *AddFont(FontFamily family, const std::string_view font_file) {
    static const auto FontsPath = fs::path("./") / "res" / "fonts";
    // These are eyeballed.
    static const std::unordered_map<FontFamily, u32> PixelsForFamily{
        {FontFamily::Main, 15},
        {FontFamily::Monospace, 17},
    };
    return ImGui::GetIO().Fonts->AddFontFromFileTTF((FontsPath / font_file).c_str(), PixelsForFamily.at(family) * Fonts::AtlasScale);
}

void Fonts::Init(float scale) {
    Main = AddFont(FontFamily::Main, "Inter-Regular.ttf");
    MainBold = AddFont(FontFamily::Main, "Inter-Bold.ttf");
    MainItalic = AddFont(FontFamily::Main, "Inter-Italic.ttf");
    MainBoldItalic = AddFont(FontFamily::Main, "Inter-BoldItalic.ttf");

    Monospace = AddFont(FontFamily::Monospace, "JetBrainsMono-Regular.ttf");
    MonospaceBold = AddFont(FontFamily::Monospace, "JetBrainsMono-Bold.ttf");
    MonospaceItalic = AddFont(FontFamily::Monospace, "JetBrainsMono-Italic.ttf");
    MonospaceBoldItalic = AddFont(FontFamily::Monospace, "JetBrainsMono-BoldItalic.ttf");

    ImGui::GetIO().FontGlobalScale = scale / AtlasScale;
}

void Fonts::Tick(float scale, u32 index) {
    static float PrevScale = 1.0;
    if (PrevScale != scale) {
        ImGui::GetIO().FontGlobalScale = scale / Fonts::AtlasScale;
        PrevScale = scale;
    }
    static int PrevIndex = 0;
    if (PrevIndex != index) {
        ImGui::GetIO().FontDefault = ImGui::GetIO().Fonts->Fonts[index];
        PrevIndex = index;
    }
}

bool Fonts::Push(FontFamily family, FontStyle style) {
    const auto *new_font = Get(family, style);
    if (ImGui::GetFont() == new_font) return false;

    ImGui::PushFont(Get(family, style));
    return true;
}
void Fonts::Pop() { ImGui::PopFont(); }
