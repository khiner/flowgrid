#pragma once

#include "Core/Primitive/Flags.h"
#include "FaustBox.h"
#include "FaustGraphAction.h"
#include "FaustGraphStyle.h"

struct FaustGraph : Component, Actionable<Action::FaustGraph::Any> {
    FaustGraph(ComponentArgs &&args)
        : Component(
              std::move(args),
              Menu({
                  Menu("File", {Action::FaustGraph::ShowSaveSvgDialog::MenuItem}),
                  Menu("View", {Settings.HoverFlags}),
              })
          ) {}

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void OnBoxChanged(Box) const;

    struct GraphSettings : Component {
        using Component::Component;

        Prop_(Flags, HoverFlags, "?Hovering over a node in the graph will display the selected information", {"ShowRect?Display the hovered node's bounding rectangle", "ShowType?Display the hovered node's box type", "ShowChannels?Display the hovered node's channel points and indices", "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"}, FaustGraphHoverFlags_None);
    };

    Prop(GraphSettings, Settings);
    Prop(FaustGraphStyle, Style);

private:
    void Render() const override;
};

extern const FaustGraph &faust_graph;
