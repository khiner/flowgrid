#include "FileDialogImpl.h"

#include "ImGuiFileDialog.h"

#include "imgui_internal.h"

#include "Helper/File.h"
#include "UI/Fonts.h"

void FileDialogImpl::AddFonts() {
    static const ImWchar IconRanges[] = {ICON_MIN_IGFD, ICON_MAX_IGFD, 0};
    ImFontConfig icons_config;
    icons_config.DstFont = ImGui::GetDefaultFont();
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15 * Fonts::AtlasScale, &icons_config, IconRanges);
}

void FileDialogImpl::Init() {
    Dialog = ImGuiFileDialog::Instance();

    // Singleton access
    Dialog->SetFileStyle(IGFD_FileStyleByFullName, "(Custom.+[.]h)", {1, 1, 0, 0.9f}); // use a regex
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".cpp", {1, 1, 0, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".hpp", {0, 0, 1, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".md", {1, 0, 1, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".png", {0, 1, 1, 0.9f}, ICON_IGFD_FILE_PIC); // add an icon for the filter type
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".gif", {0, 1, 0.5f, 0.9f}, "[GIF]"); // add an text for a filter type
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir, nullptr, {0.5f, 1, 0.9f, 0.9f}, ICON_IGFD_FOLDER); // for all dirs
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile, "CMakeLists.txt", {0.1f, 0.5f, 0.5f, 0.9f}, ICON_IGFD_ADD);
    Dialog->SetFileStyle(IGFD_FileStyleByFullName, "doc", {0.9f, 0.2f, 0, 0.9f}, ICON_IGFD_FILE_PIC);
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile, nullptr, {0.2f, 0.9f, 0.2f, 0.9f}, ICON_IGFD_FILE); // for all link files
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FOLDER); // for all link dirs
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FILE); // for all link files
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByContainedInFullName, ".git", {0.9f, 0.2f, 0, 0.9f}, ICON_IGFD_BOOKMARK);
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByContainedInFullName, ".git", {0.5f, 0.8f, 0.5f, 0.9f}, ICON_IGFD_SAVE);

#ifdef USE_BOOKMARK
    // Load bookmarks
    if (fs::exists("bookmarks.conf")) Dialog->DeserializeBookmarks(FileIO::read("bookmarks.conf"));
    Dialog->AddBookmark("Current dir", ".");
#endif
}

void FileDialogImpl::Uninit() {
#ifdef USE_THUMBNAILS
    Dialog->ManageGPUThumbnails();
#endif

#ifdef USE_BOOKMARK
    Dialog->RemoveBookmark("Current dir");
    FileIO::write("bookmarks_1.conf", Dialog->SerializeBookmarks());
#endif
}
