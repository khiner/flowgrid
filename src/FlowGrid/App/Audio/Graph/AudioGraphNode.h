#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"

struct ma_node_graph;

// Corresponds to `ma_node_base`.
// MA tracks nodes with an `ma_node *` type, where `ma_node` is an alias to `void`.
// This base `Node` can either be specialized or instantiated on its own.
struct AudioGraphNode : Component {
    AudioGraphNode(ComponentArgs &&, bool on = true);

    void Set(void *); // Set MA node.
    void *Get() const; // Get MA node.

    Count InputBusCount() const;
    Count OutputBusCount() const;
    Count InputChannelCount(Count bus) const;
    Count OutputChannelCount(Count bus) const;

    bool IsSource() const { return OutputBusCount() > 0; }
    bool IsDestination() const { return InputBusCount() > 0; }

    void Init(ma_node_graph *); // Add MA node.
    void Update(ma_node_graph *); // Update MA node based on current settings (e.g. volume).
    void Uninit(); // Remove MA node.

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
    Prop(Float, Volume, 1.0);

protected:
    void Render() const override;

    virtual bool NeedsRestart() const { return false; }; // Return `true` if node needs re-initialization due to changed state.
    virtual void DoInit(ma_node_graph *){};
    virtual void DoUpdate(){};
    virtual void DoUninit();

private:
    inline static std::unordered_map<ID, void *> DataForId; // MA node for owning Node's ID.
};
