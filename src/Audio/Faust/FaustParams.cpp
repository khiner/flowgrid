#include "FaustParams.h"
#include "FaustParamsUI.h"

#include "Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"

#include <imgui.h>

FaustParams::FaustParams(ComponentArgs &&args, const FaustParamsStyle &style)
    : Component(std::move(args)), Style(style) {}

FaustParams::~FaustParams() {
    if (Dsp) Dsp->instanceResetUserInterface();
}

void FaustParams::SetDsp(dsp *dsp) {
    if (Dsp) {
        Dsp->instanceResetUserInterface();
        if (!dsp) Impl.reset();
    }
    Dsp = dsp;
    if (Dsp) {
        Impl = std::make_unique<FaustParamsUI>(*this);
        Dsp->buildUserInterface(Impl.get());
    }
}

void FaustParams::Render() const {
    if (!Impl) return;

    RootGroup.Render(ImGui::GetContentRegionAvail().y, true);

    // if (hovered_node) {
    //     const string label = GetUiLabel(hovered_node->tree);
    //     if (!label.empty()) {
    //         const auto *widget = GetWidget(label);
    //         if (widget) cout << "Found widget: " << label << '\n';
    //     }
    // }
}
