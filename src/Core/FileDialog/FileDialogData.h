#pragma once

#include <string>

using ID = unsigned int;

using ImGuiFileDialogFlags = int;
// Copied from `ImGuiFileDialog` source with a different name to avoid redefinition. Brittle but we can avoid an include this way.
constexpr ImGuiFileDialogFlags FileDialogFlags_ConfirmOverwrite = 1 << 0;
constexpr ImGuiFileDialogFlags FileDialogFlags_Modal = 1 << 9;

struct FileDialogData {
    ID OwnerId;
    std::string Title{"Choose file"};
    std::string Filters{".*"};
    std::string FilePath{"."};
    std::string DefaultFileName{""};
    bool SaveMode{false};
    int MaxNumSelections{1};
    ImGuiFileDialogFlags Flags{FileDialogFlags_Modal};
};
