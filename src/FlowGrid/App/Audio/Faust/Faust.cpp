#include "Faust.h"

#include <range/v3/core.hpp>
#include <range/v3/view/transform.hpp>

#include "imgui.h"

#include "App/Audio/AudioIO.h"
#include "App/FileDialog/FileDialog.h"
#include "Helper/File.h"

static const std::string FaustDspFileExtension = ".dsp";

void Faust::Apply(const Action::FaustAction &action) const {
    using namespace Action;
    Match(
        action,
        [&](const FaustFileAction &a) {
            Match(
                a,
                [&](const ShowOpenFaustFileDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", ""}); },
                [&](const ShowSaveFaustFileDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}); },
                [&](const OpenFaustFile &a) { store::Set(Code, FileIO::read(a.path)); },
                [&](const SaveFaustFile &a) { FileIO::write(a.path, Code); },
            );
        },
        [&](const FaustGraphAction &a) { Graph.Apply(a); },
    );
}

bool Faust::CanApply(const Action::FaustAction &action) const { return true; }

bool Faust::IsReady() const { return Code && !Log.Error; }
bool Faust::NeedsRestart() const {
    static string PreviousCode = Code;

    const bool needs_restart = Code != PreviousCode;
    PreviousCode = Code;
    return needs_restart;
}

ImGuiTableFlags TableFlagsToImGui(const TableFlags flags) {
    ImGuiTableFlags imgui_flags = ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingStretchProp;
    if (flags & TableFlags_Resizable) imgui_flags |= ImGuiTableFlags_Resizable;
    if (flags & TableFlags_Reorderable) imgui_flags |= ImGuiTableFlags_Reorderable;
    if (flags & TableFlags_Hideable) imgui_flags |= ImGuiTableFlags_Hideable;
    if (flags & TableFlags_Sortable) imgui_flags |= ImGuiTableFlags_Sortable;
    if (flags & TableFlags_ContextMenuInBody) imgui_flags |= ImGuiTableFlags_ContextMenuInBody;
    if (flags & TableFlags_BordersInnerH) imgui_flags |= ImGuiTableFlags_BordersInnerH;
    if (flags & TableFlags_BordersOuterH) imgui_flags |= ImGuiTableFlags_BordersOuterH;
    if (flags & TableFlags_BordersInnerV) imgui_flags |= ImGuiTableFlags_BordersInnerV;
    if (flags & TableFlags_BordersOuterV) imgui_flags |= ImGuiTableFlags_BordersOuterV;
    if (flags & TableFlags_NoBordersInBody) imgui_flags |= ImGuiTableFlags_NoBordersInBody;
    if (flags & TableFlags_PadOuterX) imgui_flags |= ImGuiTableFlags_PadOuterX;
    if (flags & TableFlags_NoPadOuterX) imgui_flags |= ImGuiTableFlags_NoPadOuterX;
    if (flags & TableFlags_NoPadInnerX) imgui_flags |= ImGuiTableFlags_NoPadInnerX;

    return imgui_flags;
}

using namespace ImGui;

void Faust::Render() const {
    static string PrevSelectedPath = "";
    if (PrevSelectedPath != file_dialog.SelectedFilePath) {
        const fs::path selected_path = string(file_dialog.SelectedFilePath);
        const string &extension = selected_path.extension();
        if (extension == FaustDspFileExtension) {
            if (file_dialog.SaveMode) Action::SaveFaustFile{selected_path}.q();
            else Action::OpenFaustFile{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

void Faust::FaustLog::Render() const {
    PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
    Error.Draw();
    PopStyleColor();
}
