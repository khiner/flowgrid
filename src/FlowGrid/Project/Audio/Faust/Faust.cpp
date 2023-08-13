#include "Faust.h"

#include "imgui.h"

#include "FaustListener.h"
#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/FileDialog/FileDialog.h"

static const std::string FaustDspFileExtension = ".dsp";

void FaustLogs::OnFaustChanged(ID, const FaustDSP &faust_dsp) {
    ErrorMessages.clear();
    ErrorMessages.emplace_back(faust_dsp.ErrorMessage);
}
void FaustLogs::OnFaustAdded(ID id, const FaustDSP &faust_dsp) {
    OnFaustChanged(id, faust_dsp);
}
void FaustLogs::OnFaustRemoved(ID) {
    ErrorMessages.clear();
}

Faust::Faust(ComponentArgs &&args) : Component(std::move(args)) {
    FaustDsps.RegisterDspChangeListener(&ParamsUis);
    FaustDsps.RegisterBoxChangeListener(&Graphs);
    FaustDsps.RegisterChangeListener(&Logs);
}
Faust::~Faust() {
    FaustDsps.UnregisterChangeListener(&Logs);
    FaustDsps.UnregisterBoxChangeListener(&Graphs);
    FaustDsps.UnregisterDspChangeListener(&ParamsUis);
}

void Faust::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust::DSP::Any &a) { FaustDsps.Apply(a); },
        [this](const Action::Faust::Graph::Any &a) { Graphs.Apply(a); },
        [this](const Action::Faust::GraphStyle::Any &a) { Graphs.Style.Apply(a); },
        [this](const Action::Faust::File::Any &a) {
            Visit(
                a,
                [](const Action::Faust::File::ShowOpenDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", ""}); },
                [](const Action::Faust::File::ShowSaveDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}); },
                [this](const Action::Faust::File::Open &a) {
                    if (FaustDsps.Empty()) return;
                    FaustDsps.front()->Code.Set(FileIO::read(a.file_path));
                },
                [this](const Action::Faust::File::Save &a) {
                    if (FaustDsps.Empty()) return;
                    FileIO::write(a.file_path, FaustDsps.front()->Code);
                },
            );
        },

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
            if (file_dialog.SaveMode) Action::Faust::File::Save{selected_path}.q();
            else Action::Faust::File::Open{selected_path}.q();
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
