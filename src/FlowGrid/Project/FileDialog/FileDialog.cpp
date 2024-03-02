#include "FileDialog.h"

#include <range/v3/range/conversion.hpp>

#include "ImGuiFileDialog.h"
#include "imgui.h"

#include "FileDialogDataJson.h"
#include "FileDialogImpl.h"
#include "Helper/File.h"
#include "UI/HelpMarker.h"
#include "UI/Styling.h"

void FileDialog::Apply(const ActionType &action) const {
    // `SelectedFilePath` mutations are non-stateful side effects.
    std::visit(
        Match{
            [this](const Action::FileDialog::Open &a) { Set(json::parse(a.dialog_json)); },
            [](const Action::FileDialog::Select &a) { SelectedFilePath = a.file_path; },
        },
        action
    );
}

bool FileDialog::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](const Action::FileDialog::Open &) { return !Visible; },
            [](const Action::FileDialog::Select &) { return true; }, // File dialog `Visible` is set to false _before_ the select action is issued.
        },
        action
    );
}

void FileDialog::Set(const FileDialogData &data) const {
    OwnerId = data.owner_id;
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

void FileDialog::Demo::OpenDialog(const FileDialogData &data) const {
    FileDialog.Q(Action::FileDialog::Open{json(data).dump()});
}

using namespace ImGui;

// Same as `ImGui::CheckboxFlags`, but with `help` arg.
bool CheckboxFlags(const char *label, int *flags, int flags_value, const char *help) {
    const bool result = ImGui::CheckboxFlags(label, flags, flags_value);
    SameLine();
    fg::HelpMarker(help);
    return result;
}

void FileDialog::Render() const {
    if (!Visible) return FileDialogImp.Dialog->Close();

    static const string DialogKey = "FileDialog";
    // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.

    ImGuiFileDialogFlags flags = Flags;
    if (SaveMode) flags |= ImGuiFileDialogFlags_ConfirmOverwrite;
    else flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;

    IGFD::FileDialogConfig config;
    config.path = FilePath;
    config.countSelectionMax = MaxNumSelections;
    config.flags = flags;
    config.filePathName = DefaultFileName;
    FileDialogImp.Dialog->OpenDialog(DialogKey, Title, Filters.c_str(), config);
    if (FileDialogImp.Dialog->Display(DialogKey, ImGuiWindowFlags_NoCollapse, GetMainViewport()->Size / 2)) {
        Visible = false;
        if (FileDialogImp.Dialog->IsOk()) Q(Action::FileDialog::Select{FileDialogImp.Dialog->GetFilePathName()});
    }
}

FileDialog::Demo::Demo(ComponentArgs &&args, const ::FileDialog &dialog) : Component(std::move(args)), FileDialog(dialog) {}

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
        OpenDialog({Id, ChooseFileOpen, ".*,.cpp,.h,.hpp", ".", "", false, 1, flags});
    }
    if (Button(ICON_IGFD_FOLDER_OPEN " Open file dialog with collections of filters")) {
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

    FilePathName = FileDialogImp.Dialog->GetFilePathName();
    static string file_path = FileDialogImp.Dialog->GetCurrentPath();
    static string user_data = FileDialogImp.Dialog->GetUserDatas() ? string((const char *)FileDialogImp.Dialog->GetUserDatas()) : "";

    Separator();

    TextUnformatted("State:\n");
    Indent();
    {
        TextUnformatted(std::format("FilePathName: {}", FilePathName).c_str());
        TextUnformatted(std::format("FilePath: {}", file_path).c_str());
        TextUnformatted(std::format("Filters: {}", FileDialogImp.Dialog->GetCurrentFilter()).c_str());
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
                const auto &selection = FileDialogImp.Dialog->GetSelection();
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
