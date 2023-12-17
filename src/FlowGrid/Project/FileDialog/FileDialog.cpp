#include "FileDialog.h"

#include <imgui_internal.h>
#include <range/v3/range/conversion.hpp>

#include "FileDialogDataJson.h"
#include "Helper/File.h"
#include "ImGuiFileDialog.h"
#include "UI/Fonts.h"
#include "UI/HelpMarker.h"
#include "UI/Styling.h"

void FileDialog::Apply(const ActionType &action) const {
    // `SelectedFilePath` mutations are non-stateful side effects.
    Visit(
        action,
        [this](const Action::FileDialog::Open &a) {
            Set(json::parse(a.dialog_json));
        },
        [](const Action::FileDialog::Select &a) {
            SelectedFilePath = a.file_path;
        },
    );
}

bool FileDialog::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [](const Action::FileDialog::Open &) { return !Visible; },
        [](const Action::FileDialog::Select &) { return true; }, // File dialog `Visible` is set to false _before_ the select action is issued.
    );
}

void FileDialog::Set(const FileDialogData &data) const {
    SelectedFilePath = "";
    Visible = true;
    Title = data.title;
    Filters = data.filters;
    FilePath = data.file_path;
    DefaultFileName = data.default_file_name;
    SaveMode = data.save_mode;
    MaxNumSelections = data.max_num_selections;
    Flags = data.flags;
}

static void OpenDialog(const FileDialogData &data) { Action::FileDialog::Open{json(data).dump()}.q(); }

using namespace ImGui;

static IGFD::FileDialog *Dialog;

// Same as `ImGui::CheckboxFlags`, but with `help` arg.
bool CheckboxFlags(const char *label, int *flags, int flags_value, const char *help) {
    const bool result = ImGui::CheckboxFlags(label, flags, flags_value);
    SameLine();
    fg::HelpMarker(help);
    return result;
}

void IGFD::AddFonts() {
    static const ImWchar IconRanges[] = {ICON_MIN_IGFD, ICON_MAX_IGFD, 0};
    ImFontConfig icons_config;
    icons_config.DstFont = GetDefaultFont();
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15 * Fonts::AtlasScale, &icons_config, IconRanges);
}

void IGFD::Init() {
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

void FileDialog::Render() const {
    if (!Visible) return Dialog->Close();

    static const string DialogKey = "FileDialog";
    // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.

    ImGuiFileDialogFlags flags = Flags;
    if (SaveMode) flags |= ImGuiFileDialogFlags_ConfirmOverwrite;
    else flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;
    Dialog->OpenDialog(DialogKey, Title, Filters.c_str(), FilePath, DefaultFileName, MaxNumSelections, nullptr, flags);
    if (Dialog->Display(DialogKey, ImGuiWindowFlags_NoCollapse, GetMainViewport()->Size / 2)) {
        Visible = false;
        if (Dialog->IsOk()) Action::FileDialog::Select{Dialog->GetFilePathName()}.q();
    }
}

void FileDialog::Demo::Render() const {
#ifdef USE_EXPLORATION_BY_KEYS
    static float flash_attenuation_sec = 1.f;
    if (Button("R##resetflashlifetime")) {
        flash_attenuation_sec = 1.f;
        Dialog->SetFlashingAttenuationInSeconds(flash_attenuation_sec);
    }
    SameLine();
    PushItemWidth(200);
    if (SliderFloat("Flash lifetime (s)", &flash_attenuation_sec, 0.01f, 5.f)) {
        Dialog->SetFlashingAttenuationInSeconds(flash_attenuation_sec);
    }
    PopItemWidth();
#endif

    Separator();

    static ImGuiFileDialogFlags flags = FileDialogFlags_Modal;
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
        Unindent();
    }

    static const string ChooseFileOpen = ICON_IGFD_FOLDER_OPEN " Choose a file";
    static const string ChooseFileSave = ICON_IGFD_SAVE " Choose a file";
    static string FilePathName; // Keep track of the last chosen file. There's an option below to open this path.

    Text("Singleton access:");
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog")) {
        OpenDialog({ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with collections of filters")) {
        OpenDialog({ChooseFileOpen, "All files{.*},Source files (*.cpp *.h *.hpp){.cpp,.h,.hpp},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open all file types with \".*\" filter")) {
        OpenDialog({ChooseFileOpen, ".*", ".", FilePathName, false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open File Dialog with filter of type regex (Custom.+[.]h)")) {
        OpenDialog({ChooseFileOpen, "Regex Custom*.h{(Custom.+[.]h)}", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with selection of 5 items")) {
        OpenDialog({ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 5, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with infinite selection")) {
        OpenDialog({ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 0, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with most recent file path name")) {
        OpenDialog({ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", FilePathName, false, 1, flags});
    }

    if (Button(ICON_IGFD_SAVE " Save file dialog with confirm-overwrite dialog if file exists")) {
        OpenDialog({ChooseFileSave, "C/C++ file (*.c *.cpp){.c,.cpp}, Header file (*.h){.h}", ".", FilePathName, true, 1, flags | ImGuiFileDialogFlags_ConfirmOverwrite});
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
    //     Dialog->OpenDialog(key, choose_file_save, filters,
    //         ".", "", [](auto &&PH1, auto &&PH2, auto &&PH3) { return InfosPane(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); }, 350, 1,
    //         save_file_user_data, flags);
    // }

    FilePathName = Dialog->GetFilePathName();
    static string file_path = Dialog->GetCurrentPath();
    static string user_data = Dialog->GetUserDatas() ? string((const char *)Dialog->GetUserDatas()) : "";

    Separator();

    TextUnformatted("State:\n");
    Indent();
    {
        TextUnformatted(std::format("FilePathName: {}", FilePathName).c_str());
        TextUnformatted(std::format("FilePath: {}", file_path).c_str());
        TextUnformatted(std::format("Filters: {}", Dialog->GetCurrentFilter()).c_str());
        TextUnformatted(std::format("UserDatas: {}", user_data).c_str());
        TextUnformatted("Selection: ");
        Indent();
        {
            if (BeginTable("##GetSelection", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                TableSetupScrollFreeze(0, 1); // Make top row always visible
                TableSetupColumn("File name", ImGuiTableColumnFlags_WidthStretch, -1, 0);
                TableSetupColumn("File path name", ImGuiTableColumnFlags_WidthFixed, -1, 1);
                TableHeadersRow();

                static int selected = 0;
                const auto &selection = Dialog->GetSelection();
                const auto selection_keys = selection | std::views::keys | ranges::to<std::vector>();
                ImGuiListClipper clipper;
                clipper.Begin(int(selection.size()), GetTextLineHeightWithSpacing());
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto &selection_key = selection_keys[i];
                        TableNextRow();
                        if (TableSetColumnIndex(0)) {
                            ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
                            flags |= ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                            if (Selectable(selection_key.c_str(), i == selected, flags)) selected = i;
                        }
                        if (TableSetColumnIndex(1)) {
                            TextUnformatted(selection.at(selection_key).c_str());
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

void IGFD::Uninit() {
#ifdef USE_THUMBNAILS
    Dialog->ManageGPUThumbnails();
#endif

#ifdef USE_BOOKMARK
    Dialog->RemoveBookmark("Current dir");
    FileIO::write("bookmarks_1.conf", Dialog->SerializeBookmarks());
#endif
}
