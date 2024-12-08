#include "ImGuiSettings.h"

#include <iostream>

#include "imgui_internal.h"

using namespace ImGui;

constexpr u32 PackImVec2ih(const ImVec2ih &unpacked) { return (u32(unpacked.x) << 16) + u32(unpacked.y); }
constexpr ImVec2ih UnpackImVec2ih(const u32 packed) { return {s16(u32(packed) >> 16), s16(u32(packed) & 0xffff)}; }

// Copy of ImGui version, which is not defined publicly
struct ImGuiDockNodeSettings { // NOLINT(cppcoreguidelines-pro-type-member-init)
    ID NodeId, ParentNodeId, ParentWindowId, SelectedTabId;
    s8 SplitAxis;
    char Depth;
    ImGuiDockNodeFlags Flags;
    ImVec2ih Pos, Size, SizeRef;
};

void DockNodeSettings::Set(TransientStore &s, const ImVector<ImGuiDockNodeSettings> &dss) const {
    NodeId.Clear(s);
    ParentNodeId.Clear(s);
    ParentWindowId.Clear(s);
    SelectedTabId.Clear(s);
    SplitAxis.Clear(s);
    Depth.Clear(s);
    Flags.Clear(s);
    Pos.Clear(s);
    Size.Clear(s);
    SizeRef.Clear(s);

    for (const auto &ds : dss) {
        NodeId.PushBack(s, ds.NodeId);
        ParentNodeId.PushBack(s, ds.ParentNodeId);
        ParentWindowId.PushBack(s, ds.ParentWindowId);
        SelectedTabId.PushBack(s, ds.SelectedTabId);
        SplitAxis.PushBack(s, ds.SplitAxis);
        Depth.PushBack(s, ds.Depth);
        Flags.PushBack(s, ds.Flags);
        Pos.PushBack(s, PackImVec2ih(ds.Pos));
        Size.PushBack(s, PackImVec2ih(ds.Size));
        SizeRef.PushBack(s, PackImVec2ih(ds.SizeRef));
    }
}

void DockNodeSettings::Update(ImGuiContext *ctx) const {
    // Assumes `DockSettingsHandler_ClearAll` has already been called.
    const auto size = NodeId.Get().size();
    for (u32 i = 0; i < size; ++i) {
        ctx->DockContext.NodesSettings.push_back({
            NodeId[i],
            ParentNodeId[i],
            ParentWindowId[i],
            SelectedTabId[i],
            s8(SplitAxis[i]),
            char(Depth[i]),
            Flags[i],
            UnpackImVec2ih(Pos[i]),
            UnpackImVec2ih(Size[i]),
            UnpackImVec2ih(SizeRef[i]),
        });
    }
}

void WindowSettings::Set(TransientStore &s, ImChunkStream<ImGuiWindowSettings> &wss) const {
    Id.Clear(s);
    ClassId.Clear(s);
    ViewportId.Clear(s);
    DockId.Clear(s);
    DockOrder.Clear(s);
    Pos.Clear(s);
    Size.Clear(s);
    ViewportPos.Clear(s);
    Collapsed.Clear(s);

    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        Id.PushBack(s, ws->ID);
        ClassId.PushBack(s, ws->ClassId);
        ViewportId.PushBack(s, ws->ViewportId);
        DockId.PushBack(s, ws->DockId);
        DockOrder.PushBack(s, ws->DockOrder);
        Pos.PushBack(s, PackImVec2ih(ws->Pos));
        Size.PushBack(s, PackImVec2ih(ws->Size));
        ViewportPos.PushBack(s, PackImVec2ih(ws->ViewportPos));
        Collapsed.PushBack(s, ws->Collapsed);
    }
}

// See `imgui.cpp::ApplyWindowSettings`
void WindowSettings::Update(ImGuiContext *) const {
    const auto *main_viewport = GetMainViewport();
    const auto size = Id.Get().size();
    for (u32 i = 0; i < size; ++i) {
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

void TableSettings::Set(TransientStore &s, ImChunkStream<ImGuiTableSettings> &tss) {
    u32 size = 0;
    for (auto *ts_it = tss.begin(); ts_it != nullptr; ts_it = tss.next_chunk(ts_it)) ++size;
    // Table settings
    ID.Resize(s, size);
    SaveFlags.Resize(s, size);
    RefScale.Resize(s, size);
    ColumnsCount.Resize(s, size);
    ColumnsCountMax.Resize(s, size);
    WantApply.Resize(s, size);
    // xxx this is the only non-const operation in all of `ImGuiSettings::CreatePatch`
    //  since it potentially needs to create new child components
    Columns.Resize_(s, size);

    u32 i = 0;
    for (auto *ts_it = tss.begin(); ts_it != nullptr; ts_it = tss.next_chunk(ts_it)) {
        auto &ts = *ts_it;
        const u32 columns_count = ts.ColumnsCount;
        ID.Set(s, i, ts.ID);
        SaveFlags.Set(s, i, ts.SaveFlags);
        RefScale.Set(s, i, ts.RefScale);
        ColumnsCount.Set(s, i, columns_count);
        ColumnsCountMax.Set(s, i, ts.ColumnsCountMax);
        WantApply.Set(s, i, ts.WantApply);

        const auto *column = Columns[i];
        column->WidthOrWeight.Clear(s);
        column->UserID.Clear(s);
        column->Index.Clear(s);
        column->DisplayOrder.Clear(s);
        column->SortOrder.Clear(s);
        column->SortDirection.Clear(s);
        column->IsEnabled.Clear(s);
        column->IsStretch.Clear(s);

        for (u32 j = 0; j < columns_count; j++) {
            const auto &cs = ts.GetColumnSettings()[j];
            // todo these nans show up when we start with a default layout showing a table and then switch the tab so that the table is hidden.
            //   should probably handle this more robustly.
            column->WidthOrWeight.PushBack(s, std::isnan(cs.WidthOrWeight) ? 0 : cs.WidthOrWeight);
            column->UserID.PushBack(s, cs.UserID);
            column->Index.PushBack(s, cs.Index);
            column->DisplayOrder.PushBack(s, cs.DisplayOrder);
            column->SortOrder.PushBack(s, cs.SortOrder);
            column->SortDirection.PushBack(s, cs.SortDirection);
            column->IsEnabled.PushBack(s, cs.IsEnabled);
            column->IsStretch.PushBack(s, cs.IsStretch);
        }
        ++i;
    }
}

// Adapted from `imgui_tables.cpp::TableLoadSettings`
void TableSettings::Update(ImGuiContext *) const {
    const auto size = ID.Get().size();
    for (u32 i = 0; i < size; ++i) {
        const auto id = ID[i];
        const auto table = TableFindByID(id);
        if (!table) {
            std::cerr << "Unable to apply settings for table with ID " << std::format("{:#08X}", id) << ": Table not found.\n";
            continue;
        }

        table->IsSettingsRequestLoad = false; // todo remove this var/behavior?
        table->SettingsLoadedFlags = SaveFlags[i]; // todo remove this var/behavior?
        table->RefScale = RefScale[i];

        const auto &settings = *Columns[i];
        // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
        ImU64 display_order_mask = 0;
        for (u32 j = 0; j < ColumnsCount[i]; j++) {
            const int column_n = settings.Index[j];
            if (column_n < 0 || column_n >= table->ColumnsCount) continue;

            auto *column = &table->Columns[column_n];
            if (ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Resizable) {
                const float width_or_weight = settings.WidthOrWeight[j];
                if (settings.IsStretch[j]) column->StretchWeight = width_or_weight;
                else column->WidthRequest = width_or_weight;
                column->AutoFitQueue = 0x00;
            }
            column->DisplayOrder = ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Reorderable ? ImGuiTableColumnIdx(settings.DisplayOrder[j]) : ImGuiTableColumnIdx(column_n);
            display_order_mask |= ImU64(1) << column->DisplayOrder;
            column->IsUserEnabled = column->IsUserEnabledNextFrame = settings.IsEnabled[j];
            column->SortOrder = ImGuiTableColumnIdx(settings.SortOrder[j]);
            column->SortDirection = settings.SortDirection[j];
        }

        // Validate and fix invalid display order data
        const ImU64 expected_display_order_mask = ColumnsCount[i] == 64 ? ~0 : (ImU64(1) << ImU8(ColumnsCount[i])) - 1;
        if (display_order_mask != expected_display_order_mask) {
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
                table->Columns[column_n].DisplayOrder = ImGuiTableColumnIdx(column_n);
            }
        }
        // Rebuild index
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
            table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = ImGuiTableColumnIdx(column_n);
        }
    }
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

void ImGuiSettings::Set(TransientStore &s, ImGuiContext *ctx) {
    Nodes.Set(s, ctx->DockContext.NodesSettings);
    Windows.Set(s, ctx->SettingsWindows);
    Tables.Set(s, ctx->SettingsTables);
}
