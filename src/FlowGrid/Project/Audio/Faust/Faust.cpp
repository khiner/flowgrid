#include "Faust.h"

#include "imgui.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/FileDialog/FileDialog.h"

static const std::string FaustDspFileExtension = ".dsp";

Faust::Faust(ComponentArgs &&args) : Component(std::move(args)) {
    InitDsp();
}
Faust::~Faust() {
    UninitDsp();
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
        [this](const Action::FaustGraph::Any &a) { Graph.Apply(a); },
    );
}

bool Faust::CanApply(const ActionType &) const { return true; }

void Faust::InitDsp() {
    if (Dsp || !Code) return;

    createLibContext();

    const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    std::vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    string &error_message = Log.ErrorMessage;
    Box = DSPToBoxes("FlowGrid", Code, argc, argv.data(), &num_inputs, &num_outputs, error_message);
    if (!Box) destroyLibContext();

    Graph.OnBoxChanged(Box);

    if (Box && error_message.empty()) {
        static llvm_dsp_factory *dsp_factory;
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", Box, argc, argv.data(), "", error_message, optimize_level);
        if (dsp_factory && error_message.empty()) {
            Dsp = dsp_factory->createDSPInstance();
            if (!Dsp) error_message = "Successfully created Faust DSP factory, but could not create the Faust DSP instance.";
        }
    } else if (!Box && error_message.empty()) {
        error_message = "`DSPToBoxes` returned no error but did not produce a result.";
    }

    Params.OnDspChanged(Dsp);
}

void Faust::UninitDsp() {
    if (Dsp) {
        Params.OnDspChanged(nullptr);
        delete Dsp;
        Dsp = nullptr;
        deleteAllDSPFactories(); // There should only be one factory, but using this instead of `deleteDSPFactory` avoids storing another file-scoped variable.
    }
    if (Box) {
        Graph.OnBoxChanged(nullptr);
        Box = nullptr;
    }
    destroyLibContext();
}

void Faust::UpdateDsp() {
    if (!Dsp && Code) {
        InitDsp();
    } else if (Dsp && !Code) {
        UninitDsp();
    } else {
        UninitDsp();
        InitDsp();
    }
}

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

void Faust::FaustLog::Render() const {
    PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
    TextUnformatted(ErrorMessage.c_str());
    PopStyleColor();
}
