#include "Info.h"

#include "imgui_internal.h"

using namespace ImGui;

// Copied from `imgui.cpp`.
// static int StackToolFormatLevelInfo(ImGuiStackTool *tool, int n, bool format_for_ui, char *buf, size_t buf_size) {
//     auto *info = &tool->Results[n];
//     auto *window = (info->Desc[0] == 0 && n == 0) ? FindWindowByID(info->ID) : NULL;
//     // Source: window name (because the root ID don't call GetID() and so doesn't get hooked)
//     if (window) return ImFormatString(buf, buf_size, format_for_ui ? "\"%s\" [window]" : "%s", window->Name);
//     // Source: GetID() hooks (prioritize over ItemInfo() because we frequently use patterns like: PushID(str), Button("") where they both have same id)
//     if (info->QuerySuccess) return ImFormatString(buf, buf_size, (format_for_ui && info->DataType == ImGuiDataType_String) ? "\"%s\"" : "%s", info->Desc);
//     // Only start using fallback below when all queries are done, so during queries we don't flickering ??? markers.
//     if (tool->StackLevel < tool->Results.Size) return (*buf = 0);
//     return ImFormatString(buf, buf_size, "???");
// }

void Info::Render() const {
    if (const auto hovered_id = GetHoveredID(); !hovered_id) return;

    auto &g = *GetCurrentContext();
    auto *tool = &g.DebugIDStackTool;
    tool->LastActiveFrame = GetFrameCount();

    PushTextWrapPos(0);
    static constexpr bool ShowId = false;
    static constexpr u32 NumColumns = ShowId ? 3 : 2;

    if (!tool->Results.empty() && BeginTable("##table", NumColumns, ImGuiTableFlags_Borders)) {
        const float id_width = CalcTextSize("0xDDDDDDDD").x;
        if (ShowId) TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, id_width);
        TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("Help", ImGuiTableColumnFlags_WidthStretch);
        TableHeadersRow();
        for (int n = 0; n < tool->Results.Size; n++) {
            const auto *info = &tool->Results[n];
            if (ShowId) {
                TableNextColumn();
                Text("0x%08X", info->ID);
            }
            if (const auto it = HelpInfo::ById.find(info->ID); it != HelpInfo::ById.end()) {
                const auto &data = it->second;
                TableNextColumn();
                TextUnformatted(data.Name);
                TableNextColumn();
                TextUnformatted(data.Help.empty() ? "-" : data.Help);
            } else {
                TableNextColumn();
                Text("-");
                TableNextColumn();
                Text("-");
            }
        }
        EndTable();
    }
    PopTextWrapPos();

    // Copied from `imgui.cpp`.
    // Keeping this around for debugging.
    // if (!tool->Results.empty() && BeginTable("##table", 4, ImGuiTableFlags_Borders)) {
    //     const float id_width = CalcTextSize("0xDDDDDDDD").x;
    //     TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthFixed, id_width);
    //     TableSetupColumn("PushID", ImGuiTableColumnFlags_WidthStretch);
    //     TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, id_width);
    //     TableHeadersRow();
    //     for (int n = 0; n < tool->Results.Size; n++) {
    //         const auto *info = &tool->Results[n];
    //         TableNextColumn();
    //         Text("0x%08X", n > 0 ? tool->Results[n - 1].ID : 0);
    //         TableNextColumn();
    //         StackToolFormatLevelInfo(tool, n, true, g.TempBuffer.Data, g.TempBuffer.Size);
    //         TextUnformatted(g.TempBuffer.Data);
    //         TableNextColumn();
    //         Text("0x%08X", info->ID);
    //         if (n == tool->Results.Size - 1) TableSetBgColor(ImGuiTableBgTarget_CellBg, GetColorU32(ImGuiCol_Header));
    //     }
    //     EndTable();
    // }
}
