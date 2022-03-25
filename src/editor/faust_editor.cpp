/**
 * Based on https://github.com/cmaughan/zep_imgui/blob/main/demo/src/editor.cpp
 */

#include "faust_editor.h"
#include "../config.h"
#include <filesystem>
#include "../context.h"

namespace fs = std::filesystem;

using namespace Zep;

struct ZepWrapper : public Zep::IZepComponent {
    ZepWrapper(const fs::path &root_path, const Zep::NVec2f &pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : zepEditor(Zep::ZepPath(root_path.string()), pixelScale), Callback(std::move(fnCommandCB)) {
        zepEditor.RegisterCallback(this);

    }

    Zep::ZepEditor &GetEditor() const override {
        return (Zep::ZepEditor &) zepEditor;
    }

    void Notify(std::shared_ptr<Zep::ZepMessage> message) override {
        Callback(message);
    }

    virtual void HandleInput() {
        zepEditor.HandleInput();
    }

    Zep::ZepEditor_ImGui zepEditor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

std::shared_ptr<ZepWrapper> spZep;

// Initialize the editor and watch for changes
void zep_init(const Zep::NVec2f &pixelScale) {
    spZep = std::make_shared<ZepWrapper>(
        config.app_root,
        Zep::NVec2f(pixelScale.x, pixelScale.y),
        [](const std::shared_ptr<ZepMessage> &_) -> void {}
    );

    auto &display = spZep->GetEditor().GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    auto pixelHeight = pImFont->FontSize;
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.125)));
}

void zep_update() {
    if (spZep) spZep->GetEditor().RefreshRequired();
}

void zep_destroy() {
    spZep.reset();
}

void zep_load(const Zep::ZepPath &file) {
    auto pBuffer = spZep->GetEditor().InitWithFileOrDir(file);
}

void zep_show() {
    auto &m_s = ui_s.ui.windows.faust_editor;
    ImGui::SetNextWindowCollapsed(!s.ui.windows.faust_editor.open);
    ImGui::SetNextWindowSize(m_s.dimensions.size, ImGuiCond_FirstUseEver);

    // TODO cannot undo a collapse after closing and undoing a close
    bool open = ImGui::Begin("Faust", &m_s.visible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);
    if (open != s.ui.windows.faust_editor.open) q.enqueue(toggle_faust_editor_open{});
    if (!s.ui.windows.faust_editor.open) {
        ImGui::End();
        return;
    }

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

    ImGui::End();
}
