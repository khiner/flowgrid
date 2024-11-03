#pragma once

#include "Container/ComponentVector.h"
#include "Container/Vector.h"

struct Patch;

template<typename T> struct ImChunkStream;
template<typename T> struct ImVector;

struct ImGuiContext;
struct ImGuiDockNodeSettings;
struct ImGuiWindowSettings;
struct ImGuiTableSettings;

// These Dock/Window/Table settings are `Component` duplicates of those in `imgui.cpp`.
// They are stored here a structs-of-arrays (vs. arrays-of-structs)
// todo Use Raw/Formatted settings in state viewers to unpack positions/sizes
struct DockNodeSettings : Component {
    using Component::Component;

    void Set(const ImVector<ImGuiDockNodeSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(Vector<ID>, NodeId);
    Prop(Vector<ID>, ParentNodeId);
    Prop(Vector<ID>, ParentWindowId);
    Prop(Vector<ID>, SelectedTabId);
    Prop(Vector<int>, SplitAxis);
    Prop(Vector<int>, Depth);
    Prop(Vector<int>, Flags);
    Prop(Vector<u32>, Pos); // Packed ImVec2ih
    Prop(Vector<u32>, Size); // Packed ImVec2ih
    Prop(Vector<u32>, SizeRef); // Packed ImVec2ih
};

struct WindowSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiWindowSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(Vector<ID>, Id);
    Prop(Vector<ID>, ClassId);
    Prop(Vector<ID>, ViewportId);
    Prop(Vector<ID>, DockId);
    Prop(Vector<int>, DockOrder);
    Prop(Vector<u32>, Pos); // Packed ImVec2ih
    Prop(Vector<u32>, Size); // Packed ImVec2ih
    Prop(Vector<u32>, ViewportPos); // Packed ImVec2ih
    Prop(Vector<bool>, Collapsed);
};

struct TableColumnSettings : Component {
    using Component::Component;

    Prop(Vector<float>, WidthOrWeight);
    Prop(Vector<ID>, UserID);
    Prop(Vector<int>, Index);
    Prop(Vector<int>, DisplayOrder);
    Prop(Vector<int>, SortOrder);
    Prop(Vector<int>, SortDirection);
    Prop(Vector<bool>, IsEnabled); // "Visible" in ini file
    Prop(Vector<bool>, IsStretch);
};

struct TableSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiTableSettings> &);
    void Update(ImGuiContext *) const;

    Prop(Vector<::ID>, ID);
    Prop(Vector<int>, SaveFlags);
    Prop(Vector<float>, RefScale);
    Prop(Vector<u32>, ColumnsCount);
    Prop(Vector<u32>, ColumnsCountMax);
    Prop(Vector<bool>, WantApply);
    Prop(ComponentVector<TableColumnSettings>, Columns);
};

struct ImGuiSettings : Component {
    using Component::Component;

    inline static bool IsChanged{false};

    // Basically `imgui_context.settings = this`.
    // Behaves just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized `.ini` text format.
    void UpdateIfChanged(ImGuiContext *) const;
    // Basically `this = imgui_context.settings`.
    void Set(ImGuiContext *);

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
};
