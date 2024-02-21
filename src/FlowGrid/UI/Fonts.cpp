#include "Fonts.h"

#include <filesystem>
#include <string>

#include "imgui.h"

namespace fs = std::filesystem;

static ImFont *AddFont(FontFamily family, const std::string_view font_file) {
    static const auto FontsPath = fs::path("./") / "res" / "fonts";
    static constexpr float MainSizePixels = 15 * Fonts::AtlasScale;
    static constexpr float MonoSizePixels = 17 * Fonts::AtlasScale;

    return ImGui::GetIO().Fonts->AddFontFromFileTTF((FontsPath / font_file).c_str(), family == FontFamily::Main ? MainSizePixels : MonoSizePixels);
}

void Fonts::Init() {
    Main = AddFont(FontFamily::Main, "Inter-Regular.ttf");
    MainBold = AddFont(FontFamily::Main, "Inter-Bold.ttf");
    MainItalic = AddFont(FontFamily::Main, "Inter-Italic.ttf");
    MainBoldItalic = AddFont(FontFamily::Main, "Inter-BoldItalic.ttf");

    Monospace = AddFont(FontFamily::Monospace, "JetBrainsMono-Regular.ttf");
    MonospaceBold = AddFont(FontFamily::Monospace, "JetBrainsMono-Bold.ttf");
    MonospaceItalic = AddFont(FontFamily::Monospace, "JetBrainsMono-Italic.ttf");
    MonospaceBoldItalic = AddFont(FontFamily::Monospace, "JetBrainsMono-BoldItalic.ttf");
}

bool Fonts::Push(FontFamily family, FontStyle style) {
    const auto *new_font = Get(family, style);
    if (ImGui::GetFont() == new_font) return false;

    ImGui::PushFont(Get(family, style));
    return true;
}
void Fonts::Pop() { ImGui::PopFont(); }
