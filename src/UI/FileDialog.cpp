#include "../Context.h"
#include "../File.h"
#include "ImGuiFileDialog.h"

static auto *file_dialog = ImGuiFileDialog::Instance();
static const string file_dialog_key = "FileDialog";

void State::File::Dialog::draw() const {
    if (visible) {
        // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.
        file_dialog->OpenDialog(file_dialog_key, title, filters.c_str(), path, default_file_name, max_num_selections, nullptr, flags);

        // TODO need to get custom vecs with math going
        const ImVec2 min_dialog_size = {ImGui::GetMainViewport()->Size.x / 2.0f, ImGui::GetMainViewport()->Size.y / 2.0f};
        if (file_dialog->Display(file_dialog_key, ImGuiWindowFlags_NoCollapse, min_dialog_size)) {
            if (file_dialog->IsOk()) {
                const fs::path &file_path = file_dialog->GetFilePathName();
                const string &extension = file_path.extension();
                if (AllProjectExtensions.find(extension) != AllProjectExtensions.end()) {
                    // TODO provide an option to save with undo state.
                    //   This file format would be a json list of diffs.
                    //   The file would generally be larger, and the load time would be slower,
                    //   but it would provide the option to save/load _exactly_ as if you'd never quit at all,
                    //   with full undo/redo history/position/etc.!
                    if (save_mode) q(save_project{file_path});
                    else q(open_project{file_path});
                } else if (extension == FaustDspFileExtension) {
                    if (save_mode) q(save_faust_dsp_file{file_path});
                    else q(open_faust_dsp_file{file_path});
                }
            }
            q(close_file_dialog{});
        }
    } else {
        file_dialog->Close();
    }
}
