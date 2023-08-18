#include "FaustParamsUIs.h"

#include "imgui.h"

std::unique_ptr<FaustParamsUI> FaustParamsUIs::CreateChild(Component *parent, string_view path_prefix_segment, string_view path_segment) {
    auto *uis = static_cast<FaustParamsUIs *>(parent->Parent);
    return std::make_unique<FaustParamsUI>(ComponentArgs{parent, path_segment, "", path_prefix_segment}, uis->Style);
}

FaustParamsUI *FaustParamsUIs::FindUi(ID dsp_id) const {
    for (auto *ui : Uis) {
        if (ui->DspId == dsp_id) return ui;
    }
    return nullptr;
}

void FaustParamsUIs::OnFaustDspChanged(ID dsp_id, dsp *dsp) {
    if (auto *ui = FindUi(dsp_id)) ui->SetDsp(dsp);
}
void FaustParamsUIs::OnFaustDspAdded(ID dsp_id, dsp *dsp) {
    static const string PrefixSegment = "Params";
    Uis.Refresh(); // todo Seems to be needed, but shouldn't be.
    auto child_it = std::find_if(Uis.begin(), Uis.end(), [dsp_id](auto *ui) { return ui->DspId == dsp_id; });
    if (child_it != Uis.end()) {
        (*child_it)->SetDsp(dsp);
        return;
    }

    Uis.EmplaceBack_(PrefixSegment, [dsp_id, dsp](auto *child) {
        child->DspId.Set_(dsp_id);
        child->SetDsp(dsp);
    });
}
void FaustParamsUIs::OnFaustDspRemoved(ID dsp_id) {
    if (auto *ui = FindUi(dsp_id)) Uis.EraseId_(ui->Id);
}

using namespace ImGui;

void FaustParamsUIs::Render() const {
    if (Uis.Empty()) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its params."); // todo link to window?
        return;
    }

    if (BeginTabBar("")) {
        for (const auto *ui : Uis) {
            if (BeginTabItem(std::format("{}", ID(ui->DspId)).c_str())) {
                ui->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}
