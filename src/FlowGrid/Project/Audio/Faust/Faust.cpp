#include "Faust.h"

#include "imgui.h"

#include "FaustListener.h"
#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/Audio/Graph/AudioGraphAction.h"
#include "Project/FileDialog/FileDialog.h"

static const std::string FaustDspFileExtension = ".dsp";

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

bool Faust::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [this](const Action::Faust::DSP::Any &a) { return FaustDsps.CanApply(a); },
        [this](const Action::Faust::Graph::Any &a) { return Graphs.CanApply(a); },
        [this](const Action::Faust::GraphStyle::Any &a) { return Graphs.Style.CanApply(a); },
        [](const Action::Faust::File::Any &) { return true; },
    );
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

FaustParamsUIs::FaustParamsUIs(ComponentArgs &&args, const FaustParamsUIStyle &style) : Vector(std::move(args), CreateChild), Style(style) {}

std::unique_ptr<FaustParamsUI> FaustParamsUIs::CreateChild(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    auto *uis = static_cast<FaustParamsUIs *>(parent);
    return std::make_unique<FaustParamsUI>(ComponentArgs{parent, path_segment, "", path_prefix_segment}, uis->Style);
}

FaustParamsUI *FaustParamsUIs::FindUi(ID dsp_id) const {
    for (auto *ui : *this) {
        if (ui->DspId == dsp_id) return ui;
    }
    return nullptr;
}

void FaustParamsUIs::OnFaustDspChanged(ID dsp_id, dsp *dsp) {
    if (auto *ui = FindUi(dsp_id)) ui->SetDsp(dsp);
}
void FaustParamsUIs::OnFaustDspAdded(ID dsp_id, dsp *dsp) {
    static const string PrefixSegment = "Params";
    Refresh(); // todo Seems to be needed, but shouldn't be.
    auto child_it = std::find_if(begin(), end(), [dsp_id](auto *ui) { return ui->DspId == dsp_id; });
    if (child_it != end()) {
        (*child_it)->SetDsp(dsp);
        return;
    }

    EmplaceBack_(PrefixSegment, [dsp_id, dsp](auto *child) {
        child->DspId.Set_(dsp_id);
        child->SetDsp(dsp);
    });
}
void FaustParamsUIs::OnFaustDspRemoved(ID dsp_id) {
    if (auto *ui = FindUi(dsp_id)) EraseId_(ui->Id);
}

static std::unordered_set<FaustGraphs *> AllInstances{};

FaustGraphs::FaustGraphs(ComponentArgs &&args, const FaustGraphStyle &style, const FaustGraphSettings &settings)
    : Vector(
          std::move(args),
          Menu({
              Menu("File", {Action::Faust::Graph::ShowSaveSvgDialog::MenuItem}),
              Menu("View", {settings.HoverFlags}),
          }),
          CreateChild
      ), Style(style), Settings(settings) {
    Style.FoldComplexity.RegisterChangeListener(this);

    AllInstances.insert(this);
}
FaustGraphs::~FaustGraphs() {
    AllInstances.erase(this);
    Field::UnregisterChangeListener(this);
}

FaustGraph *FaustGraphs::FindGraph(ID dsp_id) const {
    for (auto *graph : *this) {
        if (graph->DspId == dsp_id) return graph;
    }
    return nullptr;
}

void FaustGraphs::OnFieldChanged() {
    if (Style.FoldComplexity.IsChanged()) {
        for (auto *graph : *this) graph->ResetBox();
    }
}

void FaustGraphs::OnFaustBoxChanged(ID dsp_id, Box box) {
    if (auto *graph = FindGraph(dsp_id)) graph->SetBox(box);
}
void FaustGraphs::OnFaustBoxAdded(ID dsp_id, Box box) {
    static const string PrefixSegment = "Graph";
    Refresh(); // todo Seems to be needed, but shouldn't be.
    auto child_it = std::find_if(begin(), end(), [dsp_id](auto *graph) { return graph->DspId == dsp_id; });
    if (child_it != end()) {
        (*child_it)->SetBox(box);
        return;
    }

    EmplaceBack_(PrefixSegment, [dsp_id, box](auto *child) {
        child->DspId.Set_(dsp_id);
        child->SetBox(box);
    });
}
void FaustGraphs::OnFaustBoxRemoved(ID dsp_id) {
    if (auto *graph = FindGraph(dsp_id)) EraseId_(graph->Id);
}

void FaustGraphs::Apply(const ActionType &action) const {
    Visit(
        action,
        // Multiple SVG files are saved in a directory, to support navigation via SVG file hrefs.
        [](const Action::Faust::Graph::ShowSaveSvgDialog &) {
            file_dialog.Set({Action::Faust::Graph::ShowSaveSvgDialog::GetMenuLabel(), ".*", ".", "faust_graph", true, 1});
        },
        [this](const Action::Faust::Graph::SaveSvgFile &a) {
            if (const auto *graph = FindGraph(a.dsp_id)) graph->SaveBoxSvg(a.dir_path);
        },
    );
}

bool FaustGraphs::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [this](const Action::Faust::Graph::ShowSaveSvgDialog &) { return !Empty(); },
        [this](const Action::Faust::Graph::SaveSvgFile &a) {
            const auto *graph = FindGraph(a.dsp_id);
            return graph && graph->RootNode;
        },
    );
}

void FaustLogs::OnFaustChanged(ID id, const FaustDSP &faust_dsp) {
    ErrorMessageByFaustDspId[id] = faust_dsp.ErrorMessage;
}
void FaustLogs::OnFaustAdded(ID id, const FaustDSP &faust_dsp) {
    ErrorMessageByFaustDspId[id] = faust_dsp.ErrorMessage;
}
void FaustLogs::OnFaustRemoved(ID id) {
    ErrorMessageByFaustDspId.erase(id);
}

std::optional<std::string> FaustGraphs::FindBoxInfo(u32 imgui_id) {
    for (const auto *instance : AllInstances) {
        if (auto box_info = instance->GetBoxInfo(imgui_id)) return box_info;
    }
    return {};
}

std::unique_ptr<FaustGraph> FaustGraphs::CreateChild(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    auto *graphs = static_cast<FaustGraphs *>(parent);
    return std::make_unique<FaustGraph>(ComponentArgs{parent, path_segment, "", path_prefix_segment}, graphs->Style, graphs->Settings);
}

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

FaustDSP::FaustDSP(ComponentArgs &&args, const FaustDSPContainer &container)
    : Component(std::move(args)), Container(container) {
    Code.RegisterChangeListener(this);
    if (Code) Init(true);
}

FaustDSP::~FaustDSP() {
    Uninit(true);
    Field::UnregisterChangeListener(this);
}

void FaustDSP::OnFieldChanged() {
    if (Code.IsChanged()) Update();
}

void FaustDSP::DestroyDsp() {
    if (Dsp) {
        Dsp->instanceResetUserInterface();
        delete Dsp;
        Dsp = nullptr;
    }
    if (DspFactory) {
        deleteDSPFactory(DspFactory);
        DspFactory = nullptr;
    }
}

void FaustDSP::Init(bool constructing) {
    const auto notification_type = constructing ? Added : Changed;

    static const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    std::vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");
    const int argc = argv.size();
    static int num_inputs, num_outputs;
    Box = DSPToBoxes("FlowGrid", string(Code), argc, argv.data(), &num_inputs, &num_outputs, ErrorMessage);
    NotifyBoxListeners(notification_type);

    if (Box && ErrorMessage.empty()) {
        static const int optimize_level = -1;
        DspFactory = createDSPFactoryFromBoxes("FlowGrid", Box, argc, argv.data(), "", ErrorMessage, optimize_level);
        if (DspFactory) {
            if (ErrorMessage.empty()) {
                Dsp = DspFactory->createDSPInstance();
                if (!Dsp) ErrorMessage = "Successfully created Faust DSP factory, but could not create the Faust DSP instance.";
            } else {
                deleteDSPFactory(DspFactory);
                DspFactory = nullptr;
            }
        }
    } else if (!Box && ErrorMessage.empty()) {
        ErrorMessage = "`DSPToBoxes` returned no error but did not produce a result.";
    }
    if (!Box && Dsp) DestroyDsp();

    NotifyDspListeners(notification_type);

    NotifyListeners(notification_type);
}

void FaustDSP::Uninit(bool destructing) {
    if (Dsp || Box) {
        const auto notification_type = destructing ? Removed : Changed;
        if (Dsp) {
            DestroyDsp();
            NotifyDspListeners(notification_type);
        }
        if (Box) {
            Box = nullptr;
            NotifyBoxListeners(notification_type);
        }
        NotifyListeners(notification_type);
    }

    ErrorMessage = "";
}

void FaustDSP::Update() {
    if (!Dsp && Code) return Init(false);
    if (Dsp && !Code) return Uninit(false);

    Uninit(false);
    Init(false);
}

void FaustDSP::NotifyBoxListeners(NotificationType type) const { Container.NotifyBoxListeners(type, *this); }
void FaustDSP::NotifyDspListeners(NotificationType type) const { Container.NotifyDspListeners(type, *this); }
void FaustDSP::NotifyListeners(NotificationType type) const { Container.NotifyListeners(type, *this); }

static const std::string FaustDspPathSegment = "FaustDSP";

FaustDSPs::FaustDSPs(ComponentArgs &&args) : Vector(std::move(args), CreateChild) {
    createLibContext();
    WindowFlags |= ImGuiWindowFlags_MenuBar;
    EmplaceBack_(FaustDspPathSegment);
}

FaustDSPs::~FaustDSPs() {
    destroyLibContext();
}

std::unique_ptr<FaustDSP> FaustDSPs::CreateChild(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    const auto *dsps = static_cast<const FaustDSPs *>(parent);
    return std::make_unique<FaustDSP>(ComponentArgs{parent, path_segment, "", path_prefix_segment}, *dsps);
}

void FaustDSPs::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust::DSP::Create &) { EmplaceBack(FaustDspPathSegment); },
        [this](const Action::Faust::DSP::Delete &a) { EraseId(a.id); },
    );
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

void FaustParamsUIs::Render() const {
    // todo don't show empty menu bar in this case
    if (Empty()) return TextUnformatted("No Faust DSPs created yet.");
    if (Size() == 1) return (*this)[0]->Draw();

    if (BeginTabBar("")) {
        for (const auto *ui : *this) {
            if (BeginTabItem(std::format("{}", ID(ui->DspId)).c_str())) {
                ui->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void FaustLogs::RenderErrorMessage(string_view error_message) const {
    if (!error_message.empty()) {
        PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
        TextUnformatted(error_message);
        PopStyleColor();
    } else {
        PushStyleColor(ImGuiCol_Text, {0, 1, 0, 1});
        TextUnformatted("No error message.");
        PopStyleColor();
    }
}

void FaustLogs::Render() const {
    if (ErrorMessageByFaustDspId.empty()) return TextUnformatted("No Faust DSPs created yet.");
    if (ErrorMessageByFaustDspId.size() == 1) return RenderErrorMessage(ErrorMessageByFaustDspId.begin()->second);

    if (BeginTabBar("")) {
        for (const auto &[faust_dsp_id, error_message] : ErrorMessageByFaustDspId) {
            if (BeginTabItem(std::format("{}", faust_dsp_id).c_str())) {
                RenderErrorMessage(error_message);
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void FaustGraphs::Render() const {
    if (Empty()) return TextUnformatted("No Faust DSPs created yet.");

    static string PrevSelectedPath = "";
    if (PrevSelectedPath != file_dialog.SelectedFilePath) {
        const fs::path selected_path = file_dialog.SelectedFilePath;
        if (file_dialog.Title == Action::Faust::Graph::ShowSaveSvgDialog::GetMenuLabel() && file_dialog.SaveMode) {
            Action::Faust::Graph::SaveSvgFile{Id, selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }

    if (Size() == 1) return (*this)[0]->Draw();

    if (BeginTabBar("")) {
        for (const auto *graph : *this) {
            if (BeginTabItem(std::format("{}", ID(graph->DspId)).c_str())) {
                graph->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void FaustDSP::Render() const {
    if (BeginMenuBar()) {
        if (BeginMenu("DSP")) {
            if (MenuItem("Delete")) Action::Faust::DSP::Delete{Id}.q();
            if (MenuItem("Create audio node")) Action::AudioGraph::CreateFaustNode{Id}.q();
            EndMenu();
        }
        EndMenuBar();
    }
    Code.Draw();
}

void FaustDSPs::Render() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Create")) {
            if (MenuItem("Create Faust DSP")) Action::Faust::DSP::Create().q();
            EndMenu();
        }
        EndMenuBar();
    }

    if (Empty()) return TextUnformatted("No Faust DSPs created yet.");
    if (Size() == 1) return (*this)[0]->Draw();

    if (BeginTabBar("")) {
        for (const auto *faust_dsp : *this) {
            if (BeginTabItem(std::format("{}", faust_dsp->Id).c_str())) {
                faust_dsp->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}
