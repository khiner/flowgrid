#include "ImGuiSettings.h"

#include <iostream>

#include "imgui_internal.h"

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
    vector<ID> node_id(size), parent_node_id(size), parent_window_id(size), selected_tab_id(size);
    vector<int> split_axis(size), depth(size), flags(size);
    vector<U32> pos(size), sz(size), sz_ref(size);
    for (Count i = 0; i < size; i++) {
        const auto &ds = dss[int(i)];
        node_id[i] = ds.NodeId;
        parent_node_id[i] = ds.ParentNodeId;
        parent_window_id[i] = ds.ParentWindowId;
        selected_tab_id[i] = ds.SelectedTabId;
        split_axis[i] = ds.SplitAxis;
        depth[i] = ds.Depth;
        flags[i] = ds.Flags;
        pos[i] = PackImVec2ih(ds.Pos);
        sz[i] = PackImVec2ih(ds.Size);
        sz_ref[i] = PackImVec2ih(ds.SizeRef);
    }
    NodeId.Set(node_id);
    ParentNodeId.Set(parent_node_id);
    ParentWindowId.Set(parent_window_id);
    SelectedTabId.Set(selected_tab_id);
    SplitAxis.Set(split_axis);
    Depth.Set(depth);
    Flags.Set(flags);
    Pos.Set(pos);
    Size.Set(sz);
    SizeRef.Set(sz_ref);
}
void DockNodeSettings::Apply(ImGuiContext *ctx) const {
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
    vector<ID> id, class_id, viewport_id, dock_id;
    vector<int> dock_order;
    vector<U32> pos, sz, viewport_pos;
    vector<bool> collapsed;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        id.push_back(ws->ID);
        class_id.push_back(ws->ClassId);
        viewport_id.push_back(ws->ViewportId);
        dock_id.push_back(ws->DockId);
        dock_order.push_back(ws->DockOrder);
        pos.push_back(PackImVec2ih(ws->Pos));
        sz.push_back(PackImVec2ih(ws->Size));
        viewport_pos.push_back(PackImVec2ih(ws->ViewportPos));
        collapsed.push_back(ws->Collapsed);
    }
    Id.Set(id);
    ClassId.Set(class_id);
    ViewportId.Set(viewport_id);
    DockId.Set(dock_id);
    DockOrder.Set(dock_order);
    Pos.Set(pos);
    Size.Set(sz);
    ViewportPos.Set(viewport_pos);
    Collapsed.Set(collapsed);
}

// See `imgui.cpp::ApplyWindowSettings`
void WindowSettings::Apply(ImGuiContext *) const {
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
    // Table settings
    vector<ImGuiID> id;
    vector<int> save_flags;
    vector<float> ref_scale;
    vector<Count> columns_counts, columns_count_max;
    vector<bool> want_apply;

    // Column settings
    vector<vector<float>> width_or_weight;
    vector<vector<::ID>> user_id;
    vector<vector<int>> index, display_order, sort_order, sort_direction;
    vector<vector<bool>> is_enabled, is_stretch;

    for (auto *ts_it = tss.begin(); ts_it != nullptr; ts_it = tss.next_chunk(ts_it)) {
        auto &ts = *ts_it;
        const auto columns_count = ts.ColumnsCount;

        id.push_back(ts.ID);
        save_flags.push_back(ts.SaveFlags);
        ref_scale.push_back(ts.RefScale);
        columns_counts.push_back(columns_count);
        columns_count_max.push_back(ts.ColumnsCountMax);
        want_apply.push_back(ts.WantApply);

        width_or_weight.push_back(vector<float>(columns_count));
        user_id.push_back(vector<::ID>(columns_count));
        index.push_back(vector<int>(columns_count));
        display_order.push_back(vector<int>(columns_count));
        sort_order.push_back(vector<int>(columns_count));
        sort_direction.push_back(vector<int>(columns_count));
        is_enabled.push_back(vector<bool>(columns_count));
        is_stretch.push_back(vector<bool>(columns_count));

        for (int column_index = 0; column_index < columns_count; column_index++) {
            const auto &cs = ts.GetColumnSettings()[column_index];
            width_or_weight.back()[column_index] = cs.WidthOrWeight;
            user_id.back()[column_index] = cs.UserID;
            index.back()[column_index] = cs.Index;
            display_order.back()[column_index] = cs.DisplayOrder;
            sort_order.back()[column_index] = cs.SortOrder;
            sort_direction.back()[column_index] = cs.SortDirection;
            is_enabled.back()[column_index] = cs.IsEnabled;
            is_stretch.back()[column_index] = cs.IsStretch;
        }
    }

    ID.Set(id);
    SaveFlags.Set(save_flags);
    RefScale.Set(ref_scale);
    ColumnsCount.Set(columns_counts);
    ColumnsCountMax.Set(columns_count_max);
    WantApply.Set(want_apply);
    Columns.WidthOrWeight.Set(width_or_weight);
    Columns.UserID.Set(user_id);
    Columns.Index.Set(index);
    Columns.DisplayOrder.Set(display_order);
    Columns.SortOrder.Set(sort_order);
    Columns.SortDirection.Set(sort_direction);
    Columns.IsEnabled.Set(is_enabled);
    Columns.IsStretch.Set(is_stretch);
}

// Adapted from `imgui_tables.cpp::TableLoadSettings`
void TableSettings::Apply(ImGuiContext *) const {
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
    SaveIniSettingsToMemory(); // Populate the `Settings` context members.

    store::BeginTransient();
    Nodes.Set(ctx->DockContext.NodesSettings);
    Windows.Set(ctx->SettingsWindows);
    Tables.Set(ctx->SettingsTables);

    return store::CreatePatch(Path);
}

void ImGuiSettings::Apply(ImGuiContext *ctx) const {
    DockSettingsHandler_ClearAll(ctx, nullptr);
    Windows.Apply(ctx);
    Tables.Apply(ctx);
    Nodes.Apply(ctx);
    DockSettingsHandler_ApplyAll(ctx, nullptr);

    // Other housekeeping to emulate `LoadIniSettingsFromMemory`
    ctx->SettingsLoaded = true;
    ctx->SettingsDirty = false;
}
