#include "Faust.h"

#include "imgui.h"

#include "FaustListener.h"
#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/FileDialog/FileDialog.h"

static const std::string FaustDspFileExtension = ".dsp";

FaustLogs::FaustLogs(ComponentArgs &&args) : Component(std::move(args)) {}

void FaustLogs::OnFaustChanged(const FaustDSP &faust_dsp) {
    ErrorMessages.clear();
    ErrorMessages.emplace_back(faust_dsp.ErrorMessage);
}

Faust::Faust(ComponentArgs &&args) : Component(std::move(args)) {
    Code.RegisterChangeListener(this);
    FaustDsp.RegisterDspChangeListener(&ParamsUis);
    FaustDsp.RegisterBoxChangeListener(&Graphs);
    FaustDsp.RegisterChangeListener(&Logs);
}
Faust::~Faust() {
    FaustDsp.UnregisterDspChangeListener(&ParamsUis);
    Field::UnregisterChangeListener(this);
}

void Faust::OnFieldChanged() {
    if (Code.IsChanged()) FaustDsp.Update(Code);
}

void Faust::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::FaustFile::Any &a) {
            Visit(
                a,
                [](const Action::FaustFile::ShowOpenDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", ""}); },
                [](const Action::FaustFile::ShowSaveDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}); },
                [this](const Action::FaustFile::Open &a) { Code.Set(FileIO::read(a.file_path)); },
                [this](const Action::FaustFile::Save &a) { FileIO::write(a.file_path, Code); },
            );
        },
        [this](const Action::FaustGraph::Any &a) { Graphs.Apply(a); },
        [this](const Action::FaustGraphStyle::Any &a) { Graphs.Style.Apply(a); },
    );
}

bool Faust::CanApply(const ActionType &) const { return true; }

using namespace ImGui;

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

void Faust::Render() const {
    static string PrevSelectedPath = "";
    if (PrevSelectedPath != file_dialog.SelectedFilePath) {
        const fs::path selected_path = file_dialog.SelectedFilePath;
        const string &extension = selected_path.extension();
        if (extension == FaustDspFileExtension) {
            if (file_dialog.SaveMode) Action::FaustFile::Save{selected_path}.q();
            else Action::FaustFile::Open{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

void FaustLogs::Render() const {
    PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
    for (const auto &error_message : ErrorMessages) {
        TextUnformatted(error_message);
    }
    PopStyleColor();
}

// void FaustLogs::OnFaustChanged(const FaustDsp &faust) {}
