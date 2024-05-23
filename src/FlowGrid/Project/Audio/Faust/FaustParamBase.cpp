#include "FaustParamBase.h"

#include <imgui.h>

#include "FaustParamsStyle.h"

using namespace ImGui;
using namespace fg;

using enum FaustParamType;
using std::min, std::max;

float FaustParamBase::CalcWidth(bool include_label) const {
    (void)include_label; // Unused.
    return GetContentRegionAvail().x;
}

float FaustParamBase::CalcHeight() const {
    const float frame_height = GetFrameHeight();
    switch (Type) {
        case Type_VBargraph:
        case Type_VSlider:
        case Type_VRadioButtons: return Style.MinVerticalItemHeight * frame_height;
        case Type_HSlider:
        case Type_NumEntry:
        case Type_HBargraph:
        case Type_Button:
        case Type_CheckButton:
        case Type_HRadioButtons:
        case Type_Menu: return frame_height;
        case Type_Knob: return Style.MinKnobItemSize * frame_height + frame_height + ImGui::GetStyle().ItemSpacing.y;
        default: return 0;
    }
}

// Returns _additional_ height needed to accommodate a label for the param
float FaustParamBase::CalcLabelHeight() const {
    switch (Type) {
        case Type_VBargraph:
        case Type_VSlider:
        case Type_VRadioButtons:
        case Type_Knob:
        case Type_HGroup:
        case Type_VGroup:
        case Type_TGroup: return GetTextLineHeightWithSpacing();
        case Type_Button:
        case Type_HSlider:
        case Type_NumEntry:
        case Type_HBargraph:
        case Type_CheckButton:
        case Type_HRadioButtons:
        case Type_Menu:
        case Type_None: return 0;
    }
}
