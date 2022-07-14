#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using std::string;

using MessagePackBytes = std::vector<std::uint8_t>;

struct File {
    using ImGuiFileDialogFlags = int; // Declared in `lib/ImGuiFileDialog/ImGuiFileDialog.h`
    // TODO window?
    struct Dialog {
        Dialog() = default;
        Dialog(string title, string filters, string path, string default_file_name = "", const bool save_mode = false, const int &max_num_selections = 1, ImGuiFileDialogFlags flags = 0)
            : visible(true), save_mode(save_mode), max_num_selections(max_num_selections), flags(flags),
              title(std::move(title)), filters(std::move(filters)), path(std::move(path)), default_file_name(std::move(default_file_name)) {};

        void draw() const;

        bool visible = false;
        bool save_mode = false; // The same file dialog instance is used for both saving & opening files.
        int max_num_selections = 1;
        ImGuiFileDialogFlags flags = 0;
        string title = "Choose file";
        string filters;
        string path = ".";
        string default_file_name;
    };

    static string read(const fs::path &path);
    static bool write(const fs::path &path, const string &contents);
    static bool write(const fs::path &path, const MessagePackBytes &contents);

    Dialog dialog;
};
