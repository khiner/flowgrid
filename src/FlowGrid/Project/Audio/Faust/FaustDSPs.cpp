#include "FaustDSPs.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

#include "Helper/File.h"
#include "Project/Audio/AudioIO.h"
#include "Project/Audio/Graph/AudioGraphAction.h"
#include "Project/FileDialog/FileDialog.h"

#include "imgui.h"

using enum NotificationType;

static const std::string FaustDspFileExtension = ".dsp";

FaustDSP::FaustDSP(ComponentArgs &&args)
    : Component(std::move(args)), ParentContainer(static_cast<FaustDSPs *>(Parent->Parent)) {
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

void FaustDSP::Init(bool constructing) {
    if (Dsp || !Code) return Uninit(false);

    const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    std::vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const auto notification_type = constructing ? Added : Changed;

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    Box = DSPToBoxes("FlowGrid", string(Code), argc, argv.data(), &num_inputs, &num_outputs, ErrorMessage);
    if (!Box) destroyLibContext();
    NotifyBoxListeners(notification_type);

    if (Box && ErrorMessage.empty()) {
        static llvm_dsp_factory *dsp_factory;
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", Box, argc, argv.data(), "", ErrorMessage, optimize_level);
        if (dsp_factory && ErrorMessage.empty()) {
            Dsp = dsp_factory->createDSPInstance();
            if (!Dsp) ErrorMessage = "Successfully created Faust DSP factory, but could not create the Faust DSP instance.";
        }
    } else if (!Box && ErrorMessage.empty()) {
        ErrorMessage = "`DSPToBoxes` returned no error but did not produce a result.";
    }
    NotifyDspListeners(notification_type);

    NotifyListeners(notification_type);
}

void FaustDSP::Uninit(bool destructing) {
    if (Dsp || Box) {
        const auto notification_type = destructing ? Removed : Changed;
        if (Dsp) {
            Dsp = nullptr;
            NotifyDspListeners(notification_type);
            delete Dsp;
            // todo only delete the factory if it's the last dsp.
            // todo this is breaking.
            deleteAllDSPFactories();
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

void FaustDSP::NotifyBoxListeners(NotificationType type) const { ParentContainer->NotifyBoxListeners(type, *this); }
void FaustDSP::NotifyDspListeners(NotificationType type) const { ParentContainer->NotifyDspListeners(type, *this); }
void FaustDSP::NotifyListeners(NotificationType type) const { ParentContainer->NotifyListeners(type, *this); }

static const std::string FaustDspPathSegment = "FaustDSP";

FaustDSPs::FaustDSPs(ComponentArgs &&args) : Vector<FaustDSP>(std::move(args)) {
    createLibContext();
    WindowFlags |= ImGuiWindowFlags_MenuBar;
    EmplaceBack_(FaustDspPathSegment);
}

FaustDSPs::~FaustDSPs() {
    destroyLibContext();
}

void FaustDSPs::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust::DSP::Create &) { EmplaceBack(FaustDspPathSegment); },
        [this](const Action::Faust::DSP::Delete &a) { EraseId(a.id); },
    );
}

using namespace ImGui;

void FaustDSP::Render() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Audio")) {
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
    if (Empty()) {
        TextUnformatted("No Faust DSPs created yet.");
        return;
    }
    if (Size() == 1) {
        front()->Draw();
    }
}
