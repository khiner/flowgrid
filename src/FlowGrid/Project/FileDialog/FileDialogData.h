#pragma once

#include <string>

using ID = unsigned int;

using ImGuiFileDialogFlags = int;
// Copied from `ImGuiFileDialog` source with a different name to avoid redefinition. Brittle but we can avoid an include this way.
constexpr ImGuiFileDialogFlags FileDialogFlags_ConfirmOverwrite = 1 << 0;
constexpr ImGuiFileDialogFlags FileDialogFlags_Modal = 1 << 9;

struct FileDialogData {
    ID owner_id;
    std::string title{"Choose file"};
    std::string filters{".*"};
    std::string file_path{"."};
    std::string default_file_name{""};
    bool save_mode{false};
    int max_num_selections{1};
    ImGuiFileDialogFlags flags{FileDialogFlags_Modal};
};
