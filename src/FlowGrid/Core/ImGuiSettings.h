#pragma once

#include "Core/Container/PrimitiveVec.h"
#include "Core/Container/Vector.h"

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

    Prop(PrimitiveVec<ID>, NodeId);
    Prop(PrimitiveVec<ID>, ParentNodeId);
    Prop(PrimitiveVec<ID>, ParentWindowId);
    Prop(PrimitiveVec<ID>, SelectedTabId);
    Prop(PrimitiveVec<int>, SplitAxis);
    Prop(PrimitiveVec<int>, Depth);
    Prop(PrimitiveVec<int>, Flags);
    Prop(PrimitiveVec<u32>, Pos); // Packed ImVec2ih
    Prop(PrimitiveVec<u32>, Size); // Packed ImVec2ih
    Prop(PrimitiveVec<u32>, SizeRef); // Packed ImVec2ih
};

struct WindowSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiWindowSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(PrimitiveVec<ID>, Id);
    Prop(PrimitiveVec<ID>, ClassId);
    Prop(PrimitiveVec<ID>, ViewportId);
    Prop(PrimitiveVec<ID>, DockId);
    Prop(PrimitiveVec<int>, DockOrder);
    Prop(PrimitiveVec<u32>, Pos); // Packed ImVec2ih
    Prop(PrimitiveVec<u32>, Size); // Packed ImVec2ih
    Prop(PrimitiveVec<u32>, ViewportPos); // Packed ImVec2ih
    Prop(PrimitiveVec<bool>, Collapsed);
};

struct TableColumnSettings : Component {
    using Component::Component;

    Prop(PrimitiveVec<float>, WidthOrWeight);
    Prop(PrimitiveVec<ID>, UserID);
    Prop(PrimitiveVec<int>, Index);
    Prop(PrimitiveVec<int>, DisplayOrder);
    Prop(PrimitiveVec<int>, SortOrder);
    Prop(PrimitiveVec<int>, SortDirection);
    Prop(PrimitiveVec<bool>, IsEnabled); // "Visible" in ini file
    Prop(PrimitiveVec<bool>, IsStretch);
};

struct TableSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiTableSettings> &);
    void Update(ImGuiContext *) const;

    Prop(PrimitiveVec<::ID>, ID);
    Prop(PrimitiveVec<int>, SaveFlags);
    Prop(PrimitiveVec<float>, RefScale);
    Prop(PrimitiveVec<u32>, ColumnsCount);
    Prop(PrimitiveVec<u32>, ColumnsCountMax);
    Prop(PrimitiveVec<bool>, WantApply);
    Prop(Vector<TableColumnSettings>, Columns);
};

struct ImGuiSettings : Component {
    using Component::Component;

    inline static bool IsChanged{false};

    // Create a patch resulting from applying the current ImGui context.
    Patch CreatePatch(ImGuiContext *);

    // `Update(ctx)` is basically `imgui_context.settings = this`.
    // Behaves just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized `.ini` text format.
    void UpdateIfChanged(ImGuiContext *) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
};
