#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nlohmann/json.hpp"

#include "ComponentArgs.h"
#include "Core/HelpInfo.h"
#include "Core/Primitive/Scalar.h"
#include "Core/Store/IDs.h"
#include "Helper/Time.h"
#include "MenuItemDrawable.h"

using json = nlohmann::json;

namespace FlowGrid {}
namespace fg = FlowGrid;

struct Store;
struct PrimitiveActionQueuer;

namespace FlowGrid {
struct Style;
}
struct FlowGridStyle;
struct Windows;

template<typename T> struct ActionQueue;

struct Menu {
    using Item = std::variant<Menu, std::reference_wrapper<const MenuItemDrawable>, std::function<void()>>;

    Menu(std::string_view label, std::vector<const Item> &&items);
    explicit Menu(std::vector<const Item> &&);
    Menu(std::vector<const Item> &&items, const bool is_main);

    const std::string Label; // If no label is provided, this is rendered as a top-level window menu bar.
    const std::vector<const Item> Items;
    const bool IsMain{false};

    void Draw() const { Render(); }

protected:
    void Render() const;
};

struct ImGuiWindow;
using ImGuiWindowFlags = int;
using ImGuiTreeNodeFlags = int;

// Copy of some of ImGui's flags, to avoid including `imgui.h` in this header.
// Be sure to keep these in sync, because they are used directly as values for their ImGui counterparts.
enum WindowFlags_ {
    WindowFlags_None = 0,
    WindowFlags_NoScrollbar = 1 << 3,
    WindowFlags_NoScrollWithMouse = 1 << 4,
    WindowFlags_MenuBar = 1 << 10,
};

struct Component {
    using References = std::vector<std::reference_wrapper<const Component>>;
    using UniquePaths = std::unordered_set<StorePath, PathHash>;
    using PathsMoment = std::pair<TimePoint, UniquePaths>;

    struct ChangeListener {
        // Called when at least one of the listened components has changed.
        // Changed component(s) are not passed to the callback, but it's called while the components are still marked as changed,
        // so listeners can use `component.IsChanged()` to check which listened components were changed if they wish.
        virtual void OnComponentChanged() = 0;
    };

    // todo these should be non-static members on the Project (root) component.
    inline static std::unordered_map<ID, Component *> ById; // Access any component by its ID.
    // Components with at least one descendent (excluding itself) updated during the latest action pass.
    inline static std::unordered_set<ID> ChangedAncestorComponentIds;

    inline static std::unordered_set<ID> FieldIds; // IDs of all "field" components (primitives/containers).

    // Use when you expect a component with exactly this path to exist.
    static Component *ByPath(const StorePath &path) noexcept { return ById.at(IDs::ByPath.at(path)); }
    static Component *ByPath(StorePath &&path) noexcept { return ById.at(IDs::ByPath.at(std::move(path))); }

    static Component *Find(const StorePath &search_path) noexcept {
        if (IDs::ByPath.contains(search_path)) return ByPath(search_path);
        return nullptr;
    }

    // Component containers are fields that dynamically create/destroy child components.
    // Each component container has a single auxiliary field as a direct child which tracks the presence/ordering of its child component(s).
    inline static std::unordered_set<ID> ContainerIds;
    inline static std::unordered_set<ID> ContainerAuxiliaryIds;

    static const Component *FindAncestorContainer(const Component &);

    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersById;

    static void RegisterChangeListener(ChangeListener *listener, const Component &component) noexcept {
        ChangeListenersById[component.Id].insert(listener);
    }
    static void UnregisterChangeListener(ChangeListener *listener) noexcept {
        for (auto &[component_id, listeners] : ChangeListenersById) listeners.erase(listener);
        std::erase_if(ChangeListenersById, [](const auto &entry) { return entry.second.empty(); });
    }
    void RegisterChangeListener(ChangeListener *listener) const noexcept { RegisterChangeListener(listener, *this); }

    // IDs of all fields updated/added/removed during the latest action or undo/redo, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the paths will consist of only the root path.
    // For container fields, the paths will contain the container-relative paths of all affected elements.
    // All values are appended to `GestureChangedPaths` if the change occurred during a runtime action batch (as opposed to undo/redo, initialization, or project load).
    // `ChangedPaths` is cleared after each action (after refreshing all affected fields), and can thus be used to determine which fields were affected by the latest action.
    // (`LatestChangedPaths` is retained for the lifetime of the application.)
    // These same key IDs are also stored in the `ChangedIds` set, which also includes IDs for all ancestor component of all changed components.
    inline static std::unordered_map<ID, PathsMoment> ChangedPaths;

    // Latest (unique-field-relative-paths, store-commit-time) pair for each field over the lifetime of the application.
    // This is updated by both the forward action pass, and by undo/redo.
    inline static std::unordered_map<ID, PathsMoment> LatestChangedPaths{};

    // Chronological vector of (unique-field-relative-paths, store-commit-time) pairs for each field that has been updated during the current gesture.
    inline static std::unordered_map<ID, std::vector<PathsMoment>> GestureChangedPaths{};

    // IDs of all fields to which `ChangedPaths` are attributed.
    // These are the fields that should have their `Refresh()` called to update their cached values to synchronize with their backing store.
    inline static std::unordered_set<ID> ChangedIds;

    static std::optional<TimePoint> LatestUpdateTime(ID field_id, std::optional<StorePath> relative_path = {}) noexcept {
        if (!LatestChangedPaths.contains(field_id)) return {};

        const auto &[update_time, paths] = LatestChangedPaths.at(field_id);
        if (!relative_path) return update_time;
        if (paths.contains(*relative_path)) return update_time;
        return {};
    }

    // Refresh the cached values of all fields.
    // Only used during `main.cpp` initialization.
    static void RefreshAll() {
        for (auto id : FieldIds) ById.at(id)->Refresh();
    }

    // todo gesturing should be project-global, not component-static.
    inline static bool IsGesturing{};
    static void UpdateGesturing();

    Component(Store &, PrimitiveActionQueuer &, const Windows &, const fg::Style &);
    Component(ComponentArgs &&);
    Component(ComponentArgs &&, ImGuiWindowFlags flags);
    Component(ComponentArgs &&, Menu &&menu);
    Component(ComponentArgs &&, ImGuiWindowFlags flags, Menu &&menu);

    virtual ~Component();

    Component(const Component &) = delete; // Copying not allowed.
    Component &operator=(const Component &) = delete; // Assignment not allowed.

    virtual void SetJson(json &&) const;
    virtual json ToJson() const;
    json::json_pointer JsonPointer() const {
        return json::json_pointer(Path.string()); // Implicit `json_pointer` constructor is disabled.
    }

    // Refresh the component's cached value(s) based on the main store.
    // Should be called for each affected component after a state change to avoid stale values.
    // This is overriden by `Field`s to update their `Value` members after a state change.
    virtual void Refresh() {
        for (auto *child : Children) child->Refresh();
    }

    // Erase the component's cached value(s) from the main store.
    // This is overriden by Container `Field`s to update their `Value` members after a state change.
    virtual void Erase() const {
        for (const auto *child : Children) child->Erase();
    }

    // Render a nested tree of components, with Fields as leaf nodes displaying their values as text.
    // By default, renders `this` a node with children as child nodes.
    virtual void RenderValueTree(bool annotate, bool auto_select) const;
    virtual void RenderDebug() const {}

    // Override to return additional details to append to label in contexts with lots of horizontal room.
    virtual std::string GetLabelDetailSuffix() const { return ""; }

    const Component *Child(u32 i) const noexcept { return Children[i]; }
    u32 ChildCount() const noexcept { return Children.size(); }

    // Returns true if this component has changed directly (it must me a `Field` to be changed directly),
    // or if any of its descendent components have changed, if `include_descendents` is true.
    bool IsChanged(bool include_descendents = false) const noexcept;
    bool IsDescendentChanged() const noexcept { return ChangedAncestorComponentIds.contains(Id); }

    ImGuiWindow *FindWindow() const;
    ImGuiWindow *FindDockWindow() const; // Find the nearest ancestor window with a `DockId` (including itself).
    void Dock(ID node_id) const;
    bool Focus() const;

    // Child renderers.
    void RenderTabs() const;
    void RenderTreeNodes(ImGuiTreeNodeFlags flags = 0) const;

    bool TreeNode(std::string_view label, bool highlight_label = false, const char *value = nullptr, bool highlight_value = false, bool auto_select = false) const;

    // Wrappers around ImGui methods to avoid including `imgui.h` in this header.
    static void TreePop();
    static void TextUnformatted(std::string_view);

    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;

    void Draw() const; // Wraps around the internal `Render` function.

    const FlowGridStyle &GetFlowGridStyle() const;

    // todo next up: instead of using a reference to the root store, provide a function to get the root store.
    //   then, instead of modifying the root store during store commits, create a new store and replace it (more value-oriented).
    Store &RootStore; // Reference to the store at the root of this component's tree.
    PrimitiveActionQueuer &PrimitiveQ;
    const Windows &gWindows;
    const fg::Style &gStyle;
    Component *Root; // The root (project) component.
    Component *Parent; // Only null for the root component.
    std::vector<Component *> Children{};
    const std::string PathSegment;
    const StorePath Path;
    const std::string Name, Help, ImGuiLabel;
    const ID Id;

    Menu WindowMenu{{}};
    ImGuiWindowFlags WindowFlags{WindowFlags_None};

protected:
    virtual void Render() const {} // By default, components don't render anything.

    void OpenChanged() const; // Open this item if changed.
    void ScrollToChanged() const; // Scroll to this item if changed.

    void FlashUpdateRecencyBackground(std::optional<StorePath> relative_path = {}) const;

private:
    Component(Component *parent, std::string_view path_segment, std::string_view path_prefix_segment, HelpInfo, ImGuiWindowFlags, Menu &&);
};

// Minimal/base debug component.
// Actual debug content is rendered in the parent component's `RenderDebug()`,
// and debug components themselves can't further override `RenderDebug()`.
// Otherwise, debug components are just like regular components - they store additional config fields, can be rendered as windows, etc.
// Override and extend `Render` to render anything other than just the parent's debug content.
struct DebugComponent : Component {
    DebugComponent(ComponentArgs &&, float split_ratio = 0.25);
    DebugComponent(ComponentArgs &&, ImGuiWindowFlags flags, Menu &&menu, float split_ratio = 0.25);
    ~DebugComponent();

    const float SplitRatio;

protected:
    virtual void Render() const override { RenderDebug(); }

private:
    void RenderDebug() const override { Parent->RenderDebug(); } // Not overridable.
};

/**
Convenience macros for compactly defining `Component` types and their properties.

todo These will very likely be defined in a separate language once the API settles down.
  If we could hot-reload and only recompile the needed bits without restarting the app, it would accelerate development A TON.
  (Long compile times, although they aren't nearly as bad as [my previous attempt](https://github.com/khiner/flowgrid_old),
  are still the biggest drain on this project.)

Macros:

All macros end in semicolons already, so there's no strict need to suffix their usage with a semicolon.
However, all macro calls in FlowGrid are treated like regular function calls, appending a semicolon.
todo If we stick with this, add a `static_assert(true, "")` to the end of all macro definitions.
https://stackoverflow.com/questions/35530850/how-to-require-a-semicolon-after-a-macro
todo Try out replacing semicolon separators by e.g. commas.

* Properties
  - `Prop` adds a new property `this`.
  Define a property of this type during construction at class scope to add a child member variable.
    - Assumes it's being called within a `PropType` class scope during construction.
    `PropType`, with variable name `PropName`, constructing the state member with `this` as a parent, and store path-segment `"{PropName}"`.
    (string with value the same as the variable name).
  - `Prop_` is the same as `Prop`, but supports overriding the displayed name & adding help text in the third arg.
  - Arguments
    1) `PropType`: Any type deriving from `Component`.
    2) `PropName` (use PascalCase) is used for:
      - The ID of the property, relative to its parent (`this` during the macro's execution).
      - The name of the instance variable added to `this` (again, defined like any other instance variable in a `Component`).
      - The default label displayed in the UI is a 'Sentense cased' label derived from the prop's 'PascalCase' `PropName` property-id/path-segment (the second arg).
    3) `MetaStr`
      - Metadata string with format "Label string?Help string".
      - Optional, available with a `_` suffix.
      - Overrides the label displayed in the UI for this property.
      - Anything after a '?' is interpretted as a help string
        - E.g. `Prop_(Bool, TestAThing, "Test-a-thing?A state member for testing things")` overrides the default "Test a thing" label with a hyphenation.
        - Or, provide nothing before the '?' to add a help string without overriding the default `PropName`-derived label.
          - E.g. "?A state member for testing things."
**/

#define Prop(PropType, PropName, ...) PropType PropName{{this, #PropName, ""}, __VA_ARGS__};
#define Prop_(PropType, PropName, MetaStr, ...) PropType PropName{{this, #PropName, MetaStr}, __VA_ARGS__};

// Sub-producers produce a subset action type, so they need a new producer generated from the parent.
#define ProducerProp(PropType, PropName, ...) PropType PropName{{{this, #PropName, ""}, CreateProducer<PropType::ProducedActionType>()}, __VA_ARGS__};
#define ProducerProp_(PropType, PropName, MetaStr, ...) PropType PropName{{{this, #PropName, MetaStr}, CreateProducer<PropType::ProducedActionType>()}, __VA_ARGS__};

// Child producers produce the same action type as their parent, so they can simply use their parent's `q` function.
#define ChildProducerProp(PropType, PropName, ...) PropType PropName{{{this, #PropName, ""}, q}, __VA_ARGS__};
#define ChildProducerProp_(PropType, PropName, MetaStr, ...) PropType PropName{{{this, #PropName, MetaStr}, q}, __VA_ARGS__};
