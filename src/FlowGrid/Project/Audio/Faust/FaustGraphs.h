#pragma once

#include "Core/Primitive/Flags.h"
#include "FaustGraphAction.h"
#include "FaustGraphStyle.h"

#include "FaustListener.h"

struct FaustGraphs : Component, Actionable<Action::Faust::Graph::Any>, FaustBoxChangeListener {
    FaustGraphs(ComponentArgs &&args)
        : Component(
              std::move(args),
              Menu({
                  Menu("File", {Action::Faust::Graph::ShowSaveSvgDialog::MenuItem}),
                  Menu("View", {Settings.HoverFlags}),
              })
          ) {}

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void OnFaustBoxChanged(ID, Box) override;
    void OnFaustBoxAdded(ID, Box) override;
    void OnFaustBoxRemoved(ID) override;

    struct GraphSettings : Component {
        using Component::Component;

        Prop_(Flags, HoverFlags, "?Hovering over a node in the graph will display the selected information", {"ShowRect?Display the hovered node's bounding rectangle", "ShowType?Display the hovered node's box type", "ShowChannels?Display the hovered node's channel points and indices", "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"}, FaustGraphHoverFlags_None);
    };

    Prop(GraphSettings, Settings);
    Prop(FaustGraphStyle, Style);

private:
    void Render() const override;

    void OnFaustBoxChangedInner(Box) const;
};

extern const FaustGraphs &faust_graphs;
