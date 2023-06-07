#pragma once

#include "Core/Field/Float.h"
#include "Core/Stateful/Window.h" // xxx should only need Stateful and Field.

struct ma_node_graph;

// Corresponds to `ma_node_base`.
// MA tracks nodes with an `ma_node *` type, where `ma_node` is an alias to `void`.
// This base `Node` can either be specialized or instantiated on its own.
struct AudioGraphNode : UIStateful {
    AudioGraphNode(Stateful::Base *parent, string_view path_segment, string_view name_help = "", bool on = true);

    void Set(void *) const; // Set MA node.
    void *Get() const; // Get MA node.

    Count InputBusCount() const;
    Count OutputBusCount() const;
    Count InputChannelCount(Count bus) const;
    Count OutputChannelCount(Count bus) const;

    bool IsSource() const { return OutputBusCount() > 0; }
    bool IsDestination() const { return InputBusCount() > 0; }

    void Init(ma_node_graph *) const; // Add MA node.
    void Update(ma_node_graph *) const; // Update MA node based on current settings (e.g. volume).
    void Uninit() const; // Remove MA node.

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
    Prop(Float, Volume, 1.0);

protected:
    void Render() const override;
    virtual void DoInit(ma_node_graph *) const {};
    virtual void DoUpdate() const {};
    virtual void DoUninit() const;
    virtual bool NeedsRestart() const { return false; }; // Return `true` if node needs re-initialization due to changed state.

private:
    inline static std::unordered_map<ID, void *> DataFor; // MA node for owning Node's ID.
};
