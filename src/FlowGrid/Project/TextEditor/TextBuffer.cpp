#include "TextBuffer.h"

#include "Core/Windows.h"
#include "TextBufferImpl.h"
#include "UI/Fonts.h"

using std::string, std::string_view;

TextBuffer::TextBuffer(ComponentArgs &&args, const fs::path &file_path)
    : Component(std::move(args)), _LastOpenedFilePath(file_path), Impl(std::make_unique<TextBufferImpl>(file_path)) {
}

TextBuffer::~TextBuffer() {}

string TextBuffer::GetText() const { return Impl->GetText(); }
bool TextBuffer::Empty() const { return Impl->Empty(); }

const string &TextBuffer::GetLanguageFileExtensionsFilter() const { return Impl->GetLanguageFileExtensionsFilter(); }

using namespace ImGui;

void TextBuffer::SetText(const std::string &text) const { Impl->SetText(text); }
void TextBuffer::OpenFile(const fs::path &path) const {
    LastOpenedFilePath.Set(path);
    Impl->OpenFile(path);
}

void TextBuffer::Render() const {
    const auto cursor_coords = Impl->GetCursorPosition();
    const string editing_file = LastOpenedFilePath ? string(fs::path(LastOpenedFilePath).filename()) : "No file";
    ImGui::Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursor_coords.L + 1, cursor_coords.C + 1, Impl->LineCount(),
        Impl->Overwrite ? "Ovr" : "Ins",
        Impl->CanUndo() ? "*" : " ",
        Impl->GetLanguageName().c_str(),
        editing_file.c_str()
    );

    const bool is_parent_focused = IsWindowFocused();
    PushFont(gFonts.FixedWidth);
    PushStyleColor(ImGuiCol_ChildBg, Impl->GetColor(PaletteIndex::Background));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    BeginChild("TextBuffer", {}, false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);
    Impl->Render(is_parent_focused);
    EndChild();
    PopStyleVar();
    PopStyleColor();
    PopFont();
}

void TextBuffer::RenderMenu() const {
    if (BeginMenu("Edit")) {
        MenuItem("Read-only mode", nullptr, &Impl->ReadOnly);
        Separator();
        if (MenuItem("Undo", "cmd+z", nullptr, Impl->CanUndo())) Impl->Undo();
        if (MenuItem("Redo", "shift+cmd+z", nullptr, Impl->CanRedo())) Impl->Redo();
        Separator();
        if (MenuItem("Copy", "cmd+c", nullptr, Impl->CanCopy())) Impl->Copy();
        if (MenuItem("Cut", "cmd+x", nullptr, Impl->CanCut())) Impl->Cut();
        if (MenuItem("Paste", "cmd+v", nullptr, Impl->CanPaste())) Impl->Paste();
        Separator();
        if (MenuItem("Select all", nullptr, nullptr)) Impl->SelectAll();
        EndMenu();
    }

    if (BeginMenu("View")) {
        if (BeginMenu("Palette")) {
            if (MenuItem("Mariana palette")) Impl->SetPalette(TextBufferPaletteIdT::Mariana);
            if (MenuItem("Dark palette")) Impl->SetPalette(TextBufferPaletteIdT::Dark);
            if (MenuItem("Light palette")) Impl->SetPalette(TextBufferPaletteIdT::Light);
            if (MenuItem("Retro blue palette")) Impl->SetPalette(TextBufferPaletteIdT::RetroBlue);
            EndMenu();
        }
        MenuItem("Show style transition points", nullptr, &Impl->ShowStyleTransitionPoints);
        MenuItem("Show changed capture ranges", nullptr, &Impl->ShowChangedCaptureRanges);
        gWindows.ToggleDebugMenuItem(Debug);
        EndMenu();
    }
}

void TextBuffer::RenderDebug() const { Impl->DebugPanel(); }
