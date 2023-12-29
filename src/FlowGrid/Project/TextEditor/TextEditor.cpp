#include "TextEditor.h"

#include "LanguageDefinition.h"

#include <algorithm>
#include <set>
#include <string>

#include "imgui_internal.h"

using std::string;

TextEditor::TextEditor() {
    SetPalette(DefaultPaletteId);
    Lines.push_back({});
}

TextEditor::~TextEditor() {}

void TextEditor::SetPalette(PaletteIdT palette_id) {
    PaletteId = palette_id;
    const PaletteT *palette_base;
    switch (PaletteId) {
        case PaletteIdT::Dark:
            palette_base = &DarkPalette;
            break;
        case PaletteIdT::Light:
            palette_base = &LightPalette;
            break;
        case PaletteIdT::Mariana:
            palette_base = &MarianaPalette;
            break;
        case PaletteIdT::RetroBlue:
            palette_base = &RetroBluePalette;
            break;
    }

    for (int i = 0; i < int(PaletteIndex::Max); ++i) {
        const ImVec4 color = U32ColorToVec4((*palette_base)[i]);
        // color.w *= ImGui::GetStyle().Alpha; todo bring this back.
        Palette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }
}

void TextEditor::SetLanguageDefinition(LanguageDefinitionIdT language_def_id) {
    LanguageDefinitionId = language_def_id;
    switch (LanguageDefinitionId) {
        case LanguageDefinitionIdT::Cpp:
            LanguageDef = &LanguageDefinition::Cpp;
            break;
        case LanguageDefinitionIdT::C:
            LanguageDef = &LanguageDefinition::C;
            break;
        case LanguageDefinitionIdT::Cs:
            LanguageDef = &LanguageDefinition::Cs;
            break;
        case LanguageDefinitionIdT::Python:
            LanguageDef = &LanguageDefinition::Python;
            break;
        case LanguageDefinitionIdT::Lua:
            LanguageDef = &LanguageDefinition::Lua;
            break;
        case LanguageDefinitionIdT::Json:
            LanguageDef = &LanguageDefinition::Jsn;
            break;
        case LanguageDefinitionIdT::Sql:
            LanguageDef = &LanguageDefinition::Sql;
            break;
        case LanguageDefinitionIdT::AngelScript:
            LanguageDef = &LanguageDefinition::AngelScript;
            break;
        case LanguageDefinitionIdT::Glsl:
            LanguageDef = &LanguageDefinition::Glsl;
            break;
        case LanguageDefinitionIdT::Hlsl:
            LanguageDef = &LanguageDefinition::Hlsl;
            break;
        case LanguageDefinitionIdT::None:
            LanguageDef = nullptr;
            break;
    }

    RegexList.clear();
    for (const auto &r : LanguageDef->TokenRegexStrings) {
        RegexList.push_back({std::regex(r.first, std::regex_constants::optimize), r.second});
    }

    Colorize();
}

const char *TextEditor::GetLanguageDefinitionName() const { return LanguageDef != nullptr ? LanguageDef->Name.c_str() : "None"; }

void TextEditor::SetTabSize(int tab_size) {
    TabSize = std::clamp(tab_size, 1, 8);
}
void TextEditor::SetLineSpacing(float line_spacing) {
    LineSpacing = std::clamp(line_spacing, 1.f, 2.f);
}

void TextEditor::SelectAll() {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        cursor.InteractiveEnd = cursor.InteractiveStart = cursor.GetSelectionEnd();
    }
    State.CurrentCursor = 0;
    MoveTop();
    MoveBottom(true);
}

bool TextEditor::AnyCursorHasSelection() const {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        if (State.Cursors[c].HasSelection()) return true;
    }
    return false;
}

bool TextEditor::AnyCursorHasMultilineSelection() const {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        if (State.Cursors[c].HasMultilineSelection()) return true;
    }
    return false;
}

bool TextEditor::AllCursorsHaveSelection() const {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        if (!State.Cursors[c].HasSelection()) return false;
    }
    return true;
}

TextEditor::Coordinates TextEditor::GetCursorPosition() const {
    return SanitizeCoordinates(State.GetCursor().InteractiveEnd);
}

void TextEditor::Copy() {
    if (AnyCursorHasSelection()) {
        string str;
        for (uint c = 0; c <= State.CurrentCursor; c++) {
            const auto &cursor = State.Cursors[c];
            if (cursor.HasSelection()) {
                if (!str.empty()) str += '\n';
                str += GetSelectedText(cursor);
            }
        }
        ImGui::SetClipboardText(str.c_str());
    } else if (!Lines.empty()) {
        string str;
        for (const auto &g : Lines[GetCursorPosition().L]) str.push_back(g.Char);
        ImGui::SetClipboardText(str.c_str());
    }
}

void TextEditor::Cut() {
    if (AnyCursorHasSelection()) {
        UndoRecord u{State};
        Copy();
        for (int c = State.CurrentCursor; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            AddSelectionUndoOp(u, UndoOperationType::Delete, cursor);
            DeleteSelection(cursor);
        }
        AddUndo(u);
    }
}

void TextEditor::Paste() {
    // Check if we should do multicursor paste.
    const string clip_text = ImGui::GetClipboardText();
    bool can_paste_to_multiple_cursors = false;
    std::vector<std::pair<int, int>> clip_text_lines;
    if (State.CurrentCursor > 0) {
        clip_text_lines.push_back({0, 0});
        for (uint i = 0; i < clip_text.length(); i++) {
            if (clip_text[i] == '\n') {
                clip_text_lines.back().second = i;
                clip_text_lines.push_back({i + 1, 0});
            }
        }
        clip_text_lines.back().second = clip_text.length();
        can_paste_to_multiple_cursors = clip_text_lines.size() == State.CurrentCursor + 1;
    }

    if (clip_text.length() > 0) {
        UndoRecord u{State};
        if (AnyCursorHasSelection()) {
            for (int c = State.CurrentCursor; c > -1; c--) {
                auto &cursor = State.Cursors[c];
                AddSelectionUndoOp(u, UndoOperationType::Delete, cursor);
                DeleteSelection(cursor);
            }
        }

        for (int c = State.CurrentCursor; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            const auto end = SanitizeCoordinates(cursor.InteractiveEnd);
            const string insert_text = can_paste_to_multiple_cursors ? clip_text.substr(clip_text_lines[c].first, clip_text_lines[c].second - clip_text_lines[c].first) : clip_text;
            InsertTextAtCursor(insert_text, cursor);
            u.Operations.emplace_back(insert_text, end, SanitizeCoordinates(cursor.InteractiveEnd), UndoOperationType::Add);
        }

        AddUndo(u);
    }
}

void TextEditor::Undo(int steps) {
    while (CanUndo() && steps-- > 0) UndoBuffer[--UndoIndex].Undo(this);
}
void TextEditor::Redo(int steps) {
    while (CanRedo() && steps-- > 0) UndoBuffer[UndoIndex++].Redo(this);
}

void TextEditor::SetText(const string &text) {
    Lines.clear();
    Lines.push_back({});
    for (auto chr : text) {
        if (chr == '\r') continue; // Ignore the carriage return character.
        if (chr == '\n') Lines.push_back({});
        else Lines.back().emplace_back(chr, PaletteIndex::Default);
    }

    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::AddUndoOp(UndoRecord &record, UndoOperationType type, const Coordinates &start, const Coordinates &end) {
    record.Operations.emplace_back(GetText(start, end), start, end, type);
}
void TextEditor::AddSelectionUndoOp(UndoRecord &record, UndoOperationType type, const Cursor &cursor) {
    if (cursor.HasSelection()) AddUndoOp(record, type, cursor.GetSelectionStart(), cursor.GetSelectionEnd());
}

string TextEditor::GetText(const Coordinates &start, const Coordinates &end) const {
    assert(end >= start);
    if (end == start || start.L == -1 || end.L == -1) return "";

    uint line_start = start.L, line_end = end.L;
    auto start_ci = GetCharIndexR(start);
    const auto end_ci = GetCharIndexR(end);

    uint s = 0;
    for (uint i = line_start; i < line_end; i++) s += Lines[i].size();

    string result;
    result.reserve(s + s / 8);
    while (start_ci < end_ci || line_start < line_end) {
        if (line_start >= Lines.size()) break;

        const auto &line = Lines[line_start];
        if (start_ci < int(line.size())) {
            result += line[start_ci].Char;
            start_ci++;
        } else {
            start_ci = 0;
            ++line_start;
            result += '\n';
        }
    }

    return result;
}

bool TextEditor::Render(const char *title, bool is_parent_focused, const ImVec2 &size, bool border) {
    if (CursorPositionChanged) OnCursorPositionChanged();
    CursorPositionChanged = false;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(Palette[int(PaletteIndex::Background)]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::BeginChild(title, size, border, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

    const bool is_focused = ImGui::IsWindowFocused();
    HandleKeyboardInputs(is_parent_focused);
    HandleMouseInputs();
    ColorizeInternal();
    Render(is_parent_focused);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    return is_focused;
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static uint UTF8CharLength(char ch) {
    if ((ch & 0xFE) == 0xFC) return 6;
    if ((ch & 0xFC) == 0xF8) return 5;
    if ((ch & 0xF8) == 0xF0) return 4;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xE0) == 0xC0) return 2;
    return 1;
}

static bool IsWordChar(char ch) {
    return UTF8CharLength(ch) > 1 ||
        (ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '_';
}

void TextEditor::EditorState::AddCursor() {
    // Vector is never resized to smaller size.
    // `CurrentCursor` points to last available cursor in vector.
    CurrentCursor++;
    Cursors.resize(CurrentCursor + 1);
    LastAddedCursor = CurrentCursor;
}

int TextEditor::EditorState::GetLastAddedCursorIndex() { return LastAddedCursor > CurrentCursor ? 0 : LastAddedCursor; }
TextEditor::Cursor &TextEditor::EditorState::GetLastAddedCursor() { return Cursors[GetLastAddedCursorIndex()]; }
TextEditor::Cursor &TextEditor::EditorState::GetCursor(int c) { return Cursors[c == -1 ? CurrentCursor : c]; }
TextEditor::Cursor const &TextEditor::EditorState::GetCursor(int c) const { return Cursors[c == -1 ? CurrentCursor : c]; }

void TextEditor::EditorState::SortCursors() {
    const auto last_added_cursor_pos = GetLastAddedCursor().InteractiveEnd;
    std::sort(Cursors.begin(), Cursors.begin() + CurrentCursor + 1, [](const auto &a, const auto &b) -> bool {
        return a.GetSelectionStart() < b.GetSelectionStart();
    });
    // Update last added cursor index to be valid after sort.
    for (uint c = 0; c <= CurrentCursor; c++) {
        if (Cursors[c].InteractiveEnd == last_added_cursor_pos) LastAddedCursor = c;
    }
}

void TextEditor::UndoRecord::Undo(TextEditor *editor) {
    for (int i = Operations.size() - 1; i > -1; i--) {
        const auto &op = Operations[i];
        if (!op.Text.empty()) {
            switch (op.Type) {
                case UndoOperationType::Delete: {
                    auto start = op.Start;
                    editor->InsertTextAt(start, op.Text.c_str());
                    editor->Colorize(op.Start.L - 1, op.End.L - op.Start.L + 2);
                    break;
                }
                case UndoOperationType::Add: {
                    editor->DeleteRange(op.Start, op.End);
                    editor->Colorize(op.Start.L - 1, op.End.L - op.Start.L + 2);
                    break;
                }
            }
        }
    }

    editor->State = Before;
    editor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *editor) {
    for (const auto &op : Operations) {
        if (!op.Text.empty()) {
            switch (op.Type) {
                case UndoOperationType::Delete: {
                    editor->DeleteRange(op.Start, op.End);
                    editor->Colorize(op.Start.L - 1, op.End.L - op.Start.L + 1);
                    break;
                }
                case UndoOperationType::Add: {
                    auto start = op.Start;
                    editor->InsertTextAt(start, op.Text.c_str());
                    editor->Colorize(op.Start.L - 1, op.End.L - op.Start.L + 1);
                    break;
                }
            }
        }
    }

    editor->State = After;
    editor->EnsureCursorVisible();
}

string TextEditor::GetText() const {
    const auto last_line = int(Lines.size()) - 1;
    return GetText({}, {last_line, GetLineMaxColumn(last_line)});
}

string TextEditor::GetSelectedText(const Cursor &cursor) const {
    return GetText(cursor.GetSelectionStart(), cursor.GetSelectionEnd());
}

void TextEditor::SetCursorPosition(const Coordinates &position, Cursor &cursor, bool clear_selection) {
    CursorPositionChanged = true;

    if (clear_selection) cursor.InteractiveStart = position;

    if (cursor.InteractiveEnd != position) {
        cursor.InteractiveEnd = position;
        EnsureCursorVisible();
    }
}

void TextEditor::InsertTextAtCursor(const string &text, Cursor &cursor) {
    if (text.empty()) return;

    auto pos = SanitizeCoordinates(cursor.InteractiveEnd);
    const auto start = std::min(pos, cursor.GetSelectionStart());
    const uint total_lines = pos.L - start.L + InsertTextAt(pos, text.c_str());
    SetCursorPosition(pos, cursor);
    Colorize(start.L - 1, total_lines + 2);
}

bool TextEditor::Move(int &line, int &ci, bool left, bool lock_line) const {
    // assumes given char index is not in the middle of utf8 sequence
    // char index can be line.length()

    // invalid line
    if (line >= int(Lines.size())) return false;

    if (left) {
        if (ci == 0) {
            if (lock_line || line == 0) return false;
            line--;
            ci = Lines[line].size();
        } else {
            ci--;
            while (ci > 0 && IsUTFSequence(Lines[line][ci].Char)) ci--;
        }
    } else { // right
        if (ci == int(Lines[line].size())) {
            if (lock_line || line == int(Lines.size()) - 1) return false;
            line++;
            ci = 0;
        } else {
            const int seq_length = UTF8CharLength(Lines[line][ci].Char);
            ci = std::min(ci + seq_length, int(Lines[line].size()));
        }
    }
    return true;
}

void TextEditor::MoveCharIndexAndColumn(int line, int &ci, int &column) const {
    assert(line < int(Lines.size()));
    assert(ci < int(Lines[line].size()));
    const char ch = Lines[line][ci].Char;
    ci += UTF8CharLength(ch);
    if (ch == '\t') column = (column / TabSize) * TabSize + TabSize;
    else column++;
}

void TextEditor::MoveCoords(Coordinates &coords, MoveDirection direction, bool is_word_mode, int line_count) const {
    int ci = GetCharIndexR(coords);
    int lindex = coords.L;
    switch (direction) {
        case MoveDirection::Right:
            if (ci >= int(Lines[lindex].size())) {
                if (lindex < int(Lines.size()) - 1) {
                    coords.L = std::clamp(lindex + 1, 0, int(Lines.size()) - 1);
                    coords.C = 0;
                }
            } else {
                Move(lindex, ci);
                int one_step_right_column = GetCharColumn(lindex, ci);
                if (is_word_mode) {
                    coords = FindWordEnd(coords);
                    coords.C = std::max(coords.C, one_step_right_column);
                } else {
                    coords.C = one_step_right_column;
                }
            }
            break;
        case MoveDirection::Left:
            if (ci == 0) {
                if (lindex > 0) {
                    coords.L = lindex - 1;
                    coords.C = GetLineMaxColumn(coords.L);
                }
            } else {
                Move(lindex, ci, true);
                coords.C = GetCharColumn(lindex, ci);
                if (is_word_mode) coords = FindWordStart(coords);
            }
            break;
        case MoveDirection::Up:
            coords.L = std::max(0, lindex - line_count);
            break;
        case MoveDirection::Down:
            coords.L = std::clamp(lindex + line_count, 0, int(Lines.size()) - 1);
            break;
    }
}

void TextEditor::MoveUp(int amount, bool select) {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        auto new_coords = cursor.InteractiveEnd;
        MoveCoords(new_coords, MoveDirection::Up, false, amount);
        SetCursorPosition(new_coords, cursor, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveDown(int amount, bool select) {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        assert(cursor.InteractiveEnd.C >= 0);
        auto new_coords = cursor.InteractiveEnd;
        MoveCoords(new_coords, MoveDirection::Down, false, amount);
        SetCursorPosition(new_coords, cursor, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveLeft(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        if (AnyCursorHasSelection() && !select && !is_word_mode) {
            SetCursorPosition(cursor.GetSelectionStart(), cursor);
        } else {
            auto new_coords = cursor.InteractiveEnd;
            MoveCoords(new_coords, MoveDirection::Left, is_word_mode);
            SetCursorPosition(new_coords, cursor, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        if (AnyCursorHasSelection() && !select && !is_word_mode) {
            SetCursorPosition(cursor.GetSelectionEnd(), cursor);
        } else {
            auto new_coords = cursor.InteractiveEnd;
            MoveCoords(new_coords, MoveDirection::Right, is_word_mode);
            SetCursorPosition(new_coords, cursor, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool select) {
    SetCursorPosition({0, 0}, State.GetCursor(), !select);
}

void TextEditor::TextEditor::MoveBottom(bool select) {
    const int max_line = int(Lines.size()) - 1;
    SetCursorPosition({max_line, GetLineMaxColumn(max_line)}, State.GetCursor(), !select);
}

void TextEditor::MoveHome(bool select) {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        SetCursorPosition({cursor.InteractiveEnd.L, 0}, cursor, !select);
    }
}

void TextEditor::MoveEnd(bool select) {
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        const int lindex = cursor.InteractiveEnd.L;
        SetCursorPosition({lindex, GetLineMaxColumn(lindex)}, cursor, !select);
    }
}

// todo can we simplify this by using `InsertTextAt...`?
void TextEditor::EnterChar(ImWchar ch, bool is_shift) {
    const bool has_selection = AnyCursorHasSelection();
    if (ch == '\t' && has_selection && AnyCursorHasMultilineSelection()) return ChangeCurrentLinesIndentation(!is_shift);

    UndoRecord u{State};
    if (has_selection) {
        for (int c = State.CurrentCursor; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            AddSelectionUndoOp(u, UndoOperationType::Delete, cursor);
            DeleteSelection(cursor);
        }
    }

    std::vector<Coordinates> coords;
    coords.reserve(State.CurrentCursor + 1);
    // Order is important here for typing '\n' in the same line at the same time.
    for (int c = State.CurrentCursor; c > -1; c--) {
        auto &cursor = State.Cursors[c];
        const auto coord = SanitizeCoordinates(cursor.InteractiveEnd);
        coords.push_back(coord);
        UndoOperation added;
        added.Type = UndoOperationType::Add;
        added.Start = coord;

        assert(!Lines.empty());

        if (ch == '\n') {
            InsertLine(coord.L + 1);
            const auto &line = Lines[coord.L];
            auto &new_line = Lines[coord.L + 1];

            added.Text = char(ch);
            if (AutoIndent) {
                for (uint i = 0; i < line.size() && isascii(line[i].Char) && isblank(line[i].Char); i++) {
                    new_line.push_back(line[i]);
                    added.Text += line[i].Char;
                }
            }

            const size_t whitespace_size = new_line.size();
            const auto ci = GetCharIndexR(coord);
            AddGlyphsToLine(coord.L + 1, new_line.size(), {line.cbegin() + ci, line.cend()});
            RemoveGlyphsFromLine(coord.L, ci, -1);
            SetCursorPosition({coord.L + 1, GetCharColumn(coord.L + 1, int(whitespace_size))}, cursor);
        } else {
            char buf[5];
            ImTextCharToUtf8(buf, ch);

            const auto &line = Lines[coord.L];
            const auto ci = GetCharIndexR(coord);
            if (Overwrite && ci < int(line.size())) {
                uint d = UTF8CharLength(line[ci].Char);

                UndoOperation removed;
                removed.Type = UndoOperationType::Delete;
                removed.Start = cursor.InteractiveEnd;
                removed.End = {coord.L, GetCharColumn(coord.L, ci + d)};
                while (d-- > 0 && ci < int(line.size())) {
                    removed.Text += line[ci].Char;
                    RemoveGlyphsFromLine(coord.L, ci, ci + 1);
                }
                u.Operations.push_back(removed);
            }
            std::vector<Glyph> glyphs;
            for (auto p = buf; *p != '\0'; p++) glyphs.emplace_back(*p, PaletteIndex::Default);
            AddGlyphsToLine(coord.L, ci, glyphs);
            added.Text = buf;

            SetCursorPosition({coord.L, GetCharColumn(coord.L, ci + glyphs.size())}, cursor);
        }

        added.End = SanitizeCoordinates(cursor.InteractiveEnd);
        u.Operations.push_back(added);
    }

    AddUndo(u);

    for (const auto &coord : coords) Colorize(coord.L - 1, 3);
    EnsureCursorVisible();
}

void TextEditor::Backspace(bool is_word_mode) {
    if (Lines.empty()) return;

    if (AnyCursorHasSelection()) {
        Delete(is_word_mode);
    } else {
        auto before_state = State;
        MoveLeft(true, is_word_mode);
        // Can't do backspace if any cursor is at {0,0}.
        if (!AllCursorsHaveSelection()) {
            if (AnyCursorHasSelection()) MoveRight();
            return;
        }
        OnCursorPositionChanged(); // Might combine cursors.
        Delete(is_word_mode, &before_state);
    }
}

void TextEditor::Delete(bool is_word_mode, const EditorState *editor_state) {
    if (Lines.empty()) return;

    if (AnyCursorHasSelection()) {
        UndoRecord u{editor_state == nullptr ? State : *editor_state};
        for (int c = State.CurrentCursor; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            AddSelectionUndoOp(u, UndoOperationType::Delete, cursor);
            DeleteSelection(cursor);
        }
        AddUndo(u);
    } else {
        auto before_state = State;
        MoveRight(true, is_word_mode);
        // Can't do delete if any cursor is at the end of the last line.
        if (!AllCursorsHaveSelection()) {
            if (AnyCursorHasSelection()) MoveLeft();
            return;
        }

        OnCursorPositionChanged(); // Might combine cursors.
        Delete(is_word_mode, &before_state);
    }
}

void TextEditor::SetSelection(Coordinates start, Coordinates end, Cursor &cursor) {
    const int max_line = int(Lines.size()) - 1;
    Coordinates min_coords{0, 0}, max_coords{max_line, GetLineMaxColumn(max_line)};
    if (start < min_coords) start = min_coords;
    else if (start > max_coords) start = max_coords;
    if (end < min_coords) end = min_coords;
    else if (end > max_coords) end = max_coords;

    cursor.InteractiveStart = start;
    SetCursorPosition(end, cursor, false);
}

void TextEditor::AddCursorForNextOccurrence(bool case_sensitive) {
    const auto &cursor = State.GetLastAddedCursor();
    const string selection = GetSelectedText(cursor);
    if (selection.empty()) return;

    Coordinates next_start, next_end;
    if (!FindNextOccurrence(selection.c_str(), selection.length(), cursor.GetSelectionEnd(), next_start, next_end, case_sensitive)) return;

    State.AddCursor();
    SetSelection(next_start, next_end, State.GetCursor());
    SortAndMergeCursors();
    EnsureCursorVisible(true);
}

bool TextEditor::FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out, bool case_sensitive) {
    assert(text_size > 0);
    int f_li, if_li;
    if_li = f_li = from.L;

    int f_i, if_i;
    if_i = f_i = GetCharIndexR(from);

    while (true) {
        bool matches;
        {
            // Match function.
            int line_offset = 0;
            uint ci_inner = f_i;
            int i = 0;
            for (; i < text_size; i++) {
                if (ci_inner == Lines[f_li + line_offset].size()) {
                    if (text[i] != '\n' || f_li + line_offset + 1 >= int(Lines.size())) break;

                    ci_inner = 0;
                    line_offset++;
                } else {
                    char to_compare_a = Lines[f_li + line_offset][ci_inner].Char;
                    char to_compare_b = text[i];
                    to_compare_a = (!case_sensitive && to_compare_a >= 'A' && to_compare_a <= 'Z') ? to_compare_a - 'A' + 'a' : to_compare_a;
                    to_compare_b = (!case_sensitive && to_compare_b >= 'A' && to_compare_b <= 'Z') ? to_compare_b - 'A' + 'a' : to_compare_b;
                    if (to_compare_a != to_compare_b) break;

                    ci_inner++;
                }
            }
            matches = i == text_size;
            if (matches) {
                start_out = {f_li, GetCharColumn(f_li, f_i)};
                end_out = {f_li + line_offset, GetCharColumn(f_li + line_offset, ci_inner)};
                return true;
            }
        }

        // Move forward.
        if (f_i == int(Lines[f_li].size())) { // Need to consider line breaks.
            f_li = f_li == int(Lines.size()) - 1 ? 0 : f_li + 1;
            f_i = 0;
        } else {
            f_i++;
        }

        // Detect complete scan.
        if (f_i == if_i && f_li == if_li) return false;
    }

    return false;
}

bool TextEditor::FindMatchingBracket(int li, int ci, Coordinates &out) {
    static const std::unordered_map<char, char>
        OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
        CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

    const auto &line = Lines[li];
    if (li > int(Lines.size()) - 1 || ci > int(line.size()) - 1) return false;

    int li_inner = li, ci_inner = ci;
    int counter = 1;
    if (CloseToOpenChar.contains(line[ci].Char)) {
        const char close_char = line[ci].Char, open_char = CloseToOpenChar.at(close_char);
        while (Move(li_inner, ci_inner, true)) {
            if (ci_inner < int(Lines[li_inner].size())) {
                const char ch = Lines[li_inner][ci_inner].Char;
                if (ch == open_char) {
                    counter--;
                    if (counter == 0) {
                        out = {li_inner, GetCharColumn(li_inner, ci_inner)};
                        return true;
                    }
                } else if (ch == close_char) {
                    counter++;
                }
            }
        }
    } else if (OpenToCloseChar.contains(line[ci].Char)) {
        const char open_char = line[ci].Char, close_char = OpenToCloseChar.at(open_char);
        while (Move(li_inner, ci_inner)) {
            if (ci_inner < int(Lines[li_inner].size())) {
                const char ch = Lines[li_inner][ci_inner].Char;
                if (ch == close_char) {
                    counter--;
                    if (counter == 0) {
                        out = {li_inner, GetCharColumn(li_inner, ci_inner)};
                        return true;
                    }
                } else if (ch == open_char) {
                    counter++;
                }
            }
        }
    }
    return false;
}

void TextEditor::ChangeCurrentLinesIndentation(bool increase) {
    UndoRecord u{State};
    for (int c = State.CurrentCursor; c > -1; c--) {
        const auto &cursor = State.Cursors[c];
        for (int li = cursor.GetSelectionEnd().L; li >= cursor.GetSelectionStart().L; li--) {
            // Check if selection ends at line start.
            if (cursor.HasSelection() && cursor.GetSelectionEnd() == Coordinates{li, 0}) continue;

            if (increase) {
                if (Lines[li].size() > 0) {
                    Coordinates line_start{li, 0}, insertion_end = line_start;
                    InsertTextAt(insertion_end, "\t"); // Sets insertion end.
                    u.Operations.emplace_back("\t", line_start, insertion_end, UndoOperationType::Add);
                    Colorize(line_start.L, 1);
                }
            } else {
                Coordinates start{li, 0}, end{li, TabSize};
                int ci = GetCharIndexL(end) - 1;
                while (ci > -1 && (Lines[li][ci].Char == ' ' || Lines[li][ci].Char == '\t')) ci--;
                bool only_space_chars_found = ci == -1;
                if (only_space_chars_found) {
                    u.Operations.emplace_back(GetText(start, end), start, end, UndoOperationType::Delete);
                    DeleteRange(start, end);
                    Colorize(li, 1);
                }
            }
        }
    }

    if (!u.Operations.empty()) AddUndo(u);
}

void TextEditor::MoveCurrentLines(bool up) {
    UndoRecord u{State};
    std::set<int> affected_lines;
    int min_li = -1, max_li = -1;
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        const auto &cursor = State.Cursors[c];
        for (int li = cursor.GetSelectionEnd().L; li >= cursor.GetSelectionStart().L; li--) {
            // Check if selection ends at line start.
            if (cursor.HasSelection() && cursor.GetSelectionEnd() == Coordinates{li, 0}) continue;

            affected_lines.insert(li);
            min_li = min_li == -1 ? li : (li < min_li ? li : min_li);
            max_li = max_li == -1 ? li : (li > max_li ? li : max_li);
        }
    }
    if ((up && min_li == 0) || (!up && max_li == int(Lines.size()) - 1)) return; // Can't move up/down anymore.

    const int start_li = min_li - (up ? 1 : 0), end_li = max_li + (up ? 0 : 1);
    const Coordinates start{start_li, 0}, end{end_li, GetLineMaxColumn(end_li)};
    AddUndoOp(u, UndoOperationType::Delete, start, end);
    if (up) {
        for (const int li : affected_lines) std::swap(Lines[li - 1], Lines[li]);
    } else {
        for (auto it = affected_lines.rbegin(); it != affected_lines.rend(); it++) std::swap(Lines[*it + 1], Lines[*it]);
    }
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        cursor.InteractiveStart.L += (up ? -1 : 1);
        cursor.InteractiveEnd.L += (up ? -1 : 1);
    }
    // No need to set CursorPositionChanged as cursors will remain sorted.
    AddUndoOp(u, UndoOperationType::Add, start, end);
    AddUndo(u);
}

void TextEditor::ToggleLineComment() {
    if (LanguageDef == nullptr) return;

    const string &comment_str = LanguageDef->SingleLineComment;

    UndoRecord u{State};
    bool should_add_comment = false;
    std::unordered_set<int> affected_line_indices;
    for (int c = State.CurrentCursor; c > -1; c--) {
        const auto &cursor = State.Cursors[c];
        for (int li = cursor.GetSelectionEnd().L; li >= cursor.GetSelectionStart().L; li--) {
            // Check if selection ends at line start.
            if (cursor.HasSelection() && cursor.GetSelectionEnd() == Coordinates{li, 0}) continue;

            affected_line_indices.insert(li);

            const auto &line = Lines[li];
            int i = 0;
            while (i < int(line.size()) && (line[i].Char == ' ' || line[i].Char == '\t')) i++;
            if (i == int(line.size())) continue;

            uint i_inner = 0;
            while (i_inner < comment_str.length() && i + i_inner < line.size() && line[i + i_inner].Char == comment_str[i_inner]) i++;
            bool matched = i_inner == comment_str.length();
            should_add_comment |= !matched;
        }
    }

    if (should_add_comment) {
        for (int li : affected_line_indices) {
            Coordinates line_start{li, 0}, insertion_end = line_start;
            InsertTextAt(insertion_end, (comment_str + ' ').c_str()); // sets insertion end
            u.Operations.emplace_back((comment_str + ' '), line_start, insertion_end, UndoOperationType::Add);
            Colorize(line_start.L, 1);
        }
    } else {
        for (int li : affected_line_indices) {
            const auto &line = Lines[li];
            uint ci = 0;
            while (ci < line.size() && (line[ci].Char == ' ' || line[ci].Char == '\t')) ci++;
            if (ci == line.size()) continue;

            uint ci_2 = 0;
            while (ci_2 < comment_str.length() && ci + ci_2 < line.size() && line[ci + ci_2].Char == comment_str[ci_2]) ci_2++;
            const bool matched = ci_2 == comment_str.length();
            assert(matched);
            if (ci + ci_2 < line.size() && line[ci + ci_2].Char == ' ') ci_2++;

            const Coordinates start{li, GetCharColumn(li, ci)}, end{li, GetCharColumn(li, ci + ci_2)};
            AddUndoOp(u, UndoOperationType::Delete, start, end);
            DeleteRange(start, end);
            Colorize(li, 1);
        }
    }
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u{State};
    if (AnyCursorHasSelection()) {
        for (int c = State.CurrentCursor; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            AddSelectionUndoOp(u, UndoOperationType::Delete, cursor);
            DeleteSelection(cursor);
        }
    }
    MoveHome();
    OnCursorPositionChanged(); // Might combine cursors.

    for (int c = State.CurrentCursor; c > -1; c--) {
        auto &cursor = State.Cursors[c];
        const int li = cursor.InteractiveEnd.L;
        const int next_li = li + 1, prev_li = li - 1;
        Coordinates to_delete_start, to_delete_end;
        if (int(Lines.size()) > next_li) { // Next line exists.
            to_delete_start = {li, 0};
            to_delete_end = {next_li, 0};
            SetCursorPosition({li, 0}, cursor);
        } else if (prev_li > -1) { // Previous line exists.
            to_delete_start = {prev_li, GetLineMaxColumn(prev_li)};
            to_delete_end = {li, GetLineMaxColumn(li)};
            SetCursorPosition({prev_li, 0}, cursor);
        } else {
            to_delete_start = {li, 0};
            to_delete_end = {li, GetLineMaxColumn(li)};
            SetCursorPosition({li, 0}, cursor);
        }

        AddUndoOp(u, UndoOperationType::Delete, to_delete_start, to_delete_end);
        if (to_delete_start.L != to_delete_end.L) {
            Lines.erase(Lines.begin() + li);
            for (uint other_c = 0; other_c <= State.CurrentCursor; other_c++) {
                const auto &other_cursor = State.Cursors[other_c];
                if (other_cursor == cursor) continue;

                const auto &end = other_cursor.InteractiveEnd;
                if (end.L >= li) SetCursorPosition({end.L - 1, end.C}, cursor);
            }
        } else {
            DeleteRange(to_delete_start, to_delete_end);
        }
    }

    AddUndo(u);
}

float TextEditor::TextDistanceToLineStart(const Coordinates &from, bool sanitize_coords) const {
    return (sanitize_coords ? SanitizeCoordinates(from) : from).C * CharAdvance.x;
}

void TextEditor::EnsureCursorVisible(bool start_too) {
    LastEnsureCursorVisible = State.GetLastAddedCursorIndex();
    LastEnsureCursorVisibleStartToo = start_too;
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates &coords) const {
    if (coords.L >= int(Lines.size())) {
        if (Lines.empty()) return {0, 0};

        const auto li = int(Lines.size()) - 1;
        return {li, GetLineMaxColumn(li)};
    }
    return {coords.L, Lines.empty() ? 0 : GetLineMaxColumn(coords.L, coords.C)};
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &position, bool insertion_mode, bool *is_over_li) const {
    static constexpr float PosToCoordsColumnOffset = 0.33;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 local{position.x - origin.x + 3.0f, position.y - origin.y};
    if (is_over_li != nullptr) *is_over_li = local.x < TextStart;

    Coordinates out{
        std::max(0, int(floor(local.y / CharAdvance.y))),
        std::max(0, int(floor((local.x - TextStart) / CharAdvance.x)))
    };
    const int ci = GetCharIndexL(out);
    if (ci > -1 && ci < int(Lines[out.L].size()) && Lines[out.L][ci].Char == '\t') {
        const int column_to_left = GetCharColumn(out.L, ci);
        const int column_to_right = GetCharColumn(out.L, GetCharIndexR(out));
        out.C = out.C - column_to_left < column_to_right - out.C ? column_to_left : column_to_right;
    } else {
        out.C = std::max(0, int(floor((local.x - TextStart + PosToCoordsColumnOffset * CharAdvance.x) / CharAdvance.x)));
    }
    return SanitizeCoordinates(out);
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &from) const {
    if (from.L >= int(Lines.size())) return from;

    const auto &line = Lines[from.L];

    int ci = GetCharIndexL(from);
    if (ci >= int(line.size()) || line.empty()) return from;
    if (ci == int(line.size())) ci--;

    const bool initial_is_word_char = IsWordChar(line[ci].Char);
    const bool initial_is_space = isspace(line[ci].Char);
    const char initial_char = line[ci].Char;
    int li = from.L;
    while (Move(li, ci, true, true)) {
        const bool is_word_char = IsWordChar(line[ci].Char);
        const bool is_space = isspace(line[ci].Char);
        if ((initial_is_space && !is_space) ||
            (initial_is_word_char && !is_word_char) ||
            (!initial_is_word_char && !initial_is_space && initial_char != line[ci].Char)) {
            Move(li, ci, false, true); // one step to the right
            break;
        }
    }
    return {from.L, GetCharColumn(from.L, ci)};
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &from) const {
    if (from.L >= int(Lines.size())) return from;

    const auto &line = Lines[from.L];

    int ci = GetCharIndexL(from);
    if (ci >= int(line.size())) return from;

    const bool initial_is_word_char = IsWordChar(line[ci].Char);
    const bool initial_is_space = isspace(line[ci].Char);
    const char initial_char = line[ci].Char;
    int li = from.L;
    while (Move(li, ci, false, true)) {
        if (ci == int(line.size())) break;

        const bool is_word_char = IsWordChar(line[ci].Char);
        const bool is_space = isspace(line[ci].Char);
        if ((initial_is_space && !is_space) ||
            (initial_is_word_char && !is_word_char) ||
            (!initial_is_word_char && !initial_is_space && initial_char != line[ci].Char))
            break;
    }
    return {li, GetCharColumn(from.L, ci)};
}

int TextEditor::GetCharIndexL(const Coordinates &coords) const {
    if (coords.L >= int(Lines.size())) return -1;

    const auto &line = Lines[coords.L];
    int i = 0, tab_coords_left = 0;
    for (int column = 0; column < coords.C && i < int(line.size()); column++) {
        if (line[i].Char == '\t') {
            if (tab_coords_left == 0) tab_coords_left = TabSizeAtColumn(column);
            if (tab_coords_left > 0) tab_coords_left--;
        }
        if (tab_coords_left == 0) i += UTF8CharLength(line[i].Char);
    }
    return i;
}

int TextEditor::GetCharIndexR(const Coordinates &coords) const {
    if (coords.L >= int(Lines.size())) return -1;

    int c = 0, i = 0;
    for (; i < int(Lines[coords.L].size()) && c < coords.C;) {
        MoveCharIndexAndColumn(coords.L, i, c);
    }
    return i;
}

int TextEditor::GetCharColumn(int li, int ci) const {
    if (li >= int(Lines.size())) return 0;

    int i = 0, c = 0;
    while (i < ci && i < int(Lines[li].size())) {
        MoveCharIndexAndColumn(li, i, c);
    }
    return c;
}

int TextEditor::GetFirstVisibleCharIndex(int li) const {
    if (li >= int(Lines.size())) return 0;

    int i = 0, c = 0;
    while (c < FirstVisibleColumn && i < int(Lines[li].size())) {
        MoveCharIndexAndColumn(li, i, c);
    }
    return c > FirstVisibleColumn ? i - 1 : i;
}

int TextEditor::GetLineMaxColumn(int li, int limit) const {
    if (li >= int(Lines.size())) return 0;

    int column = 0;
    for (int i = 0; i < int(Lines[li].size());) {
        MoveCharIndexAndColumn(li, i, column);
        if (limit != -1 && column > limit) return limit;
    }
    return column;
}

TextEditor::LineT &TextEditor::InsertLine(int li) {
    auto &result = *Lines.insert(Lines.begin() + li, LineT{});
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        const auto &end = cursor.InteractiveEnd;
        if (end.L >= li) SetCursorPosition({end.L + 1, end.C}, cursor);
    }

    return result;
}

void TextEditor::DeleteRange(const Coordinates &start, const Coordinates &end, const Cursor *exclude_cursor) {
    if (end <= start) return;

    const auto start_ci = GetCharIndexL(start), end_ci = GetCharIndexR(end);
    if (start.L == end.L) return RemoveGlyphsFromLine(start.L, start_ci, end_ci);

    RemoveGlyphsFromLine(start.L, start_ci, -1); // from start to end of line
    RemoveGlyphsFromLine(end.L, 0, end_ci);
    if (start.L == end.L) return;

    // At least one line completely removed.
    AddGlyphsToLine(start.L, Lines[start.L].size(), Lines[end.L]);

    // Move up all cursors after the last removed line.
    const uint num_removed_lines = end.L - start.L;
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        auto &cursor = State.Cursors[c];
        if (exclude_cursor != nullptr && cursor == *exclude_cursor) continue;

        if (cursor.InteractiveEnd.L >= end.L) {
            cursor.InteractiveStart.L -= num_removed_lines;
            cursor.InteractiveEnd.L -= num_removed_lines;
        }
    }

    Lines.erase(Lines.begin() + start.L + 1, Lines.begin() + end.L + 1);
}

void TextEditor::DeleteSelection(Cursor &cursor) {
    if (!cursor.HasSelection()) return;

    // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
    DeleteRange(cursor.GetSelectionStart(), cursor.GetSelectionEnd(), &cursor);
    SetCursorPosition(cursor.GetSelectionStart(), cursor);
    Colorize(cursor.GetSelectionStart().L, 1);
}

// Returns new character index for each affected cursor when other cursor writes/deletes are on the same line.
std::unordered_map<int, int> TextEditor::BeforeLineChanged(int li, int column, int char_count, bool is_deleted) {
    std::unordered_map<int, int> adjusted_ci_for_cursor;
    for (uint c = 0; c <= State.CurrentCursor; c++) {
        const auto &cursor = State.Cursors[c];
        // Cursor needs adjusting if it's to the right of a change, without a selection.
        if (cursor.InteractiveEnd.L == li && cursor.InteractiveEnd.C > column && !cursor.HasSelection()) {
            adjusted_ci_for_cursor[c] = GetCharIndexR({li, cursor.InteractiveEnd.C}) + (is_deleted ? -char_count : char_count);
        }
    }
    return adjusted_ci_for_cursor;
}

void TextEditor::AfterLineChanged(int li, std::unordered_map<int, int> &&adjusted_ci_for_cursor) {
    for (const auto &[c, ci] : adjusted_ci_for_cursor) {
        auto &cursor = State.Cursors[c];
        SetCursorPosition({li, GetCharColumn(li, ci)}, cursor);
    }
}

void TextEditor::RemoveGlyphsFromLine(int li, int ci, int end_ci) {
    const int column = GetCharColumn(li, ci);
    auto &line = Lines[li];
    // todo the `end_ci == -1` case is the only thing preventing this from being combined with `AddGlyphsToLine`.
    std::span<const Glyph> glyphs = {line.cbegin() + ci, end_ci == -1 ? line.cend() : line.cbegin() + end_ci};
    auto adjusted_ci_for_cursor = BeforeLineChanged(li, column, glyphs.size(), true);
    line.erase(glyphs.begin(), glyphs.end());
    AfterLineChanged(li, std::move(adjusted_ci_for_cursor));
}

void TextEditor::AddGlyphsToLine(int li, int ci, std::span<const Glyph> glyphs) {
    const int column = GetCharColumn(li, ci);
    auto &line = Lines[li];
    auto adjusted_ci_for_cursor = BeforeLineChanged(li, column, glyphs.size(), false);
    line.insert(line.begin() + ci, glyphs.begin(), glyphs.end());
    AfterLineChanged(li, std::move(adjusted_ci_for_cursor));
}

ImU32 TextEditor::GetGlyphColor(const Glyph &glyph) const {
    if (LanguageDef == nullptr) return Palette[int(PaletteIndex::Default)];
    if (glyph.IsComment) return Palette[int(PaletteIndex::Comment)];
    if (glyph.IsMultiLineComment) return Palette[int(PaletteIndex::MultiLineComment)];

    const auto color = Palette[int(glyph.ColorIndex)];
    if (glyph.IsPreprocessor) {
        const auto ppcolor = Palette[int(PaletteIndex::Preprocessor)];
        const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
        const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

static bool IsPressed(ImGuiKey key) {
    const auto key_index = ImGui::GetKeyIndex(key);
    const auto window_id = ImGui::GetCurrentWindowRead()->ID;
    ImGui::SetKeyOwner(key_index, window_id); // Prevent app from handling this key press.
    return ImGui::IsKeyPressed(key_index, window_id);
}

void TextEditor::HandleKeyboardInputs(bool is_parent_focused) {
    if (ImGui::IsWindowFocused() || is_parent_focused) {
        if (ImGui::IsWindowHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

        auto &io = ImGui::GetIO();
        const auto is_osx = io.ConfigMacOSXBehaviors;
        const auto alt = io.KeyAlt;
        const auto ctrl = io.KeyCtrl;
        const auto shift = io.KeyShift;
        const auto super = io.KeySuper;

        const bool is_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift;
        const bool is_shift_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && shift && !alt;
        const bool is_wordmove_key = is_osx ? alt : ctrl;
        const bool is_alt_only = alt && !ctrl && !shift && !super;
        const bool is_ctrl_only = ctrl && !alt && !shift && !super;
        const bool is_shift_only = shift && !alt && !ctrl && !super;

        io.WantCaptureKeyboard = true;
        io.WantTextInput = true;

        if (!ReadOnly && is_shortcut && IsPressed(ImGuiKey_Z))
            Undo();
        else if (!ReadOnly && is_alt_only && IsPressed(ImGuiKey_Backspace))
            Undo();
        else if (!ReadOnly && is_shortcut && IsPressed(ImGuiKey_Y))
            Redo();
        else if (!ReadOnly && is_shift_shortcut && IsPressed(ImGuiKey_Z))
            Redo();
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_UpArrow))
            MoveUp(1, shift);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_DownArrow))
            MoveDown(1, shift);
        else if ((is_osx ? !ctrl : !alt) && !super && IsPressed(ImGuiKey_LeftArrow))
            MoveLeft(shift, is_wordmove_key);
        else if ((is_osx ? !ctrl : !alt) && !super && IsPressed(ImGuiKey_RightArrow))
            MoveRight(shift, is_wordmove_key);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_PageUp))
            MoveUp(VisibleLineCount - 2, shift);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_PageDown))
            MoveDown(VisibleLineCount - 2, shift);
        else if (ctrl && !alt && !super && IsPressed(ImGuiKey_Home))
            MoveTop(shift);
        else if (ctrl && !alt && !super && IsPressed(ImGuiKey_End))
            MoveBottom(shift);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_Home))
            MoveHome(shift);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_End))
            MoveEnd(shift);
        else if (!ReadOnly && !alt && !shift && !super && IsPressed(ImGuiKey_Delete))
            Delete(ctrl);
        else if (!ReadOnly && !alt && !shift && !super && IsPressed(ImGuiKey_Backspace))
            Backspace(ctrl);
        else if (!ReadOnly && !alt && ctrl && shift && !super && IsPressed(ImGuiKey_K))
            RemoveCurrentLines();
        else if (!ReadOnly && !alt && ctrl && !shift && !super && IsPressed(ImGuiKey_LeftBracket))
            ChangeCurrentLinesIndentation(false);
        else if (!ReadOnly && !alt && ctrl && !shift && !super && IsPressed(ImGuiKey_RightBracket))
            ChangeCurrentLinesIndentation(true);
        else if (!ReadOnly && !alt && ctrl && shift && !super && IsPressed(ImGuiKey_UpArrow))
            MoveCurrentLines(true);
        else if (!ReadOnly && !alt && ctrl && shift && !super && IsPressed(ImGuiKey_DownArrow))
            MoveCurrentLines(false);
        else if (!ReadOnly && !alt && ctrl && !shift && !super && IsPressed(ImGuiKey_Slash))
            ToggleLineComment();
        else if (!alt && !ctrl && !shift && !super && IsPressed(ImGuiKey_Insert))
            Overwrite ^= true;
        else if (is_ctrl_only && IsPressed(ImGuiKey_Insert))
            Copy();
        else if (is_shortcut && IsPressed(ImGuiKey_C))
            Copy();
        else if (!ReadOnly && is_shift_only && IsPressed(ImGuiKey_Insert))
            Paste();
        else if (!ReadOnly && is_shortcut && IsPressed(ImGuiKey_V))
            Paste();
        else if ((is_shortcut && IsPressed(ImGuiKey_X)) || (is_shift_only && IsPressed(ImGuiKey_Delete)))
            if (ReadOnly) Copy();
            else Cut();
        else if (is_shortcut && IsPressed(ImGuiKey_A))
            SelectAll();
        else if (is_shortcut && IsPressed(ImGuiKey_D))
            AddCursorForNextOccurrence();
        else if (!ReadOnly && !alt && !ctrl && !shift && !super && (IsPressed(ImGuiKey_Enter) || IsPressed(ImGuiKey_KeypadEnter)))
            EnterChar('\n', false);
        else if (!ReadOnly && !alt && !ctrl && !super && IsPressed(ImGuiKey_Tab))
            EnterChar('\t', shift);
        if (!ReadOnly && !io.InputQueueCharacters.empty() && ctrl == alt && !super) {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                const auto ch = io.InputQueueCharacters[i];
                if (ch != 0 && (ch == '\n' || ch >= 32)) EnterChar(ch, shift);
            }
            io.InputQueueCharacters.resize(0);
        }
    }
}

static float Distance(const ImVec2 &a, const ImVec2 &b) {
    const float x = a.x - b.x, y = a.y - b.y;
    return sqrt(x * x + y * y);
}

void TextEditor::HandleMouseInputs() {
    auto &io = ImGui::GetIO();
    const auto shift = io.KeyShift;
    const auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    const auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    // Pan with middle mouse button
    Panning &= ImGui::IsMouseDown(2);
    if (Panning && ImGui::IsMouseDragging(2)) {
        ImVec2 scroll{ImGui::GetScrollX(), ImGui::GetScrollY()};
        ImVec2 mouse_pos = ImGui::GetMouseDragDelta(2);
        ImVec2 mouse_delta = mouse_pos - LastMousePos;
        ImGui::SetScrollY(scroll.y - mouse_delta.y);
        ImGui::SetScrollX(scroll.x - mouse_delta.x);
        LastMousePos = mouse_pos;
    }

    // Mouse left button dragging (=> update selection)
    IsDraggingSelection &= ImGui::IsMouseDown(0);
    if (IsDraggingSelection && ImGui::IsMouseDragging(0)) {
        io.WantCaptureMouse = true;
        const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
        SetCursorPosition(cursor_coords, State.GetLastAddedCursor(), false);
    }

    if (ImGui::IsWindowHovered()) {
        const auto is_click = ImGui::IsMouseClicked(0);
        if (!shift && !alt) {
            if (is_click) IsDraggingSelection = true;

            // Pan with middle mouse button
            if (ImGui::IsMouseClicked(2)) {
                Panning = true;
                LastMousePos = ImGui::GetMouseDragDelta(2);
            }

            const bool is_double_click = ImGui::IsMouseDoubleClicked(0);
            const auto t = ImGui::GetTime();
            const bool is_triple_click = is_click && !is_double_click && (LastClickTime != -1.0f && (t - LastClickTime) < io.MouseDoubleClickTime && Distance(io.MousePos, LastClickPos) < 0.01f);
            if (is_triple_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                const auto target_cursor_pos = cursor_coords.L < int(Lines.size()) - 1 ?
                    Coordinates{cursor_coords.L + 1, 0} :
                    Coordinates{cursor_coords.L, GetLineMaxColumn(cursor_coords.L)};
                SetSelection({cursor_coords.L, 0}, target_cursor_pos, State.GetCursor());

                LastClickTime = -1.0f;
            } else if (is_double_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(FindWordStart(cursor_coords), FindWordEnd(cursor_coords), State.GetCursor());

                LastClickTime = float(ImGui::GetTime());
                LastClickPos = io.MousePos;
            } else if (is_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                bool is_over_li;
                const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite, &is_over_li);
                if (is_over_li) {
                    const auto target_cursor_pos = cursor_coords.L < int(Lines.size()) - 1 ?
                        Coordinates{cursor_coords.L + 1, 0} :
                        Coordinates{cursor_coords.L, GetLineMaxColumn(cursor_coords.L)};
                    SetSelection({cursor_coords.L, 0}, target_cursor_pos, State.GetCursor());
                } else {
                    SetCursorPosition(cursor_coords, State.GetLastAddedCursor());
                }

                LastClickTime = float(ImGui::GetTime());
                LastClickPos = io.MousePos;
            } else if (ImGui::IsMouseReleased(0)) {
                SortAndMergeCursors();
            }
        } else if (shift && is_click) {
            const auto new_selection = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
            SetCursorPosition(SanitizeCoordinates(new_selection), State.GetCursor(), false);
        }
    }
}

void TextEditor::UpdateViewVariables(float scroll_x, float scroll_y) {
    static constexpr float ImGuiScrollbarWidth = 14;

    ContentHeight = ImGui::GetWindowHeight() - (IsHorizontalScrollbarVisible() ? ImGuiScrollbarWidth : 0.0f);
    ContentWidth = ImGui::GetWindowWidth() - (IsVerticalScrollbarVisible() ? ImGuiScrollbarWidth : 0.0f);

    VisibleLineCount = std::max(int(ceil(ContentHeight / CharAdvance.y)), 0);
    FirstVisibleLineI = std::max(int(scroll_y / CharAdvance.y), 0);
    LastVisibleLineI = std::max(int((ContentHeight + scroll_y) / CharAdvance.y), 0);

    VisibleColumnCount = std::max(int(ceil((ContentWidth - std::max(TextStart - scroll_x, 0.0f)) / CharAdvance.x)), 0);
    FirstVisibleColumn = std::max(int(std::max(scroll_x - TextStart, 0.0f) / CharAdvance.x), 0);
    LastVisibleColumn = std::max(int((ContentWidth + scroll_x - TextStart) / CharAdvance.x), 0);
}

void TextEditor::Render(bool is_parent_focused) {
    /* Compute CharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
    const float font_width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    const float font_height = ImGui::GetTextLineHeightWithSpacing();
    CharAdvance = {font_width, font_height * LineSpacing};

    // Deduce `TextStart` by evaluating `Lines` size plus two spaces as text width.
    TextStart = LeftMargin;
    static char li_buffer[16];
    if (ShowLineNumbers) {
        snprintf(li_buffer, 16, " %d ", int(Lines.size()));
        TextStart += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, li_buffer, nullptr, nullptr).x;
    }
    const ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
    ScrollX = ImGui::GetScrollX();
    ScrollY = ImGui::GetScrollY();
    UpdateViewVariables(ScrollX, ScrollY);

    int max_column_limited = 0;
    if (!Lines.empty()) {
        auto dl = ImGui::GetWindowDrawList();
        const float font_size = ImGui::GetFontSize();
        const float space_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, " ").x;

        for (int li = FirstVisibleLineI; li <= LastVisibleLineI && li < int(Lines.size()); li++) {
            const auto &line = Lines[li];
            max_column_limited = std::max(GetLineMaxColumn(li, LastVisibleColumn), max_column_limited);

            const ImVec2 line_start_screen_pos{cursor_screen_pos.x, cursor_screen_pos.y + li * CharAdvance.y};
            const float text_screen_pos_x = line_start_screen_pos.x + TextStart;
            const Coordinates line_start_coord{li, 0}, line_end_coord{li, max_column_limited};
            // Draw selection for the current line
            for (uint c = 0; c <= State.CurrentCursor; c++) {
                const auto &cursor = State.Cursors[c];
                const auto cursor_selection_start = cursor.GetSelectionStart();
                const auto cursor_selection_end = cursor.GetSelectionEnd();
                assert(cursor_selection_start <= cursor_selection_end);

                float rect_start = -1.0f, rect_end = -1.0f;
                if (cursor_selection_start <= line_end_coord)
                    rect_start = cursor_selection_start > line_start_coord ? TextDistanceToLineStart(cursor_selection_start) : 0.0f;
                if (cursor_selection_end > line_start_coord)
                    rect_end = TextDistanceToLineStart(cursor_selection_end < line_end_coord ? cursor_selection_end : line_end_coord);
                if (cursor_selection_end.L > li || (cursor_selection_end.L == li && cursor_selection_end > line_end_coord))
                    rect_end += CharAdvance.x;

                if (rect_start != -1 && rect_end != -1 && rect_start < rect_end) {
                    dl->AddRectFilled(
                        {text_screen_pos_x + rect_start, line_start_screen_pos.y},
                        {text_screen_pos_x + rect_end, line_start_screen_pos.y + CharAdvance.y},
                        Palette[int(PaletteIndex::Selection)]
                    );
                }
            }

            // Draw line number (right aligned)
            if (ShowLineNumbers) {
                snprintf(li_buffer, 16, "%d  ", li + 1);
                const float line_num_width = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, li_buffer).x;
                dl->AddText({text_screen_pos_x - line_num_width, line_start_screen_pos.y}, Palette[int(PaletteIndex::LineNumber)], li_buffer);
            }

            std::vector<Coordinates> cursor_coords_in_this_line;
            for (uint c = 0; c <= State.CurrentCursor; c++) {
                const auto &cursor = State.Cursors[c];
                if (cursor.InteractiveEnd.L == li) cursor_coords_in_this_line.push_back(cursor.InteractiveEnd);
            }
            if (cursor_coords_in_this_line.size() > 0) {
                // Render the cursors
                if (ImGui::IsWindowFocused() || is_parent_focused) {
                    for (const auto &cursor_coords : cursor_coords_in_this_line) {
                        float width = 1.0f;
                        const int ci = GetCharIndexR(cursor_coords);
                        const float cx = TextDistanceToLineStart(cursor_coords);
                        if (Overwrite && ci < int(line.size())) {
                            if (line[ci].Char == '\t') {
                                const auto x = (1.0f + std::floor((1.0f + cx) / (float(TabSize) * space_size))) * (float(TabSize) * space_size);
                                width = x - cx;
                            } else {
                                width = CharAdvance.x;
                            }
                        }
                        const ImVec2 cstart{text_screen_pos_x + cx, line_start_screen_pos.y};
                        const ImVec2 cend{text_screen_pos_x + cx + width, line_start_screen_pos.y + CharAdvance.y};
                        dl->AddRectFilled(cstart, cend, Palette[int(PaletteIndex::Cursor)]);
                        if (CursorOnBracket) {
                            const ImVec2 top_left{cstart.x, line_start_screen_pos.y + font_height + 1.0f};
                            const ImVec2 bottom_right{top_left.x + CharAdvance.x, top_left.y + 1.0f};
                            dl->AddRectFilled(top_left, bottom_right, Palette[int(PaletteIndex::Cursor)]);
                        }
                    }
                }
            }

            // Render colorized text
            int ci = GetFirstVisibleCharIndex(li);
            int column = FirstVisibleColumn; // can be in the middle of tab character
            while (ci < int(Lines[li].size()) && column <= LastVisibleColumn) {
                const auto &glyph = line[ci];
                const ImVec2 glyph_pos = line_start_screen_pos + ImVec2{TextStart + TextDistanceToLineStart({li, column}, false), 0};
                if (glyph.Char == '\t') {
                    if (ShowWhitespaces) {
                        const ImVec2 p1{glyph_pos + ImVec2{CharAdvance.x * 0.3f, font_height * 0.5f}};
                        const ImVec2 p2{
                            glyph_pos.x + (ShortTabs ? (TabSizeAtColumn(column) * CharAdvance.x - CharAdvance.x * 0.3f) : CharAdvance.x),
                            p1.y
                        };
                        const float gap = ImGui::GetFontSize() * (ShortTabs ? 0.16f : 0.2f);
                        const ImVec2 p3{p2.x - gap, p1.y - gap}, p4{p2.x - gap, p1.y + gap};
                        const ImU32 color = Palette[int(PaletteIndex::ControlCharacter)];
                        dl->AddLine(p1, p2, color);
                        dl->AddLine(p2, p3, color);
                        dl->AddLine(p2, p4, color);
                    }
                } else if (glyph.Char == ' ') {
                    if (ShowWhitespaces) {
                        dl->AddCircleFilled(
                            glyph_pos + ImVec2{space_size, ImGui::GetFontSize()} * 0.5f,
                            1.5f, Palette[int(PaletteIndex::ControlCharacter)], 4
                        );
                    }
                } else {
                    const uint seq_length = UTF8CharLength(glyph.Char);
                    if (CursorOnBracket && seq_length == 1 && MatchingBracketCoords == Coordinates{li, column}) {
                        const ImVec2 top_left{glyph_pos + ImVec2{0, font_height + 1.0f}};
                        dl->AddRectFilled(top_left, top_left + ImVec2{CharAdvance.x, 1.0f}, Palette[int(PaletteIndex::Cursor)]);
                    }
                    string glyph_buffer;
                    for (uint i = 0; i < seq_length; i++) glyph_buffer.push_back(line[ci + i].Char);
                    dl->AddText(glyph_pos, GetGlyphColor(glyph), glyph_buffer.c_str());
                }
                MoveCharIndexAndColumn(li, ci, column);
            }
        }
    }
    CurrentSpaceHeight = (Lines.size() + std::min(VisibleLineCount - 1, int(Lines.size()))) * CharAdvance.y;
    CurrentSpaceWidth = std::max((max_column_limited + std::min(VisibleColumnCount - 1, max_column_limited)) * CharAdvance.x, CurrentSpaceWidth);

    ImGui::SetCursorPos({0, 0});
    ImGui::Dummy({CurrentSpaceWidth, CurrentSpaceHeight});
    if (LastEnsureCursorVisible > -1) {
        // First pass for interactive end and second pass for interactive start.
        for (int i = 0; i < (LastEnsureCursorVisibleStartToo ? 2 : 1); i++) {
            if (i) UpdateViewVariables(ScrollX, ScrollY); // Second pass depends on changes made in first pass
            const auto &cursor = State.GetCursor(LastEnsureCursorVisible);
            const auto target = SanitizeCoordinates(i > 0 ? cursor.InteractiveStart : cursor.InteractiveEnd);
            if (target.L <= FirstVisibleLineI) {
                float scroll = std::max(0.0f, (target.L - 0.5f) * CharAdvance.y);
                if (scroll < ScrollY) ImGui::SetScrollY(scroll);
            }
            if (target.L >= LastVisibleLineI) {
                float scroll = std::max(0.0f, (target.L + 1.5f) * CharAdvance.y - ContentHeight);
                if (scroll > ScrollY) ImGui::SetScrollY(scroll);
            }
            if (target.C <= FirstVisibleColumn) {
                if (target.C >= LastVisibleColumn) {
                    float scroll = std::max(0.0f, TextStart + (target.C + 0.5f) * CharAdvance.x - ContentWidth);
                    if (scroll > ScrollX) ImGui::SetScrollX(ScrollX = scroll);
                } else {
                    float scroll = std::max(0.0f, TextStart + (target.C - 0.5f) * CharAdvance.x);
                    if (scroll < ScrollX) ImGui::SetScrollX(ScrollX = scroll);
                }
            }
        }
        LastEnsureCursorVisible = -1;
    }
    if (ScrollToTop) {
        ScrollToTop = false;
        ImGui::SetScrollY(0);
    }
    if (SetViewAtLineI > -1) {
        float scroll;
        switch (SetViewAtLineMode) {
            default:
            case SetViewAtLineMode::FirstVisibleLine:
                scroll = std::max(0.0f, SetViewAtLineI * CharAdvance.y);
                break;
            case SetViewAtLineMode::LastVisibleLine:
                scroll = std::max(0.0f, (SetViewAtLineI - (LastVisibleLineI - FirstVisibleLineI)) * CharAdvance.y);
                break;
            case SetViewAtLineMode::Centered:
                scroll = std::max(0.0f, (SetViewAtLineI - (LastVisibleLineI - FirstVisibleLineI) * 0.5f) * CharAdvance.y);
                break;
        }
        ImGui::SetScrollY(scroll);
        SetViewAtLineI = -1;
    }
}

void TextEditor::OnCursorPositionChanged() {
    const auto &cursor = State.Cursors[0];
    const bool one_cursor_without_selection = State.CurrentCursor == 0 && !cursor.HasSelection();
    CursorOnBracket = one_cursor_without_selection ? FindMatchingBracket(cursor.InteractiveEnd.L, GetCharIndexR(cursor.InteractiveEnd), MatchingBracketCoords) : false;

    if (!IsDraggingSelection) SortAndMergeCursors();
}

void TextEditor::SortAndMergeCursors() {
    State.SortCursors();

    std::unordered_set<const Cursor *> delete_cursors;
    if (AnyCursorHasSelection()) {
        // Merge cursors if they overlap.
        for (int c = State.CurrentCursor; c > 0; c--) {
            const auto &cursor = State.Cursors[c];
            auto &prev_cursor = State.Cursors[c - 1];
            if (prev_cursor.GetSelectionEnd() >= cursor.GetSelectionEnd()) {
                delete_cursors.insert(&cursor);
            } else if (prev_cursor.GetSelectionEnd() > cursor.GetSelectionStart()) {
                const auto pc_start = prev_cursor.GetSelectionStart(), pc_end = prev_cursor.GetSelectionEnd();
                prev_cursor.InteractiveStart = pc_start;
                prev_cursor.InteractiveEnd = pc_end;
                delete_cursors.insert(&cursor);
            }
        }
    } else {
        // Merge cursors if they are at the same position.
        for (uint c = 1; c < State.CurrentCursor; c++) {
            const auto &cursor = State.Cursors[c];
            if (State.Cursors[c - 1].InteractiveEnd == cursor.InteractiveEnd) delete_cursors.insert(&cursor);
        }
    }
    std::erase_if(State.Cursors, [&delete_cursors](const auto &c) { return delete_cursors.contains(&c); });

    State.CurrentCursor -= delete_cursors.size();
}

void TextEditor::AddUndo(UndoRecord &record) {
    record.After = State;
    UndoBuffer.resize(size_t(UndoIndex + 1));
    UndoBuffer.back() = record;
    ++UndoIndex;
}

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML
void TextEditor::Colorize(int from_li, int line_count) {
    const int to_line = line_count == -1 ? int(Lines.size()) : std::min(int(Lines.size()), from_li + line_count);
    ColorRangeMin = std::min(ColorRangeMin, from_li);
    ColorRangeMax = std::max(ColorRangeMax, to_line);
    ColorRangeMin = std::max(0, ColorRangeMin);
    ColorRangeMax = std::max(ColorRangeMin, ColorRangeMax);
    ShouldCheckComments = true;
}

void TextEditor::ColorizeRange(int from_li, int to_li) {
    if (Lines.empty() || from_li >= to_li || LanguageDef == nullptr) return;

    string buffer, id;
    std::cmatch results;
    const int end_li = std::clamp(to_li, 0, int(Lines.size()));
    for (int i = from_li; i < end_li; ++i) {
        auto &line = Lines[i];
        if (line.empty()) continue;

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j) {
            auto &col = line[j];
            buffer[j] = col.Char;
            col.ColorIndex = PaletteIndex::Default;
        }

        const char *buffer_begin = &buffer.front();
        const char *buffer_end = buffer_begin + buffer.size();
        for (auto first = buffer_begin; first != buffer_end;) {
            const char *token_begin = nullptr, *token_end = nullptr;
            PaletteIndex token_color = PaletteIndex::Default;
            bool has_tokenize_results = LanguageDef->Tokenize != nullptr && LanguageDef->Tokenize(first, buffer_end, token_begin, token_end, token_color);
            if (!has_tokenize_results) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);
                for (const auto &p : RegexList) {
                    if (std::regex_search(first, buffer_end, results, p.first, std::regex_constants::match_continuous)) {
                        has_tokenize_results = true;

                        const auto &v = *results.begin();
                        token_begin = v.first;
                        token_end = v.second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (!has_tokenize_results) {
                first++;
            } else {
                const size_t token_length = token_end - token_begin;
                if (token_color == PaletteIndex::Identifier) {
                    id.assign(token_begin, token_end);

                    // todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                    if (!LanguageDef->IsCaseSensitive) {
                        std::transform(id.begin(), id.end(), id.begin(), ::toupper);
                    }
                    if (!line[first - buffer_begin].IsPreprocessor) {
                        if (LanguageDef->Keywords.count(id) != 0) token_color = PaletteIndex::Keyword;
                        else if (LanguageDef->Identifiers.count(id) != 0) token_color = PaletteIndex::KnownIdentifier;
                    }
                }

                for (size_t j = 0; j < token_length; ++j) {
                    line[(token_begin - buffer_begin) + j].ColorIndex = token_color;
                }
                first = token_end;
            }
        }
    }
}

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool ColorizerEquals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!p(*first1, *first2)) return false;
    }
    return first1 == last1 && first2 == last2;
}

void TextEditor::ColorizeInternal() {
    if (Lines.empty() || LanguageDef == nullptr) return;

    if (ShouldCheckComments) {
        bool within_string = false;
        bool within_single_line_comment = false;
        bool within_preproc = false;
        bool first_char = true; // there is no other non-whitespace characters in the line before
        bool concatenate = false; // '\' on the very end of the line
        int li = 0, i = 0;
        int end_i = 0, end_li = Lines.size();
        int comment_start_li = end_li, comment_start_i = end_i;
        while (li < end_li || i < end_i) {
            auto &line = Lines[li];
            if (i == 0 && !concatenate) {
                within_single_line_comment = false;
                within_preproc = false;
                first_char = true;
            }

            concatenate = false;
            if (!line.empty()) {
                const auto &g = line[i];
                const auto ch = g.Char;
                if (ch != LanguageDef->PreprocChar && !isspace(ch)) first_char = false;
                if (i == int(line.size()) - 1 && line[line.size() - 1].Char == '\\') concatenate = true;

                bool in_comment = (comment_start_li < li || (comment_start_li == li && comment_start_i <= i));
                if (within_string) {
                    line[i].IsMultiLineComment = in_comment;
                    if (ch == '\"') {
                        if (i + 1 < int(line.size()) && line[i + 1].Char == '\"') {
                            i += 1;
                            if (i < int(line.size())) line[i].IsMultiLineComment = in_comment;
                        } else {
                            within_string = false;
                        }
                    } else if (ch == '\\') {
                        i += 1;
                        if (i < int(line.size())) line[i].IsMultiLineComment = in_comment;
                    }
                } else {
                    if (first_char && ch == LanguageDef->PreprocChar) within_preproc = true;
                    if (ch == '\"') {
                        within_string = true;
                        line[i].IsMultiLineComment = in_comment;
                    } else {
                        static const auto pred = [](const char &a, const Glyph &b) { return a == b.Char; };
                        const auto from = line.begin() + i;
                        const auto &start_str = LanguageDef->CommentStart;
                        const auto &single_start_str = LanguageDef->SingleLineComment;
                        if (!within_single_line_comment && i + start_str.size() <= line.size() &&
                            ColorizerEquals(start_str.cbegin(), start_str.cend(), from, from + start_str.size(), pred)) {
                            comment_start_li = li;
                            comment_start_i = i;
                        } else if (single_start_str.size() > 0 && i + single_start_str.size() <= line.size() && ColorizerEquals(single_start_str.begin(), single_start_str.end(), from, from + single_start_str.size(), pred)) {
                            within_single_line_comment = true;
                        }

                        in_comment = comment_start_li < li || (comment_start_li == li && comment_start_i <= i);
                        line[i].IsMultiLineComment = in_comment;
                        line[i].IsComment = within_single_line_comment;

                        const auto &end_str = LanguageDef->CommentEnd;
                        if (i + 1 >= int(end_str.size()) &&
                            ColorizerEquals(end_str.cbegin(), end_str.cend(), from + 1 - end_str.size(), from + 1, pred)) {
                            comment_start_i = end_i;
                            comment_start_li = end_li;
                        }
                    }
                }
                if (i < int(line.size())) line[i].IsPreprocessor = within_preproc;
                i += UTF8CharLength(ch);
                if (i >= int(line.size())) {
                    i = 0;
                    ++li;
                }
            } else {
                i = 0;
                ++li;
            }
        }
        ShouldCheckComments = false;
    }

    if (ColorRangeMin < ColorRangeMax) {
        const int increment = (LanguageDef->Tokenize == nullptr) ? 10 : 10000;
        const int to = std::min(ColorRangeMin + increment, ColorRangeMax);
        ColorizeRange(ColorRangeMin, to);
        ColorRangeMin = to;
        if (ColorRangeMax == ColorRangeMin) {
            ColorRangeMin = std::numeric_limits<int>::max();
            ColorRangeMax = 0;
        }
        return;
    }
}

int TextEditor::InsertTextAt(/* inout */ Coordinates &at, const char *text) {
    int ci = GetCharIndexR(at);
    int total_lines = 0;
    while (*text != '\0') {
        assert(!Lines.empty());
        if (*text == '\r') {
            text++; // skip
        } else if (*text == '\n') {
            if (ci < int(Lines[at.L].size())) {
                InsertLine(at.L + 1);
                const auto &line = Lines[at.L];
                AddGlyphsToLine(at.L + 1, 0, {line.cbegin() + ci, line.cend()});
                RemoveGlyphsFromLine(at.L, ci, -1);
            } else {
                InsertLine(at.L + 1);
            }
            ++at.L;
            at.C = 0;
            ci = 0;
            ++total_lines;
            ++text;
        } else {
            std::vector<Glyph> glyphs;
            const uint length = UTF8CharLength(*text);
            for (uint d = 0; d < length && *text != '\0'; d++) glyphs.emplace_back(*text++, PaletteIndex::Default);
            AddGlyphsToLine(at.L, ci, glyphs);
            ci += glyphs.size();
            at.C = GetCharColumn(at.L, ci);
        }
    }

    return total_lines;
}

const TextEditor::PaletteT TextEditor::DarkPalette = {{
    0xdcdfe4ff, // Default
    0xe06c75ff, // Keyword
    0xe5c07bff, // Number
    0x98c379ff, // String
    0xe0a070ff, // Char literal
    0x6a7384ff, // Punctuation
    0x808040ff, // Preprocessor
    0xdcdfe4ff, // Identifier
    0x61afefff, // Known identifier
    0xc678ddff, // Preproc identifier
    0x3696a2ff, // Comment (single line)
    0x3696a2ff, // Comment (multi line)
    0x282c34ff, // Background
    0xe0e0e0ff, // Cursor
    0x2060a080, // Selection
    0xff200080, // ErrorMarker
    0xffffff15, // ControlCharacter
    0x0080f040, // Breakpoint
    0x7a8394ff, // Line number
    0x00000040, // Current line fill
    0x80808040, // Current line fill (inactive)
    0xa0a0a040, // Current line edge
}};

const TextEditor::PaletteT TextEditor::MarianaPalette = {{
    0xffffffff, // Default
    0xc695c6ff, // Keyword
    0xf9ae58ff, // Number
    0x99c794ff, // String
    0xe0a070ff, // Char literal
    0x5fb4b4ff, // Punctuation
    0x808040ff, // Preprocessor
    0xffffffff, // Identifier
    0x4dc69bff, // Known identifier
    0xe0a0ffff, // Preproc identifier
    0xa6acb9ff, // Comment (single line)
    0xa6acb9ff, // Comment (multi line)
    0x303841ff, // Background
    0xe0e0e0ff, // Cursor
    0x4e5a6580, // Selection
    0xec5f6680, // ErrorMarker
    0xffffff30, // ControlCharacter
    0x0080f040, // Breakpoint
    0xffffffb0, // Line number
    0x4e5a6580, // Current line fill
    0x4e5a6530, // Current line fill (inactive)
    0x4e5a65b0, // Current line edge
}};

const TextEditor::PaletteT TextEditor::LightPalette = {{
    0x404040ff, // None
    0x060cffff, // Keyword
    0x008000ff, // Number
    0xa02020ff, // String
    0x704030ff, // Char literal
    0x000000ff, // Punctuation
    0x606040ff, // Preprocessor
    0x404040ff, // Identifier
    0x106060ff, // Known identifier
    0xa040c0ff, // Preproc identifier
    0x205020ff, // Comment (single line)
    0x205040ff, // Comment (multi line)
    0xffffffff, // Background
    0x000000ff, // Cursor
    0x00006040, // Selection
    0xff1000a0, // ErrorMarker
    0x90909090, // ControlCharacter
    0x0080f080, // Breakpoint
    0x005050ff, // Line number
    0x00000040, // Current line fill
    0x80808040, // Current line fill (inactive)
    0x00000040, // Current line edge
}};

const TextEditor::PaletteT TextEditor::RetroBluePalette = {{
    0xffff00ff, // None
    0x00ffffff, // Keyword
    0x00ff00ff, // Number
    0x008080ff, // String
    0x008080ff, // Char literal
    0xffffffff, // Punctuation
    0x008000ff, // Preprocessor
    0xffff00ff, // Identifier
    0xffffffff, // Known identifier
    0xff00ffff, // Preproc identifier
    0x808080ff, // Comment (single line)
    0x404040ff, // Comment (multi line)
    0x000080ff, // Background
    0xff8000ff, // Cursor
    0x00ffff80, // Selection
    0xff0000a0, // ErrorMarker
    0x0080ff80, // Breakpoint
    0x008080ff, // Line number
    0x00000040, // Current line fill
    0x80808040, // Current line fill (inactive)
    0x00000040, // Current line edge
}};
