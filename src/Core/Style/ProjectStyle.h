#pragma once

#include "Core/ActionProducerComponent.h"
#include "Core/Primitive/Float.h"
#include "Core/UI/Colors.h"
#include "StyleAction.h" // todo just ProjectStyle action

struct ImVec4;

enum ProjectCol_ {
    ProjectCol_GestureIndicator, // 2nd series in ImPlot color map (same in all 3 styles for now): `ImPlot::GetColormapColor(1, 0)`
    ProjectCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    ProjectCol_Flash, // ImGuiCol_FrameBgActive
    ProjectCol_COUNT
};
using ProjectCol = int;

struct ProjectStyle : ActionProducerComponent<Action::Combine<Action::Style::Any, Colors::ProducedActionType>> {
    ProjectStyle(ArgsT &&);

    static std::unordered_map<size_t, ImVec4> ColorsDark, ColorsLight, ColorsClassic;
    static const char *GetColorName(ProjectCol idx);

    Prop_(Float, FlashDurationSec, "?Duration (sec) of short flashes to visually notify on events.", 0.2, 0.1, 1);
    ProducerProp(Colors, Colors, ProjectCol_COUNT, GetColorName);

protected:
    void Render() const override;
};
