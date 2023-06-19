#pragma once

#include "Core/Container/Vector.h"
#include "Core/Container/Vector2D.h"
#include "Core/Store/Patch/Patch.h"

template<typename T> struct ImChunkStream;
template<typename T> struct ImVector;

struct ImGuiContext;
struct ImGuiDockNodeSettings;
struct ImGuiWindowSettings;
struct ImGuiTableSettings;

// These Dock/Window/Table settings are `Component` duplicates of those in `imgui.cpp`.
// They are stored here a structs-of-arrays (vs. arrays-of-structs)
// todo These will show up counter-intuitively in the json state viewers.
//  Use Raw/Formatted settings in state viewers to:
//  * convert structs-of-arrays to arrays-of-structs,
//  * unpack positions/sizes
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
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, SizeRef); // Packed ImVec2ih
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
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, ViewportPos); // Packed ImVec2ih
    Prop(Vector<bool>, Collapsed);
};

struct TableColumnSettings : Component {
    using Component::Component;

    // [table_index][column_index]
    Prop(Vector2D<float>, WidthOrWeight);
    Prop(Vector2D<ID>, UserID);
    Prop(Vector2D<int>, Index);
    Prop(Vector2D<int>, DisplayOrder);
    Prop(Vector2D<int>, SortOrder);
    Prop(Vector2D<int>, SortDirection);
    Prop(Vector2D<bool>, IsEnabled); // "Visible" in ini file
    Prop(Vector2D<bool>, IsStretch);
};

struct TableSettings : Component {
    using Component::Component;

    void Set(ImChunkStream<ImGuiTableSettings> &) const;
    void Update(ImGuiContext *) const;

    Prop(Vector<ImGuiID>, ID);
    Prop(Vector<int>, SaveFlags);
    Prop(Vector<float>, RefScale);
    Prop(Vector<Count>, ColumnsCount);
    Prop(Vector<Count>, ColumnsCountMax);
    Prop(Vector<bool>, WantApply);
    Prop(TableColumnSettings, Columns);
};

struct ImGuiSettings : Component {
    using Component::Component;

    // Create a patch resulting from applying the current ImGui context.
    Patch CreatePatch(ImGuiContext *ctx) const;

    // `Update(ctx)` is basically `imgui_context.settings = this`.
    // Behaves just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized `.ini` text format.
    void Update(ImGuiContext *ctx) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
};

extern const ImGuiSettings &imgui_settings;
