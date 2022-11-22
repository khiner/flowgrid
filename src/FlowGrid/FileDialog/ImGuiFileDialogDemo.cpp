#include <sstream>

#include "ImGuiFileDialogDemo.h"
#include "ImGuiFileDialog.h"

#include "../StateJson.h"

using namespace ImGui;

ImGuiFileDialog *dialog = ImGuiFileDialog::Instance();

// todo move `fg::HelpMarker` to a new header that only requires imgui and use it here
void HelpMarker(const char *help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help);
        PopTextWrapPos();
        EndTooltip();
    }
}

// Same as `ImGui::CheckboxFlags`, but with `help` arg.
bool CheckboxFlags(const char *label, int *flags, int flags_value, const char *help) {
    const bool result = ImGui::CheckboxFlags(label, flags, flags_value);
    SameLine();
    HelpMarker(help);
    return result;
}

void IGFD::InitializeDemo() {
#ifdef USE_THUMBNAILS
    dialog->SetCreateThumbnailCallback([](IGFD_Thumbnail_Info *thumbnail_info) -> void
    {
        if (thumbnail_info && thumbnail_info->isReadyToUpload && thumbnail_info->textureFileDatas) {
            GLuint textureId = 0;
            glGenTextures(1, &textureId);
            thumbnail_info->textureID = (void*)(size_t)textureId;

            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                (GLsizei)thumbnail_info->textureWidth, (GLsizei)thumbnail_info->textureHeight,
                0, GL_RGBA, GL_UNSIGNED_BYTE, thumbnail_info->textureFileDatas);
            glFinish();
            glBindTexture(GL_TEXTURE_2D, 0);

            delete[] thumbnail_info->textureFileDatas;
            thumbnail_info->textureFileDatas = nullptr;

            thumbnail_info->isReadyToUpload = false;
            thumbnail_info->isReadyToDisplay = true;
        }
    });
    dialog->SetDestroyThumbnailCallback([](IGFD_Thumbnail_Info* thumbnail_info)
    {
        if (thumbnail_info) {
            GLuint tex_id = (GLuint)(size_t)thumbnail_info->textureID;
            glDeleteTextures(1, &tex_id);
            glFinish();
        }
    });
#endif // USE_THUMBNAILS

    static const ImWchar icons_ranges[] = {ICON_MIN_IGFD, ICON_MAX_IGFD, 0};
    ImFontConfig icons_config;
    icons_config.DstFont = GetDefaultFont();
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15, &icons_config, icons_ranges);

    // Singleton access
    dialog->SetFileStyle(IGFD_FileStyleByFullName, "(Custom.+[.]h)", {1, 1, 0, 0.9f}); // use a regex
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".cpp", {1, 1, 0, 0.9f});
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".hpp", {0, 0, 1, 0.9f});
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".md", {1, 0, 1, 0.9f});
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".png", {0, 1, 1, 0.9f}, ICON_IGFD_FILE_PIC); // add an icon for the filter type
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".gif", {0, 1, 0.5f, 0.9f}, "[GIF]"); // add an text for a filter type
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir, nullptr, {0.5f, 1, 0.9f, 0.9f}, ICON_IGFD_FOLDER); // for all dirs
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile, "CMakeLists.txt", {0.1f, 0.5f, 0.5f, 0.9f}, ICON_IGFD_ADD);
    dialog->SetFileStyle(IGFD_FileStyleByFullName, "doc", {0.9f, 0.2f, 0, 0.9f}, ICON_IGFD_FILE_PIC);
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile, nullptr, {0.2f, 0.9f, 0.2f, 0.9f}, ICON_IGFD_FILE); // for all link files
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FOLDER); // for all link dirs
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FILE); // for all link files
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByContainedInFullName, ".git", {0.9f, 0.2f, 0, 0.9f}, ICON_IGFD_BOOKMARK);
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByContainedInFullName, ".git", {0.5f, 0.8f, 0.5f, 0.9f}, ICON_IGFD_SAVE);

#ifdef USE_BOOKMARK
    // Load bookmarks
    if (fs::exists("bookmarks.conf")) dialog->DeserializeBookmarks(FileIO::read("bookmarks.conf"));
    dialog->AddBookmark("Current dir", ".");
#endif
}

void OpenDialog(const FileDialogData &data) { q(open_file_dialog{json(data).dump()}); }

void IGFD::ShowDemoWindow() {
#ifdef USE_EXPLORATION_BY_KEYS
    static float flash_attenuation_sec = 1.f;
    if (Button("R##resetflashlifetime")) {
        flash_attenuation_sec = 1.f;
        dialog->SetFlashingAttenuationInSeconds(flash_attenuation_sec);
    }
    SameLine();
    PushItemWidth(200);
    if (SliderFloat("Flash lifetime (s)", &flash_attenuation_sec, 0.01f, 5.f)) {
        dialog->SetFlashingAttenuationInSeconds(flash_attenuation_sec);
    }
    PopItemWidth();
#endif

    Separator();

    static ImGuiFileDialogFlags flags = FileDialogFlags_Default;
    {
        Text("ImGuiFileDialog flags: ");
        Indent();
        CheckboxFlags("Overwrite", &flags, ImGuiFileDialogFlags_ConfirmOverwrite, "Overwrite verification before dialog closing");
        CheckboxFlags("Hide hidden files", &flags, ImGuiFileDialogFlags_DontShowHiddenFiles, "Hide hidden files");
        CheckboxFlags("Case-insensitive extensions", &flags, ImGuiFileDialogFlags_CaseInsensitiveExtention, "Don't take into account the case of file extensions");
        CheckboxFlags("Disable directory creation", &flags, ImGuiFileDialogFlags_DisableCreateDirectoryButton, "Disable directory creation button in dialog");
#ifdef USE_THUMBNAILS
        CheckboxFlags("Disable thumbnails mode", &flags, ImGuiFileDialogFlags_DisableThumbnailMode, "Disable thumbnails display in dialog");
#endif
#ifdef USE_BOOKMARK
        CheckboxFlags("Disable bookmark mode", &flags, ImGuiFileDialogFlags_DisableBookmarkMode, "Disable bookmark display in dialog");
#endif

        Spacing();
        Text("Hide columns by default:");
        CheckboxFlags("Hide 'Type' column", &flags, ImGuiFileDialogFlags_HideColumnType);
        CheckboxFlags("Hide 'Size' column", &flags, ImGuiFileDialogFlags_HideColumnSize);
        CheckboxFlags("Hide 'Date' column", &flags, ImGuiFileDialogFlags_HideColumnDate);
        Unindent();
    }

    static string choose_file_open = ICON_IGFD_FOLDER_OPEN " Choose a file";
    static const string choose_file_save = ICON_IGFD_SAVE " Choose a file";
    static string file_path_name; // Keep track of the last chosen file. There's an option below to open this path.

    Text("Singleton access:");
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog")) {
        OpenDialog({choose_file_open, ".*,.cpp,.h,.hpp", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with collections of filters")) {
        OpenDialog({choose_file_open, "All files{.*},Source files (*.cpp *.h *.hpp){.cpp,.h,.hpp},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open all file types with \".*\" filter")) {
        OpenDialog({choose_file_open, ".*", ".", file_path_name, false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open File Dialog with filter of type regex (Custom.+[.]h)")) {
        OpenDialog({choose_file_open, "Regex Custom*.h{(Custom.+[.]h)}", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with selection of 5 items")) {
        OpenDialog({choose_file_open, ".*,.cpp,.h,.hpp", ".", "", false, 5, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with infinite selection")) {
        OpenDialog({choose_file_open, ".*,.cpp,.h,.hpp", ".", "", false, 0, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with most recent file path name")) {
        OpenDialog({choose_file_open, ".*,.cpp,.h,.hpp", ".", file_path_name, false, 1, flags});
    }

    if (Button(ICON_IGFD_SAVE " Save file dialog with confirm-overwrite dialog if file exists")) {
        OpenDialog({choose_file_save, "C/C++ file (*.c *.cpp){.c,.cpp}, Header file (*.h){.h}", ".", file_path_name, true, 1, flags | ImGuiFileDialogFlags_ConfirmOverwrite});
    }

    // Keeping this around to remind myself that custom panes & UserDatas are a thing.
    // If `cant_continue` is false, the user can't validate the dialog.
    // static bool can_validate_dialog = false;
    // inline void InfosPane(const char *filter, IGFDUserDatas user_data, bool *cant_continue) {
    //     TextColored(ImVec4(0, 1, 1, 1), "Infos Pane");
    //     Text("Selected Filter: %s", filter);
    //     if (user_data) Text("User Data: %s", (const char *) user_data);
    //     Checkbox("If not checked, you can't validate the dialog", &can_validate_dialog);
    //     if (cant_continue) *cant_continue = can_validate_dialog;
    // }
    // auto save_file_user_data = IGFDUserDatas("SaveFile");
    // if (Button(ICON_IGFD_SAVE " Save file dialog with a custom pane")) {
    //     const char *filters = "C++ File (*.cpp){.cpp}";
    //     dialog->OpenDialog(key, choose_file_save, filters,
    //         ".", "", [](auto &&PH1, auto &&PH2, auto &&PH3) { return InfosPane(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); }, 350, 1,
    //         save_file_user_data, flags);
    // }

    file_path_name = dialog->GetFilePathName();
    static string file_path = dialog->GetCurrentPath();
    static string user_data = dialog->GetUserDatas() ? string((const char *) dialog->GetUserDatas()) : "";

    // Convert from map to vector of pairs. TODO use `ranges::view` piped transform
    const auto &selections = dialog->GetSelection();
    static vector<std::pair<string, string>> selection = {};
    selection.clear();
    for (const auto &sel: selections) selection.emplace_back(sel.first, sel.second);

    Separator();

    TextUnformatted("FileDialog state:\n");
    Indent();
    {
        TextUnformatted(format("FilePathName: {}", file_path_name).c_str());
        TextUnformatted(format("FilePath: {}", file_path).c_str());
        TextUnformatted(format("Filters: {}", string(s.FileDialog.Filters)).c_str());
        TextUnformatted(format("UserDatas: {}", user_data).c_str());
        TextUnformatted("Selection: ");
        Indent();
        {
            static int selected = false;
            if (BeginTable("##GetSelection", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                TableSetupScrollFreeze(0, 1); // Make top row always visible
                TableSetupColumn("File name", ImGuiTableColumnFlags_WidthStretch, -1, 0);
                TableSetupColumn("File path name", ImGuiTableColumnFlags_WidthFixed, -1, 1);
                TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin((int) selection.size(), GetTextLineHeightWithSpacing());
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto &sel = selection[i];
                        TableNextRow();
                        if (TableSetColumnIndex(0)) {
                            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_AllowDoubleClick;
                            selectableFlags |= ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                            if (Selectable(sel.first.c_str(), i == selected, selectableFlags)) selected = i;
                        }
                        if (TableSetColumnIndex(1)) {
                            TextUnformatted(sel.second.c_str());
                        }
                    }
                }
                clipper.End();

                EndTable();
            }
        }
        Unindent();
    }
    Unindent();
}

void IGFD::CleanupDemo() {
#ifdef USE_THUMBNAILS
    dialog->ManageGPUThumbnails();
#endif

#ifdef USE_BOOKMARK
    dialog->RemoveBookmark("Current dir");
    FileIO::write("bookmarks_1.conf", dialog->SerializeBookmarks());
#endif
}
