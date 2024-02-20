#pragma once

// This demo code is adapted from the [ImGuiFileDialog:main branch](https://github.com/aiekick/ImGuiFileDialog/blob/master/main.cpp)
// It is up-to-date as of https://github.com/aiekick/ImGuiFileDialog/commit/43daff00783dd1c4862d31e69a8186259ab1605b
// Demos related to the C interface have been removed.
namespace IGFD {
class FileDialog;
} // namespace IGFD

struct FileDialogImpl {
    IGFD::FileDialog *Dialog;

    void AddFonts();
    void Init();
    void Uninit();
};

extern FileDialogImpl FileDialogImp; // Defined in `main.cpp`. xxx bad
