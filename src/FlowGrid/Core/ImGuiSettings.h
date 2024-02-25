#pragma once

#include "Core/Container/PrimitiveVector.h"
#include "Core/Container/PrimitiveVector2D.h"

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

    Prop(PrimitiveVector<ID>, NodeId);
    Prop(PrimitiveVector<ID>, ParentNodeId);
    Prop(PrimitiveVector<ID>, ParentWindowId);
    Prop(PrimitiveVector<ID>, SelectedTabId);
    Prop(PrimitiveVector<int>, SplitAxis);
    Prop(PrimitiveVector<int>, Depth);
    Prop(PrimitiveVector<int>, Flags);
    Prop(PrimitiveVector<u32>, Pos); // Packed ImVec2ih
    Prop(PrimitiveVector<u32>, Size); // Packed ImVec2ih
    Prop(PrimitiveVector<u32>, SizeRef); // Packed ImVec2ih
};

struct WindowSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiWindowSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(PrimitiveVector<ID>, Id);
    Prop(PrimitiveVector<ID>, ClassId);
    Prop(PrimitiveVector<ID>, ViewportId);
    Prop(PrimitiveVector<ID>, DockId);
    Prop(PrimitiveVector<int>, DockOrder);
    Prop(PrimitiveVector<u32>, Pos); // Packed ImVec2ih
    Prop(PrimitiveVector<u32>, Size); // Packed ImVec2ih
    Prop(PrimitiveVector<u32>, ViewportPos); // Packed ImVec2ih
    Prop(PrimitiveVector<bool>, Collapsed);
};

struct TableColumnSettings : Component {
    using Component::Component;

    // [table_index][column_index]
    Prop(PrimitiveVector2D<float>, WidthOrWeight);
    Prop(PrimitiveVector2D<ID>, UserID);
    Prop(PrimitiveVector2D<int>, Index);
    Prop(PrimitiveVector2D<int>, DisplayOrder);
    Prop(PrimitiveVector2D<int>, SortOrder);
    Prop(PrimitiveVector2D<int>, SortDirection);
    Prop(PrimitiveVector2D<bool>, IsEnabled); // "Visible" in ini file
    Prop(PrimitiveVector2D<bool>, IsStretch);
};

struct TableSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiTableSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(PrimitiveVector<ImGuiID>, ID);
    Prop(PrimitiveVector<int>, SaveFlags);
    Prop(PrimitiveVector<float>, RefScale);
    Prop(PrimitiveVector<u32>, ColumnsCount);
    Prop(PrimitiveVector<u32>, ColumnsCountMax);
    Prop(PrimitiveVector<bool>, WantApply);
    Prop(TableColumnSettings, Columns);
};

struct ImGuiSettings : Component {
    using Component::Component;

    inline static bool IsChanged{false};

    // Create a patch resulting from applying the current ImGui context.
    Patch CreatePatch(ImGuiContext *) const;

    // `Update(ctx)` is basically `imgui_context.settings = this`.
    // Behaves just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized `.ini` text format.
    void UpdateIfChanged(ImGuiContext *) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
};
