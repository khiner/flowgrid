#include "FileDialog.h"
#include "FileDialogDemo.h"

#include <ranges>

#include "ImGuiFileDialog.h"
#include "imgui_internal.h"

#include "Core/Helper/File.h"
#include "Core/UI/Fonts.h"
#include "Core/UI/HelpMarker.h"
#include "FileDialogDataJson.h"
#include "FileDialogManager.h"

using std::views::keys, std::ranges::to;

static IGFD::FileDialog *Dialog;

void FileDialog::Set(FileDialogData &&data) const {
    Visible = true;
    SelectedFilePath = "";
    Data = std::move(data);
}

void FileDialog::SetJson(json &&j) const { Set(std::move(j)); }

using namespace ImGui;

// Same as `ImGui::CheckboxFlags`, but with `help` arg.
bool CheckboxFlags(const char *label, int *flags, int flags_value, const char *help) {
    const bool result = ImGui::CheckboxFlags(label, flags, flags_value);
    SameLine();
    flowgrid::HelpMarker(help);
    return result;
}

void FileDialog::Render() const {
    if (!Visible) return Dialog->Close();

    static const std::string DialogKey{"FileDialog"};
    // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.

    ImGuiFileDialogFlags flags = Data.Flags;
    if (Data.SaveMode) flags |= ImGuiFileDialogFlags_ConfirmOverwrite;
    else flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;

    IGFD::FileDialogConfig config;
    config.path = Data.FilePath;
    config.countSelectionMax = Data.MaxNumSelections;
    config.flags = flags;
    config.filePathName = Data.DefaultFileName;
    Dialog->OpenDialog(DialogKey, Data.Title, Data.Filters.c_str(), config);
    if (Dialog->Display(DialogKey, ImGuiWindowFlags_NoCollapse, GetMainViewport()->Size / 2)) {
        Visible = false;
        if (Dialog->IsOk()) Q(Action::FileDialog::Select{Dialog->GetFilePathName()});
    }
}

// This demo code is adapted from the [ImGuiFileDialog:main branch](https://github.com/aiekick/ImGuiFileDialog/blob/master/main.cpp)
// It is up-to-date as of https://github.com/aiekick/ImGuiFileDialog/commit/43daff00783dd1c4862d31e69a8186259ab1605b
// Demos related to the C interface have been removed.

void FileDialogDemo::OpenDialog(const FileDialogData &data) const { Q(Action::FileDialog::Open{json(data).dump()}); }

void FileDialogDemo::Render() const {
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
        CheckboxFlags("Disable directory creation", &flags, ImGuiFileDialogFlags_DisableCreateDirectoryButton, "Disable directory creation button in dialog");
#ifdef USE_PLACES_FEATURE
        CheckboxFlags("Disable place mode", &flags, ImGuiFileDialogFlags_DisablePlaceMode, "Disable place display in dialog");
#endif // USE_PLACES_FEATURE

        SeparatorText("Default-hidden columns");
        CheckboxFlags("Type", &flags, ImGuiFileDialogFlags_HideColumnType, "Hide Type column by default");
        CheckboxFlags("Size", &flags, ImGuiFileDialogFlags_HideColumnSize, "Hide Size column by default");
        CheckboxFlags("Date", &flags, ImGuiFileDialogFlags_HideColumnDate, "Hide Date column by default");

        Separator();
        CheckboxFlags("Case insensitive extentions filtering", &flags, ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering, "Ignore file extention case for filtering");
        CheckboxFlags("Disable quick path selection", &flags, ImGuiFileDialogFlags_DisableQuickPathSelection, nullptr);
        CheckboxFlags("Show devices button", &flags, ImGuiFileDialogFlags_ShowDevicesButton, nullptr);
        CheckboxFlags("Natural sorting", &flags, ImGuiFileDialogFlags_NaturalSorting, nullptr);

        Separator();
        Text("Result modes for GetFilePathName and GetSelection");
        static IGFD_ResultMode resultMode = IGFD_ResultMode_AddIfNoFileExt;
        CheckboxFlags("Add if no file ext", &resultMode, IGFD_ResultMode_::IGFD_ResultMode_AddIfNoFileExt, nullptr);
        CheckboxFlags("Overwrite file ext", &resultMode, IGFD_ResultMode_::IGFD_ResultMode_OverwriteFileExt, nullptr);
        CheckboxFlags("Keep input file", &resultMode, IGFD_ResultMode_::IGFD_ResultMode_KeepInputFile, nullptr);
        Unindent();
    }

    static const std::string ChooseFileOpen = ICON_IGFD_FOLDER_OPEN " Choose a file";
    static const std::string ChooseFileSave = ICON_IGFD_SAVE " Choose a file";
    static std::string FilePathName; // Keep track of the last chosen file. There's an option below to open this path.

    Text("Singleton access:");
    // todo update to bring in anything relevant that's new in https://github.com/aiekick/ImGuiFileDialog/blob/DemoApp/src/gui/DemoDialog.cpp#L595
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog")) {
        OpenDialog({Id, ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with collections of Filters")) {
        OpenDialog({Id, ChooseFileOpen, "All files{.*},Source files (*.cpp *.h *.hpp){.cpp,.h,.hpp},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open all file types with \".*\" filter")) {
        OpenDialog({Id, ChooseFileOpen, ".*", ".", FilePathName, false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open File Dialog with filter of type regex (Custom.+[.]h)")) {
        OpenDialog({Id, ChooseFileOpen, "Regex Custom*.h{(Custom.+[.]h)}", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with selection of 5 items")) {
        OpenDialog({Id, ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 5, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with infinite selection")) {
        OpenDialog({Id, ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 0, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with most recent file path name")) {
        OpenDialog({Id, ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", FilePathName, false, 1, flags});
    }

    if (Button(ICON_IGFD_SAVE " Save file dialog with confirm-overwrite dialog if file exists")) {
        OpenDialog({Id, ChooseFileSave, "C/C++ file (*.c *.cpp){.c,.cpp}, Header file (*.h){.h}", ".", FilePathName, true, 1, flags | ImGuiFileDialogFlags_ConfirmOverwrite});
    }

    // Keeping this around to remind myself that custom panes & UserDatas are a thing.
    // If `cant_continue` is false, the user can't validate the dialog.
    // static bool can_validate_dialog = false;
    // void InfosPane(const char *filter, IGFDUserDatas user_data, bool *cant_continue) {
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
    static const std::string file_path = Dialog->GetCurrentPath();
    static const std::string user_data = Dialog->GetUserDatas() ? std::string((const char *)Dialog->GetUserDatas()) : "";

    Separator();

    TextUnformatted("State:\n");
    Indent();
    {
        Text("FilePathName: %s", FilePathName.c_str());
        Text("FilePath: %s", file_path.c_str());
        Text("Filters: %s", Dialog->GetCurrentFilter().c_str());
        Text("UserDatas: %s", user_data.c_str());
        TextUnformatted("Selection: ");
        Indent();
        {
            if (BeginTable("##GetSelection", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                TableSetupScrollFreeze(0, 1); // Make top row always visible
                TableSetupColumn("File name", ImGuiTableColumnFlags_WidthStretch, -1, 0);
                TableSetupColumn("File path name", ImGuiTableColumnFlags_WidthFixed, -1, 1);
                TableHeadersRow();

                static int selected = 0;
                const auto selection = Dialog->GetSelection();
                const auto selection_keys = selection | keys | to<std::vector>();
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
                            TextUnformatted(selection.at(selection_key));
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

void FileDialogManager::Init() {
    // Add fonts
    constexpr ImWchar IconRanges[] = {ICON_MIN_IGFD, ICON_MAX_IGFD, 0};
    ImFontConfig icons_config;
    icons_config.DstFont = ImGui::GetDefaultFont();
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, 15 * Fonts::AtlasScale, &icons_config, IconRanges);

    Dialog = ImGuiFileDialog::Instance();

    // Singleton access
    Dialog->SetFileStyle(IGFD_FileStyleByFullName, "(Custom.+[.]h)", {0.1, 0.9, 0.1, 0.9f}); // use a regex
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".cpp", {1, 1, 0, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".hpp", {0, 0, 1, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".md", {1, 0, 1, 0.9f});
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".png", {0, 1, 1, 0.9f}, ICON_IGFD_FILE_PIC);
    Dialog->SetFileStyle(IGFD_FileStyleByExtention, ".gif", {0, 1, 0.5f, 0.9f}, "[GIF]"); // add an text for a filter type
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir, nullptr, {0.5f, 1, 0.9f, 0.9f}, ICON_IGFD_FOLDER); // for all dirs
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile, "CMakeLists.txt", {0.1, 0.5, 0.5, 0.9f}, ICON_IGFD_ADD);
    Dialog->SetFileStyle(IGFD_FileStyleByFullName, "doc", {0.9, 0.2, 0, 0.9f}, ICON_IGFD_FILE_PIC);
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile, nullptr, {0.2, 0.9, 0.2, 0.9f}, ICON_IGFD_FILE); // for all link files
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FOLDER); // for all link dirs
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByTypeLink, nullptr, {0.8f, 0.8f, 0.8f, 0.8f}, ICON_IGFD_FILE); // for all link files
    Dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByContainedInFullName, ".git", {0.9f, 0.2f, 0, 0.9f}, ICON_IGFD_BOOKMARK);
    Dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByContainedInFullName, ".git", {0.5f, 0.8f, 0.5f, 0.9f}, ICON_IGFD_SAVE);

#ifdef USE_PLACES_FEATURE
    // load place
    std::ifstream docFile_1("places_1.conf", std::ios::in);
    if (docFile_1.is_open()) {
        std::stringstream strStream;
        strStream << docFile_1.rdbuf(); // read the file
        Dialog->DeserializePlaces(strStream.str());
        docFile_1.close();
    }

    std::ifstream docFile_2("places_2.conf", std::ios::in);
    if (docFile_2.is_open()) {
        std::stringstream strStream;
        strStream << docFile_2.rdbuf(); // read the file
        fileDialog2.DeserializePlaces(strStream.str());
        docFile_2.close();
    }

    // c interface
    std::ifstream docFile_c("places_c.conf", std::ios::in);
    if (docFile_c.is_open()) {
        std::stringstream strStream;
        strStream << docFile_c.rdbuf(); // read the file
        IGFD_DeserializePlaces(cFileDialogPtr, strStream.str().c_str());
        docFile_c.close();
    }

    // add places :
    const char *group_name = ICON_IGFD_SHORTCUTS " Places";
    Dialog->AddPlacesGroup(group_name, 1, false);
    IGFD_AddPlacesGroup(cFileDialogPtr, group_name, 1, false);

    // Places :
    auto places_ptr = Dialog->GetPlacesGroupPtr(group_name);
    if (places_ptr != nullptr) {
#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(__WIN64__) || defined(WIN64) || defined(_WIN64) || defined(_MSC_VER)
#define addKnownFolderAsPlace(knownFolder, folderLabel, folderIcon)                                        \
    {                                                                                                      \
        PWSTR path = NULL;                                                                                 \
        HRESULT hr = SHGetKnownFolderPath(knownFolder, 0, NULL, &path);                                    \
        if (SUCCEEDED(hr)) {                                                                               \
            IGFD::FileStyle style;                                                                         \
            style.icon = folderIcon;                                                                       \
            auto place_path = IGFD::Utils::UTF8Encode(path);                                               \
            places_ptr->AddPlace(folderLabel, place_path, false, style);                                   \
            IGFD_AddPlace(cFileDialogPtr, group_name, folderLabel, place_path.c_str(), false, folderIcon); \
        }                                                                                                  \
        CoTaskMemFree(path);                                                                               \
    }
        addKnownFolderAsPlace(FOLDERID_Desktop, "Desktop", ICON_IGFD_DESKTOP);
        addKnownFolderAsPlace(FOLDERID_Startup, "Startup", ICON_IGFD_HOME);
        places_ptr->AddPlaceSeparator(3.0f); // add a separator
        addKnownFolderAsPlace(FOLDERID_Downloads, "Downloads", ICON_IGFD_DOWNLOADS);
        addKnownFolderAsPlace(FOLDERID_Pictures, "Pictures", ICON_IGFD_PICTURE);
        addKnownFolderAsPlace(FOLDERID_Music, "Music", ICON_IGFD_MUSIC);
        addKnownFolderAsPlace(FOLDERID_Videos, "Videos", ICON_IGFD_FILM);
#undef addKnownFolderAsPlace
#else
#endif
        places_ptr = nullptr;
    }

    // add place by code (why ? because we can :-) )
    // ImGuiFileDialog->
    // todo : do the code
    // Dialog->AddPlace("Current Dir", ".");
#endif // USE_PLACES_FEATURE
}

void FileDialogManager::Uninit() {
#ifdef USE_PLACES_FEATURE
    // remove place
    // todo : do the code
    // Dialog->RemovePlace("Current Dir");

    // save place dialog 1
    std::ofstream configFileWriter_1("places_1.conf", std::ios::out);
    if (!configFileWriter_1.bad()) {
        configFileWriter_1 << Dialog->SerializePlaces();
        configFileWriter_1.close();
    }
    // save place dialog 2
    std::ofstream configFileWriter_2("places_2.conf", std::ios::out);
    if (!configFileWriter_2.bad()) {
        configFileWriter_2 << fileDialog2.SerializePlaces();
        configFileWriter_2.close();
    }
    // save place dialog c interface
    std::ofstream configFileWriter_c("places_c.conf", std::ios::out);
    if (!configFileWriter_c.bad()) {
        char *s = IGFD_SerializePlaces(cFileDialogPtr, true);
        if (s) {
            configFileWriter_c << std::string(s);
            configFileWriter_c.close();
        }
    }
#endif
}
