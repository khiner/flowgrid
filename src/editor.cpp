#include "editor.h"
#include "config.h"
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

using namespace Zep;

using cmdFunc = std::function<void(const std::vector<std::string> &)>;

class ZepCmd : public ZepExCommand {
public:
    ZepCmd(ZepEditor &editor, const std::string name, cmdFunc fn)
        : ZepExCommand(editor), m_name(name), m_func(fn) {
    }

    virtual void Run(const std::vector<std::string> &args) override {
        m_func(args);
    }

    virtual const char *ExCommandName() const override {
        return m_name.c_str();
    }

private:
    std::string m_name;
    cmdFunc m_func;
};

struct ZepWrapper : public Zep::IZepComponent {
    ZepWrapper(const fs::path &root_path, const Zep::NVec2f &pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : zepEditor(Zep::ZepPath(root_path.string()), pixelScale), Callback(fnCommandCB) {
        zepEditor.RegisterCallback(this);

    }

    virtual Zep::ZepEditor &GetEditor() const override {
        return (Zep::ZepEditor &) zepEditor;
    }

    virtual void Notify(std::shared_ptr<Zep::ZepMessage> message) override {
        Callback(message);

        return;
    }

    virtual void HandleInput() {
        zepEditor.HandleInput();
    }

    Zep::ZepEditor_ImGui zepEditor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

std::shared_ptr<ZepWrapper> spZep;

void zep_init(const Zep::NVec2f &pixelScale) {
    // Initialize the editor and watch for changes
    spZep = std::make_shared<ZepWrapper>(config.app_root, Zep::NVec2f(pixelScale.x, pixelScale.y), [](std::shared_ptr<ZepMessage> spMessage) -> void {
    });

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
    if (spZep) {
        spZep->GetEditor().RefreshRequired();
    }
}

void zep_destroy() {
    spZep.reset();
}

ZepEditor &zep_get_editor() {
    return spZep->GetEditor();
}

void zep_load(const Zep::ZepPath &file) {
    auto pBuffer = zep_get_editor().InitWithFileOrDir(file);
}

void zep_show(const Zep::NVec2i &displaySize) {
    bool show = true;
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, displaySize.y), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Zep", &show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    if (max.x <= 0)
        max.x = 1;
    if (max.y <= 0)
        max.y = 1;
    ImGui::InvisibleButton("ZepContainer", max);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;

    spZep->zepEditor.SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));
    spZep->zepEditor.Display();
    bool zep_focused = ImGui::IsWindowFocused();
    if (zep_focused) {
        spZep->zepEditor.HandleInput();
    }

    // TODO: A Better solution for this; I think the audio graph is creating a new window and stealing focus
    static int focus_count = 0;
    if (focus_count++ < 2) {
        ImGui::SetWindowFocus();
    }
    ImGui::End();
}
