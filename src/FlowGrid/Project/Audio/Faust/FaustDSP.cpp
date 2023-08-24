#include "FaustDSP.h"

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
