#pragma once

enum FaustParamType {
    Type_None = 0,
    // Containers
    Type_HGroup,
    Type_VGroup,
    Type_TGroup,

    // Widgets
    Type_Button,
    Type_CheckButton,
    Type_VSlider,
    Type_HSlider,
    Type_NumEntry,
    Type_HBargraph,
    Type_VBargraph,

    // Types specified with metadata
    Type_Knob,
    Type_Menu,
    Type_VRadioButtons,
    Type_HRadioButtons,
};
