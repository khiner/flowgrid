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
