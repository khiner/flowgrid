#include <sstream>

#include "ImGuiFileDialogDemo.h"
#include "ImGuiFileDialog.h"

#include "../StateJson.h"

using namespace ImGui;

inline bool RadioButtonLabeled(const char *label, const char *help, bool active, bool disabled) {
    using namespace ImGui;

    ImGuiWindow *window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext &g = *GImGui;
    const ImGuiStyle &style = g.Style;
    float w = CalcItemWidth();
    if (w == window->ItemWidthDefault) w = 0; // no push item width
    const ID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, nullptr, true);
    ImVec2 bb_size = ImVec2(style.FramePadding.x * 2 - 1, style.FramePadding.y * 2 - 1) + label_size;
    bb_size.x = ImMax(w, bb_size.x);

    const ImRect check_bb(window->DC.CursorPos, window->DC.CursorPos + bb_size);
    ItemSize(check_bb, style.FramePadding.y);

    if (!ItemAdd(check_bb, id)) return false;

    // check
    bool pressed = false;
    if (!disabled) {
        bool hovered, held;
        pressed = ButtonBehavior(check_bb, id, &hovered, &held);

        window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), style.FrameRounding);
        if (active) {
            const ImU32 col = GetColorU32((hovered && held) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, col, style.FrameRounding);
        }
    }

    // circle shadow + bg
    if (style.FrameBorderSize > 0) {
        window->DrawList->AddRect(check_bb.Min + ImVec2(1, 1), check_bb.Max, GetColorU32(ImGuiCol_BorderShadow), style.FrameRounding);
        window->DrawList->AddRect(check_bb.Min, check_bb.Max, GetColorU32(ImGuiCol_Border), style.FrameRounding);
    }

    if (label_size.x > 0) RenderText(check_bb.GetCenter() - label_size * 0, label);
    if (help && IsItemHovered())SetTooltip("%s", help);

    return pressed;
}

template<typename T>
inline bool RadioButtonLabeled_BitWise(
    const char *label, const char *help, T *container, T flag,
    bool oneOrZeroAtTime = false, // only one selected at a time
    bool alwaysOne = true, // radio behavior, always one selected
    T flagsToTakeIntoAccount = (T) 0,
    bool disableSelection = false) {

    bool selected = (*container) & flag;
    const bool res = RadioButtonLabeled(label, help, selected, disableSelection);
    if (res) {
        if (!selected) {
            if (oneOrZeroAtTime) {
                if (flagsToTakeIntoAccount) {
                    if (flag & flagsToTakeIntoAccount) {
                        *container = (T) (*container & ~flagsToTakeIntoAccount); // remove these flags
                        *container = (T) (*container | flag); // add
                    }
                } else *container = flag; // set
            } else {
                if (flagsToTakeIntoAccount) {
                    if (flag & flagsToTakeIntoAccount) {
                        *container = (T) (*container & ~flagsToTakeIntoAccount); // remove these flags
                        *container = (T) (*container | flag); // add
                    }
                } else *container = (T) (*container | flag); // add
            }
        } else {
            if (oneOrZeroAtTime) {
                if (!alwaysOne) *container = (T) (0); // remove all
            } else *container = (T) (*container & ~flag); // remove one
        }
    }
    return res;
}

ImGuiFileDialog *dialog = ImGuiFileDialog::Instance();

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
            GLuint texID = (GLuint)(size_t)thumbnail_info->textureID;
            glDeleteTextures(1, &texID);
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
    dialog->SetFileStyle(IGFD_FileStyleByFullName, "(Custom.+[.]h)", ImVec4(1, 1, 0, 0.9f)); // use a regex
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".cpp", ImVec4(1, 1, 0, 0.9f));
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".hpp", ImVec4(0, 0, 1, 0.9f));
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".md", ImVec4(1, 0, 1, 0.9f));
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".png", ImVec4(0, 1, 1, 0.9f), ICON_IGFD_FILE_PIC); // add an icon for the filter type
    dialog->SetFileStyle(IGFD_FileStyleByExtention, ".gif", ImVec4(0, 1, 0.5f, 0.9f), "[GIF]"); // add an text for a filter type
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir, nullptr, ImVec4(0.5f, 1, 0.9f, 0.9f), ICON_IGFD_FOLDER); // for all dirs
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile, "CMakeLists.txt", ImVec4(0.1f, 0.5f, 0.5f, 0.9f), ICON_IGFD_ADD);
    dialog->SetFileStyle(IGFD_FileStyleByFullName, "doc", ImVec4(0.9f, 0.2f, 0, 0.9f), ICON_IGFD_FILE_PIC);
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile, nullptr, ImVec4(0.2f, 0.9f, 0.2f, 0.9f), ICON_IGFD_FILE); // for all link files
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByTypeLink, nullptr, ImVec4(0.8f, 0.8f, 0.8f, 0.8f), ICON_IGFD_FOLDER); // for all link dirs
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByTypeLink, nullptr, ImVec4(0.8f, 0.8f, 0.8f, 0.8f), ICON_IGFD_FILE); // for all link files
    dialog->SetFileStyle(IGFD_FileStyleByTypeDir | IGFD_FileStyleByContainedInFullName, ".git", ImVec4(0.9f, 0.2f, 0, 0.9f), ICON_IGFD_BOOKMARK);
    dialog->SetFileStyle(IGFD_FileStyleByTypeFile | IGFD_FileStyleByContainedInFullName, ".git", ImVec4(0.5f, 0.8f, 0.5f, 0.9f), ICON_IGFD_SAVE);

#ifdef USE_BOOKMARK
    // Load bookmarks
    std::ifstream docFile_1("bookmarks.conf", std::ios::in);
    if (docFile_1.is_open()) {
        std::stringstream strStream;
        strStream << docFile_1.rdbuf();//read the file
        dialog->DeserializeBookmarks(strStream.str());
        docFile_1.close();
    }

    // Add bookmark by code
    dialog->AddBookmark("Current dir", ".");
#endif
}

void OpenDialog(const FileDialogData &data) {
    q(open_file_dialog{json(data).dump()});
}

void IGFD::ShowDemoWindow() {
#ifdef USE_EXPLORATION_BY_KEYS
    static float flashingAttenuationInSeconds = 1.0f;
    if (Button("R##resetflashlifetime")) {
        flashingAttenuationInSeconds = 1.0f;
        dialog->SetFlashingAttenuationInSeconds(flashingAttenuationInSeconds);
    }
    SameLine();
    PushItemWidth(200);
    if (SliderFloat("Flash lifetime (s)", &flashingAttenuationInSeconds, 0.01f, 5.0f)) {
        dialog->SetFlashingAttenuationInSeconds(flashingAttenuationInSeconds);
    }
    PopItemWidth();
#endif

    Separator();

    static ImGuiFileDialogFlags flags = ImGuiFileDialogFlags_Default;
    Text("ImGuiFileDialog flags: ");
    Indent();
    {
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Overwrite", "Overwrite verification before dialog closing", &flags, ImGuiFileDialogFlags_ConfirmOverwrite);
        SameLine();
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Hide hidden files", "Hide hidden files", &flags, ImGuiFileDialogFlags_DontShowHiddenFiles);

        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Disable directory creation", "Disable directory creation button in dialog", &flags, ImGuiFileDialogFlags_DisableCreateDirectoryButton);
#ifdef USE_THUMBNAILS
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Disable thumbnails mode", "Disable thumbnails display in dialog", &flags, ImGuiFileDialogFlags_DisableThumbnailMode);
#endif
#ifdef USE_BOOKMARK
        SameLine();
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Disable bookmark mode", "Disable bookmark display in dialog", &flags, ImGuiFileDialogFlags_DisableBookmarkMode);
#endif

        Text("Hide Column by default: (saved in imgui.ini, \n\tso defined when the imgui.ini does not exist)");
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Hide 'Type' column", "Hide file type by default", &flags, ImGuiFileDialogFlags_HideColumnType);
        SameLine();
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Hide 'Size' column", "Hide file size by default", &flags, ImGuiFileDialogFlags_HideColumnSize);
        SameLine();
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Hide 'Date' column", "Hide file date by default", &flags, ImGuiFileDialogFlags_HideColumnDate);
        RadioButtonLabeled_BitWise<ImGuiFileDialogFlags>("Case-insensitive extensions", "will not take into account the case of file extensions", &flags, ImGuiFileDialogFlags_CaseInsensitiveExtention);
    }
    Unindent();

    static string filePathName; // Keep track of the last chosen file. There's an option below to open this path.
    static string chooseFile = ICON_IGFD_FOLDER_OPEN " Choose a file";
    static const char *chooseFileSave = ICON_IGFD_SAVE " Choose a file";

    Text("Singleton access:");
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog")) {
        OpenDialog({chooseFile, ".*,.cpp,.h,.hpp", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with collections of filters")) {
        const string filters = "All files{.*},Source files (*.cpp *.h *.hpp){.cpp,.h,.hpp},Image files (*.png *.gif *.jpg *.jpeg){.png,.gif,.jpg,.jpeg},.md";
        OpenDialog({chooseFile, filters, ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open all file types with \".*\" filter")) {
        OpenDialog({chooseFile, ".*", ".", filePathName, false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open File Dialog with filter of type regex (Custom.+[.]h)")) {
        OpenDialog({chooseFile, "Regex Custom*.h{(Custom.+[.]h)}", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with selection of 5 items")) {
        OpenDialog({chooseFile, ".*,.cpp,.h,.hpp", ".", "", false, 5, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with infinite selection")) {
        OpenDialog({chooseFile, ".*,.cpp,.h,.hpp", ".", "", false, 0, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with last file path name")) {
        OpenDialog({chooseFile, ".*,.cpp,.h,.hpp", ".", filePathName, false, 1, flags});
    }

    if (Button(ICON_IGFD_SAVE " Save file dialog with confirm-overwrite dialog if file exists")) {
        const string filters = "C/C++ file (*.c *.cpp){.c,.cpp}, Header file (*.h){.h}";
        OpenDialog({chooseFileSave, filters, ".", filePathName, true, 1, ImGuiFileDialogFlags_ConfirmOverwrite});
    }

    // Keeping this around to remind myself that custom panes & UserDatas are a thing.
    // If `cantContinue` is false, the user can't validate the dialog.
    // static bool canValidateDialog = false;
    // inline void InfosPane(const char *filter, IGFDUserDatas userData, bool *cantContinue) {
    //     TextColored(ImVec4(0, 1, 1, 1), "Infos Pane");
    //     Text("Selected Filter: %s", filter);
    //     if (userData) Text("User Data: %s", (const char *) userData);
    //     Checkbox("If not checked, you can't validate the dialog", &canValidateDialog);
    //     if (cantContinue) *cantContinue = canValidateDialog;
    // }
    //
    // auto saveFileUserData = IGFDUserDatas("SaveFile");
    // if (Button(ICON_IGFD_SAVE " Save file dialog with a custom pane")) {
    //     const char *filters = "C++ File (*.cpp){.cpp}";
    //     dialog->OpenDialog(chooseFileDialogKey, chooseFileSave, filters,
    //         ".", "", [](auto &&PH1, auto &&PH2, auto &&PH3) { return InfosPane(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); }, 350, 1,
    //         saveFileUserData, flags);
    // }

    filePathName = dialog->GetFilePathName();
    static string filePath = dialog->GetCurrentPath();
    static string userData = dialog->GetUserDatas() ? string((const char *) dialog->GetUserDatas()) : "";

    // Convert from map to vector of pairs. TODO use `ranges::view` piped transform
    const auto &selections = dialog->GetSelection();
    static vector<std::pair<string, string>> selection = {};
    selection.clear();
    for (const auto &sel: selections) selection.emplace_back(sel.first, sel.second);

    Separator();

    TextUnformatted("FileDialog state:\n");
    Indent();
    {
        TextUnformatted(format("FilePathName: {}", filePathName).c_str());
        TextUnformatted(format("FilePath: {}", filePath).c_str());
        TextUnformatted(format("Filters: {}", string(s.FileDialog.Filters)).c_str());
        TextUnformatted(format("UserDatas: {}", userData).c_str());
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

    // Save bookmarks dialog 1
    std::ofstream configFileWriter_1("bookmarks_1.conf", std::ios::out);
    if (!configFileWriter_1.bad()) {
        configFileWriter_1 << dialog->SerializeBookmarks();
        configFileWriter_1.close();
    }
#endif
}
