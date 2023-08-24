#include "FaustParamsUI.h"
#include "FaustParamsUIImpl.h"

#include "Project/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/dsp.h"

#include <imgui.h>

FaustParamsUI::FaustParamsUI(ComponentArgs &&args, const FaustParamsUIStyle &style)
    : Component(std::move(args)), Style(style) {}

FaustParamsUI::~FaustParamsUI() {}

void FaustParamsUI::SetDsp(dsp *dsp) {
    if (!dsp) Impl.reset();
    Dsp = dsp;
    if (Dsp) {
        Impl = std::make_unique<FaustParamsUIImpl>(*this);
        Dsp->buildUserInterface(Impl.get());
    }
}

void FaustParamsUI::Render() const {
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
