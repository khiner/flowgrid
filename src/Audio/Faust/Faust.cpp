#include "Faust.h"

#include "imgui.h"

#include "Core/FileDialog/FileDialog.h"
#include "Core/Helper/File.h"
#include "Core/Project/ProjectContext.h"

void Faust::Render() const {}

FaustParamss::FaustParamss(ComponentArgs &&args, const FaustParamsStyle &style)
    : ComponentVector(std::move(args), [](auto &&child_args) {
          const auto *uis = static_cast<const FaustParamss *>(child_args.Parent);
          return std::make_unique<FaustParams>(std::move(child_args), uis->Style);
      }),
      Style(style) {
}

FaustParams *FaustParamss::FindUi(ID dsp_id) const {
    for (auto *ui : *this) {
        if (ui->DspId == dsp_id) return ui;
    }
    return nullptr;
}

static std::unordered_set<FaustGraphs *> AllInstances{};

FaustGraphs::FaustGraphs(ArgsT &&args, const FaustGraphStyle &style, const FaustGraphSettings &settings)
    : ComponentVector(
          std::move(args.Args),
          Menu({
              Menu("File", {ShowSaveSvgDialogMenuItem}),
              Menu("View", {settings.HoverFlags}),
          }),
          [this](auto &&child_args) {
              const auto *graphs = static_cast<const FaustGraphs *>(child_args.Parent);
              return std::make_unique<FaustGraph>(
                  FaustGraph::ArgsT{std::move(child_args), CreateProducer<FaustGraph::ProducedActionType>()}, graphs->Style, graphs->Settings
              );
          }
      ),
      ActionableProducer(std::move(args.Q)),
      Style(style), Settings(settings) {
    Style.FoldComplexity.RegisterChangeListener(this);
    AllInstances.insert(this);
}

FaustGraphs::~FaustGraphs() {
    AllInstances.erase(this);
    UnregisterChangeListener(this);
}

FaustGraph *FaustGraphs::FindGraph(ID dsp_id) const {
    for (auto *graph : *this) {
        if (graph->DspId == dsp_id) return graph;
    }
    return nullptr;
}

void FaustGraphs::OnComponentChanged() {
    if (Style.FoldComplexity.IsChanged()) {
        for (auto *graph : *this) graph->ResetBox();
    }
}

void FaustGraphs::Apply(const ActionType &action) const {
    std::visit(
        Match{
            // Multiple SVG files are saved in a directory, to support navigation via SVG file hrefs.
            [this](const Action::Faust::Graph::ShowSaveSvgDialog &) {
                Ctx.FileDialog.Set({
                    .OwnerId = Id,
                    .Title = Action::Faust::Graph::ShowSaveSvgDialog::GetMenuLabel(),
                    .DefaultFileName = "faust_graph",
                    .SaveMode = true,
                });
            },
            [this](const Action::Faust::Graph::SaveSvgFile &a) {
                if (const auto *graph = FindGraph(a.dsp_id)) graph->SaveBoxSvg(a.dir_path);
            },
        },
        action
    );
}

bool FaustGraphs::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](const Action::Faust::Graph::ShowSaveSvgDialog &) { return !Empty(); },
            [this](const Action::Faust::Graph::SaveSvgFile &a) {
                const auto *graph = FindGraph(a.dsp_id);
                return graph && graph->RootNode;
            },
        },
        action
    );
}

#include "Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

FaustDSP::FaustDSP(ArgsT &&args, FaustDSPContainer &container)
    : ActionProducerComponent(std::move(args)), Container(container) {
    Editor.RegisterChangeListener(this);
    Init();
}

FaustDSP::~FaustDSP() {
    Uninit();
    UnregisterChangeListener(this);
}

void FaustDSP::OnComponentChanged() {
    if (Editor.IsChanged()) Update();
}

void FaustDSP::DestroyDsp() {
    if (Dsp) {
        delete Dsp;
        Dsp = nullptr;
    }
    if (DspFactory) {
        deleteDSPFactory(DspFactory);
        DspFactory = nullptr;
    }
}

void FaustDSP::Init() {
    if (Editor.Empty()) return;

    static const std::string libraries_path = fs::relative("../lib/faust/libraries");
    std::vector<const char *> argv = {"-I", libraries_path.c_str()};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");
    const int argc = argv.size();

    const auto code = Editor.GetText();
    static int num_inputs, num_outputs;
    Box = DSPToBoxes("FlowGrid", code, argc, argv.data(), &num_inputs, &num_outputs, ErrorMessage);

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
    if (Box && Dsp) Container.NotifyListeners(Added, *this);
}

void FaustDSP::Uninit() {
    Container.NotifyListeners(Removed, *this);
    if (Dsp || Box) {
        if (Dsp) DestroyDsp();
        if (Box) Box = nullptr;
    }
    ErrorMessage = "";
}

void FaustDSP::Update() {
    Uninit();
    Init();
}

FaustDSPs::FaustDSPs(ArgsT &&args)
    : ComponentVector(std::move(args.Args), [&](auto &&child_args) {
          auto *container = static_cast<Faust *>(child_args.Parent->Parent);
          return std::make_unique<FaustDSP>(
              FaustDSP::ArgsT{std::move(child_args), CreateProducer<FaustDSP::ProducedActionType>()}, *container
          );
      }),
      ActionProducer(std::move(args.Q)) {
    createLibContext();
    WindowFlags |= ImGuiWindowFlags_MenuBar;
    EmplaceBack_(FaustDspPathSegment);
}

FaustDSPs::~FaustDSPs() {
    destroyLibContext();
}

void Faust::NotifyListeners(NotificationType type, FaustDSP &faust_dsp) {
    const ID id = faust_dsp.Id;
    dsp *dsp = faust_dsp.Dsp;
    Box box = faust_dsp.Box;
    if (type == Changed) {
        if (auto *ui = Paramss.FindUi(id)) ui->SetDsp(dsp);
        if (auto *graph = Graphs.FindGraph(id)) graph->SetBox(box);
        Logs.ErrorMessageByFaustDspId[id] = faust_dsp.ErrorMessage;
        for (auto *listener : DspChangeListeners) listener->OnFaustDspChanged(id, dsp);
    } else if (type == Added) {
        // Params
        static const auto ParamsPrefixSegment{"Params"};
        Paramss.Refresh(); // todo Seems to be needed, but shouldn't be.
        if (auto params_it = std::find_if(Paramss.begin(), Paramss.end(), [id](auto *ui) { return ui->DspId == id; });
            params_it != Paramss.end()) {
            (*params_it)->SetDsp(dsp);
        } else {
            Paramss.EmplaceBack_(ParamsPrefixSegment, [id, dsp](auto *child) {
                child->DspId.Set_(id);
                child->SetDsp(dsp);
            });
        }

        // Boxes
        static const auto GraphPrefixSegment{"Graph"};
        Graphs.Refresh(); // todo Seems to be needed, but shouldn't be.
        if (auto graph_it = std::find_if(Graphs.begin(), Graphs.end(), [id](auto *graph) { return graph->DspId == id; });
            graph_it != Graphs.end()) {
            (*graph_it)->SetBox(box);
        } else {
            Graphs.EmplaceBack_(GraphPrefixSegment, [id, box](auto *child) {
                child->DspId.Set_(id);
                child->SetBox(box);
            });
        }

        // Logs
        Logs.ErrorMessageByFaustDspId[id] = faust_dsp.ErrorMessage;

        // External listeners
        for (auto *listener : DspChangeListeners) listener->OnFaustDspAdded(id, dsp);
    } else if (type == Removed) {
        for (auto *listener : DspChangeListeners) listener->OnFaustDspRemoved(id);
        Logs.ErrorMessageByFaustDspId.erase(id);
        if (auto *graph = Graphs.FindGraph(id)) Graphs.EraseId_(graph->Id);
        if (auto *ui = Paramss.FindUi(id)) Paramss.EraseId_(ui->Id);
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

void FaustParamss::Render() const {
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

void FaustLogs::RenderErrorMessage(std::string_view error_message) const {
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

    static std::string PrevSelectedPath = "";
    auto &file_dialog = Ctx.FileDialog;
    if (PrevSelectedPath != file_dialog.SelectedFilePath && file_dialog.Data.OwnerId == Id && file_dialog.Data.SaveMode) {
        const fs::path selected_path = file_dialog.SelectedFilePath;
        PrevSelectedPath = file_dialog.SelectedFilePath = "";
        Q(Action::Faust::Graph::SaveSvgFile{LastSelectedDspId, selected_path});
    }

    if (Size() == 1) {
        const auto *graph = (*this)[0];
        LastSelectedDspId = graph->DspId;
        return graph->Draw();
    }

    if (BeginTabBar("")) {
        for (const auto *graph : *this) {
            if (BeginTabItem(std::format("{}", ID(graph->DspId)).c_str())) {
                LastSelectedDspId = graph->DspId;
                graph->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void FaustDSP::Render() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Faust DSP")) {
            if (MenuItem("Create DSP")) Q(Action::Faust::DSP::Create());
            if (BeginMenu("Current DSP")) {
                if (MenuItem("Create audio node")) Q(Action::AudioGraph::CreateFaustNode{Id});
                if (MenuItem("Delete")) Q(Action::Faust::DSP::Delete{Id});
                EndMenu();
            }
            EndMenu();
        }
        EndMenuBar();
    }
    Editor.Draw();
}

void FaustDSPs::Render() const {
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
