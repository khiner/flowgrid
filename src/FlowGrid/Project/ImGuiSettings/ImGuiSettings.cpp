#include "ImGuiSettings.h"

#include "imgui_internal.h"
#include <iostream>

#include "Core/Store/Store.h"

using namespace ImGui;

constexpr U32 PackImVec2ih(const ImVec2ih &unpacked) { return (U32(unpacked.x) << 16) + U32(unpacked.y); }
constexpr ImVec2ih UnpackImVec2ih(const U32 packed) { return {S16(U32(packed) >> 16), S16(U32(packed) & 0xffff)}; }

// Copy of ImGui version, which is not defined publicly
struct ImGuiDockNodeSettings { // NOLINT(cppcoreguidelines-pro-type-member-init)
    ID NodeId;
    ID ParentNodeId;
    ID ParentWindowId;
    ID SelectedTabId;
    S8 SplitAxis;
    char Depth;
    ImGuiDockNodeFlags Flags;
    ImVec2ih Pos;
    ImVec2ih Size;
    ImVec2ih SizeRef;
};

void DockNodeSettings::Set(const ImVector<ImGuiDockNodeSettings> &dss) const {
    const Count size = dss.Size;

    NodeId.Resize(size);
    ParentNodeId.Resize(size);
    ParentWindowId.Resize(size);
    SelectedTabId.Resize(size);
    SplitAxis.Resize(size);
    Depth.Resize(size);
    Flags.Resize(size);
    Pos.Resize(size);
    Size.Resize(size);
    SizeRef.Resize(size);

    for (Count i = 0; i < size; i++) {
        const auto &ds = dss[int(i)];
        NodeId.Set(i, ds.NodeId);
        ParentNodeId.Set(i, ds.ParentNodeId);
        ParentWindowId.Set(i, ds.ParentWindowId);
        SelectedTabId.Set(i, ds.SelectedTabId);
        SplitAxis.Set(i, ds.SplitAxis);
        Depth.Set(i, ds.Depth);
        Flags.Set(i, ds.Flags);
        Pos.Set(i, PackImVec2ih(ds.Pos));
        Size.Set(i, PackImVec2ih(ds.Size));
        SizeRef.Set(i, PackImVec2ih(ds.SizeRef));
    }
}

void DockNodeSettings::Update(ImGuiContext *ctx) const {
    // Assumes `DockSettingsHandler_ClearAll` has already been called.
    const auto size = NodeId.Size();
    for (Count i = 0; i < size; i++) {
        ctx->DockContext.NodesSettings.push_back({
            NodeId[i],
            ParentNodeId[i],
            ParentWindowId[i],
            SelectedTabId[i],
            S8(SplitAxis[i]),
            char(Depth[i]),
            Flags[i],
            UnpackImVec2ih(Pos[i]),
            UnpackImVec2ih(Size[i]),
            UnpackImVec2ih(SizeRef[i]),
        });
    }
}

void WindowSettings::Set(ImChunkStream<ImGuiWindowSettings> &wss) const {
    const Count size = wss.size();

    Id.Resize(size);
    ClassId.Resize(size);
    ViewportId.Resize(size);
    DockId.Resize(size);
    DockOrder.Resize(size);
    Pos.Resize(size);
    Size.Resize(size);
    ViewportPos.Resize(size);
    Collapsed.Resize(size);

    Count i = 0;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        Id.Set(i, ws->ID);
        ClassId.Set(i, ws->ClassId);
        ViewportId.Set(i, ws->ViewportId);
        DockId.Set(i, ws->DockId);
        DockOrder.Set(i, ws->DockOrder);
        Pos.Set(i, PackImVec2ih(ws->Pos));
        Size.Set(i, PackImVec2ih(ws->Size));
        ViewportPos.Set(i, PackImVec2ih(ws->ViewportPos));
        Collapsed.Set(i, ws->Collapsed);
        i++;
    }
}

// See `imgui.cpp::ApplyWindowSettings`
void WindowSettings::Update(ImGuiContext *) const {
    const auto *main_viewport = GetMainViewport();
    const auto size = Id.Size();
    for (Count i = 0; i < size; i++) {
        const auto id = Id[i];
        auto *window = FindWindowByID(id);
        if (!window) {
            std::cerr << "Unable to apply settings for window with ID " << std::format("{:#08X}", id) << ": Window not found.\n";
            continue;
        }

        window->ViewportPos = main_viewport->Pos;
        if (ViewportId[i]) {
            window->ViewportId = ViewportId[i];
            const auto viewport_pos = UnpackImVec2ih(ViewportPos[i]);
            window->ViewportPos = ImVec2(viewport_pos.x, viewport_pos.y);
        }
        const auto pos = UnpackImVec2ih(Pos[i]);
        window->Pos = ImVec2(pos.x, pos.y) + ImFloor(window->ViewportPos);

        const auto size = UnpackImVec2ih(Size[i]);
        if (size.x > 0 && size.y > 0) window->Size = window->SizeFull = ImVec2(size.x, size.y);
        window->Collapsed = Collapsed[i];
        window->DockId = DockId[i];
        window->DockOrder = short(DockOrder[i]);
    }
}

void TableSettings::Set(ImChunkStream<ImGuiTableSettings> &tss) const {
    const Count size = tss.size();
    // Table settings
    ID.Resize(size);
    SaveFlags.Resize(size);
    RefScale.Resize(size);
    ColumnsCount.Resize(size);
    ColumnsCountMax.Resize(size);
    WantApply.Resize(size);
    // Column settings
    Columns.WidthOrWeight.Resize(size);
    Columns.UserID.Resize(size);
    Columns.Index.Resize(size);
    Columns.DisplayOrder.Resize(size);
    Columns.SortOrder.Resize(size);
    Columns.SortDirection.Resize(size);
    Columns.IsEnabled.Resize(size);
    Columns.IsStretch.Resize(size);

    Count i = 0;
    for (auto *ts_it = tss.begin(); ts_it != nullptr; ts_it = tss.next_chunk(ts_it)) {
        auto &ts = *ts_it;
        const Count columns_count = ts.ColumnsCount;

        Columns.WidthOrWeight.Resize(i, columns_count);
        Columns.UserID.Resize(i, columns_count);
        Columns.Index.Resize(i, columns_count);
        Columns.DisplayOrder.Resize(i, columns_count);
        Columns.SortOrder.Resize(i, columns_count);
        Columns.SortDirection.Resize(i, columns_count);
        Columns.IsEnabled.Resize(i, columns_count);
        Columns.IsStretch.Resize(i, columns_count);

        for (Count j = 0; j < columns_count; j++) {
            const auto &cs = ts.GetColumnSettings()[j];
            // todo these nans show up when we start with a default layout showing a table and then switch the tab so that the table is hidden.
            //   should probably handle this more robustly.
            Columns.WidthOrWeight.Set(i, j, std::isnan(cs.WidthOrWeight) ? 0 : cs.WidthOrWeight);
            Columns.UserID.Set(i, j, cs.UserID);
            Columns.Index.Set(i, j, cs.Index);
            Columns.DisplayOrder.Set(i, j, cs.DisplayOrder);
            Columns.SortOrder.Set(i, j, cs.SortOrder);
            Columns.SortDirection.Set(i, j, cs.SortDirection);
            Columns.IsEnabled.Set(i, j, cs.IsEnabled);
            Columns.IsStretch.Set(i, j, cs.IsStretch);
        }
        i++;
    }
}

// Adapted from `imgui_tables.cpp::TableLoadSettings`
void TableSettings::Update(ImGuiContext *) const {
    const auto size = ID.Size();
    for (Count i = 0; i < size; i++) {
        const auto id = ID[i];
        const auto table = TableFindByID(id);
        if (!table) {
            std::cerr << "Unable to apply settings for table with ID " << std::format("{:#08X}", id) << ": Table not found.\n";
            continue;
        }

        table->IsSettingsRequestLoad = false; // todo remove this var/behavior?
        table->SettingsLoadedFlags = SaveFlags[i]; // todo remove this var/behavior?
        table->RefScale = RefScale[i];

        // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
        ImU64 display_order_mask = 0;
        for (Count j = 0; j < ColumnsCount[i]; j++) {
            int column_n = Columns.Index(i, j);
            if (column_n < 0 || column_n >= table->ColumnsCount) continue;

            ImGuiTableColumn *column = &table->Columns[column_n];
            if (ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Resizable) {
                float width_or_weight = Columns.WidthOrWeight(i, j);
                if (Columns.IsStretch(i, j)) column->StretchWeight = width_or_weight;
                else column->WidthRequest = width_or_weight;
                column->AutoFitQueue = 0x00;
            }
            column->DisplayOrder = ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Reorderable ? ImGuiTableColumnIdx(Columns.DisplayOrder(i, j)) : (ImGuiTableColumnIdx)column_n;
            display_order_mask |= (ImU64)1 << column->DisplayOrder;
            column->IsUserEnabled = column->IsUserEnabledNextFrame = Columns.IsEnabled(i, j);
            column->SortOrder = ImGuiTableColumnIdx(Columns.SortOrder(i, j));
            column->SortDirection = Columns.SortDirection(i, j);
        }

        // Validate and fix invalid display order data
        const ImU64 expected_display_order_mask = ColumnsCount[i] == 64 ? ~0 : ((ImU64)1 << ImU8(ColumnsCount[i])) - 1;
        if (display_order_mask != expected_display_order_mask) {
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
                table->Columns[column_n].DisplayOrder = (ImGuiTableColumnIdx)column_n;
            }
        }
        // Rebuild index
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
            table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx)column_n;
        }
    }
}

Patch ImGuiSettings::CreatePatch(ImGuiContext *ctx) const {
    RootStore.BeginTransient();

    Nodes.Set(ctx->DockContext.NodesSettings);
    Windows.Set(ctx->SettingsWindows);
    Tables.Set(ctx->SettingsTables);

    return RootStore.CreatePatch(Path);
}

void ImGuiSettings::UpdateIfChanged(ImGuiContext *ctx) const {
    if (!IsChanged) return;

    IsChanged = false;

    DockSettingsHandler_ClearAll(ctx, nullptr);
    Windows.Update(ctx);
    Tables.Update(ctx);
    Nodes.Update(ctx);
    DockSettingsHandler_ApplyAll(ctx, nullptr);

    // Other housekeeping to emulate `LoadIniSettingsFromMemory`
    ctx->SettingsLoaded = true;
    ctx->SettingsDirty = false;
}
