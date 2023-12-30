#include "TextEditor.h"

#include "LanguageDefinition.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <string>

#include "imgui_internal.h"

using std::string, std::ranges::reverse_view, std::ranges::any_of, std::ranges::all_of;

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

    for (uint i = 0; i < uint(PaletteIndex::Max); ++i) {
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

    Colorize(0, Lines.size());
}

const char *TextEditor::GetLanguageDefinitionName() const { return LanguageDef != nullptr ? LanguageDef->Name.c_str() : "None"; }

void TextEditor::SetTabSize(uint tab_size) { TabSize = std::clamp(tab_size, 1u, 8u); }
void TextEditor::SetLineSpacing(float line_spacing) { LineSpacing = std::clamp(line_spacing, 1.f, 2.f); }

void TextEditor::SelectAll() {
    for (auto &c : State.Cursors) c.End = c.Start = c.SelectionEnd();
    State.ResetCursors();
    MoveTop();
    MoveBottom(true);
}

bool TextEditor::AnyCursorHasSelection() const {
    return any_of(State.Cursors, [](const auto &c) { return c.HasSelection(); });
}
bool TextEditor::AnyCursorHasMultilineSelection() const {
    return any_of(State.Cursors, [](const auto &c) { return c.HasMultilineSelection(); });
}
bool TextEditor::AllCursorsHaveSelection() const {
    return all_of(State.Cursors, [](const auto &c) { return c.HasSelection(); });
}

TextEditor::Coordinates TextEditor::GetCursorPosition() const { return SanitizeCoordinates(State.GetCursor().End); }

void TextEditor::Copy() {
    if (AnyCursorHasSelection()) {
        string str;
        for (const auto &c : State.Cursors) {
            if (c.HasSelection()) {
                if (!str.empty()) str += '\n';
                str += GetSelectedText(c);
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
    if (!AnyCursorHasSelection()) return;

    UndoRecord u{State};
    Copy();
    for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);
    AddUndo(u);
}

void TextEditor::Paste() {
    // Check if we should do multicursor paste.
    const string clip_text = ImGui::GetClipboardText();
    bool can_paste_to_multiple_cursors = false;
    std::vector<std::pair<uint, uint>> clip_text_lines;
    if (State.Cursors.size() > 1) {
        clip_text_lines.push_back({0, 0});
        for (uint i = 0; i < clip_text.length(); i++) {
            if (clip_text[i] == '\n') {
                clip_text_lines.back().second = i;
                clip_text_lines.push_back({i + 1, 0});
            }
        }
        clip_text_lines.back().second = clip_text.length();
        can_paste_to_multiple_cursors = clip_text_lines.size() == State.Cursors.size() + 1;
    }

    if (clip_text.length() > 0) {
        UndoRecord u{State};
        for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);

        for (int c = State.Cursors.size() - 1; c > -1; c--) {
            auto &cursor = State.Cursors[c];
            const auto before_end = SanitizeCoordinates(cursor.End);
            const string insert_text = can_paste_to_multiple_cursors ? clip_text.substr(clip_text_lines[c].first, clip_text_lines[c].second - clip_text_lines[c].first) : clip_text;
            InsertTextAtCursor(insert_text, cursor);
            u.Operations.emplace_back(insert_text, before_end, SanitizeCoordinates(cursor.End), UndoOperationType::Add);
        }

        AddUndo(u);
    }
}

void TextEditor::Undo(uint steps) {
    while (CanUndo() && steps-- > 0) UndoBuffer[--UndoIndex].Undo(this);
}
void TextEditor::Redo(uint steps) {
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

    Colorize(0, Lines.size());
}

void TextEditor::AddUndoOp(UndoRecord &record, UndoOperationType type, const Coordinates &start, const Coordinates &end) {
    record.Operations.emplace_back(GetText(start, end), start, end, type);
}

string TextEditor::GetText(const Coordinates &start, const Coordinates &end) const {
    if (end == start) return "";

    assert(end > start);
    uint line_start = start.L, line_end = end.L;
    uint start_ci = GetCharIndexR(start);
    const auto end_ci = GetCharIndexR(end);

    uint s = 0;
    for (uint i = line_start; i < line_end; i++) s += Lines[i].size();

    string result;
    result.reserve(s + s / 8);
    while (start_ci < end_ci || line_start < line_end) {
        if (line_start >= Lines.size()) break;

        const auto &line = Lines[line_start];
        if (start_ci < line.size()) {
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
    return UTF8CharLength(ch) > 1 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

void TextEditor::EditorState::AddCursor() {
    Cursors.push_back({});
    LastAddedCursorIndex = Cursors.size() - 1;
}
void TextEditor::EditorState::ResetCursors() {
    Cursors.clear();
    Cursors.push_back({});
    LastAddedCursorIndex = 0;
}

uint TextEditor::EditorState::GetLastAddedCursorIndex() { return LastAddedCursorIndex >= Cursors.size() ? 0 : LastAddedCursorIndex; }
TextEditor::Cursor &TextEditor::EditorState::GetLastAddedCursor() { return Cursors[GetLastAddedCursorIndex()]; }
TextEditor::Cursor &TextEditor::EditorState::GetCursor(uint c) { return Cursors[c]; }
TextEditor::Cursor const &TextEditor::EditorState::GetCursor(uint c) const { return Cursors[c]; }
TextEditor::Cursor &TextEditor::EditorState::GetCursor() { return Cursors.back(); }
TextEditor::Cursor const &TextEditor::EditorState::GetCursor() const { return Cursors.back(); }

void TextEditor::EditorState::SortCursors() {
    const auto last_added_cursor_pos = GetLastAddedCursor().End;
    std::ranges::sort(Cursors, [](const auto &a, const auto &b) { return a.SelectionStart() < b.SelectionStart(); });
    // Update last added cursor index to be valid after sort.
    for (auto &c : Cursors) {
        if (c.End == last_added_cursor_pos) LastAddedCursorIndex = &c - Cursors.data();
    }
}

void TextEditor::UndoRecord::Undo(TextEditor *editor) {
    for (const auto &op : reverse_view(Operations)) {
        if (op.Text.empty()) continue;

        switch (op.Type) {
            case UndoOperationType::Delete: {
                editor->InsertTextAt(op.Start, op.Text);
                editor->Colorize(op.Start.L, op.End.L - op.Start.L + 2);
                break;
            }
            case UndoOperationType::Add: {
                editor->DeleteRange(op.Start, op.End);
                editor->Colorize(op.Start.L, op.End.L - op.Start.L + 2);
                break;
            }
        }
    }

    editor->State = Before;
    editor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *editor) {
    for (const auto &op : Operations) {
        if (op.Text.empty()) continue;

        switch (op.Type) {
            case UndoOperationType::Delete: {
                editor->DeleteRange(op.Start, op.End);
                editor->Colorize(op.Start.L, op.End.L - op.Start.L + 1);
                break;
            }
            case UndoOperationType::Add: {
                editor->InsertTextAt(op.Start, op.Text);
                editor->Colorize(op.Start.L, op.End.L - op.Start.L + 1);
                break;
            }
        }
    }

    editor->State = After;
    editor->EnsureCursorVisible();
}

string TextEditor::GetText() const {
    return Lines.empty() ? "" : GetText({}, {uint(Lines.size() - 1), GetLineMaxColumn(Lines.size() - 1)});
}

string TextEditor::GetSelectedText(const Cursor &c) const { return GetText(c.SelectionStart(), c.SelectionEnd()); }

void TextEditor::SetCursorPosition(const Coordinates &position, Cursor &c, bool clear_selection) {
    CursorPositionChanged = true;

    if (clear_selection) c.Start = position;

    if (c.End != position) {
        c.End = position;
        EnsureCursorVisible();
    }
}

void TextEditor::InsertTextAtCursor(const string &text, Cursor &c) {
    if (text.empty()) return;

    const auto pos = SanitizeCoordinates(c.End);
    const auto start = std::min(pos, c.SelectionStart());
    const auto insertion_end = InsertTextAt(pos, text);
    SetCursorPosition(insertion_end, c);
    Colorize(start.L, insertion_end.L - start.L + std::ranges::count(text, '\n') + 2);
}

// Assumes given char index is not in the middle of a UTF8 sequence.
// Char index can be equal to line length.
bool TextEditor::Move(uint &li, uint &ci, bool left, bool lock_line) const {
    if (li >= Lines.size()) return false;

    if (left) {
        if (ci == 0) {
            if (lock_line || li == 0) return false;

            li--;
            ci = Lines[li].size();
        } else {
            ci--;
            while (ci > 0 && IsUTFSequence(Lines[li][ci].Char)) ci--;
        }
    } else { // right
        if (ci == Lines[li].size()) {
            if (lock_line || li == Lines.size() - 1) return false;

            li++;
            ci = 0;
        } else {
            ci = std::min(ci + UTF8CharLength(Lines[li][ci].Char), uint(Lines[li].size()));
        }
    }
    return true;
}

static uint NextTabstop(uint column, uint tab_size) { return ((column / tab_size) + 1) * tab_size; }

void TextEditor::MoveCharIndexAndColumn(uint line, uint &ci, uint &column) const {
    const char ch = Lines[line][ci].Char;
    ci += UTF8CharLength(ch);
    column = ch == '\t' ? NextTabstop(column, TabSize) : column + 1;
}

void TextEditor::MoveCoords(Coordinates &coords, MoveDirection direction, bool is_word_mode, uint line_count) const {
    uint ci = GetCharIndexR(coords), li = coords.L;
    switch (direction) {
        case MoveDirection::Right:
            if (ci >= Lines[li].size()) {
                if (li < Lines.size() - 1) {
                    coords.L = std::clamp(li + 1, 0u, uint(Lines.size() - 1));
                    coords.C = 0;
                }
            } else {
                Move(li, ci);
                const uint one_step_right_column = GetCharColumn(li, ci);
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
                if (li > 0) {
                    coords.L = li - 1;
                    coords.C = GetLineMaxColumn(coords.L);
                }
            } else {
                Move(li, ci, true);
                coords.C = GetCharColumn(li, ci);
                if (is_word_mode) coords = FindWordStart(coords);
            }
            break;
        case MoveDirection::Up:
            coords.L = std::max(0u, li - line_count);
            break;
        case MoveDirection::Down:
            coords.L = std::clamp(li + line_count, 0u, uint(Lines.size() - 1));
            break;
    }
}

void TextEditor::MoveUp(uint amount, bool select) {
    for (auto &c : State.Cursors) {
        auto new_coords = c.End;
        MoveCoords(new_coords, MoveDirection::Up, false, amount);
        SetCursorPosition(new_coords, c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveDown(uint amount, bool select) {
    for (auto &c : State.Cursors) {
        assert(c.End.C >= 0);
        auto new_coords = c.End;
        MoveCoords(new_coords, MoveDirection::Down, false, amount);
        SetCursorPosition(new_coords, c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveLeft(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    for (auto &c : State.Cursors) {
        if (AnyCursorHasSelection() && !select && !is_word_mode) {
            SetCursorPosition(c.SelectionStart(), c);
        } else {
            auto new_coords = c.End;
            MoveCoords(new_coords, MoveDirection::Left, is_word_mode);
            SetCursorPosition(new_coords, c, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    for (auto &c : State.Cursors) {
        if (AnyCursorHasSelection() && !select && !is_word_mode) {
            SetCursorPosition(c.SelectionEnd(), c);
        } else {
            auto new_coords = c.End;
            MoveCoords(new_coords, MoveDirection::Right, is_word_mode);
            SetCursorPosition(new_coords, c, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool select) {
    SetCursorPosition({0, 0}, State.GetCursor(), !select);
}

void TextEditor::TextEditor::MoveBottom(bool select) {
    const uint max_line = Lines.size() - 1;
    SetCursorPosition({max_line, GetLineMaxColumn(max_line)}, State.GetCursor(), !select);
}

void TextEditor::MoveHome(bool select) {
    for (auto &c : State.Cursors) SetCursorPosition({c.End.L, 0}, c, !select);
}

void TextEditor::MoveEnd(bool select) {
    for (auto &c : State.Cursors) SetCursorPosition({c.End.L, GetLineMaxColumn(c.End.L)}, c, !select);
}

// todo can we simplify this by using `InsertTextAt...`?
void TextEditor::EnterChar(ImWchar ch, bool is_shift) {
    if (ch == '\t' && AnyCursorHasMultilineSelection()) return ChangeCurrentLinesIndentation(!is_shift);

    UndoRecord u{State};
    for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);

    std::vector<Coordinates> coords;
    coords.reserve(State.Cursors.size());
    // Order is important here for typing '\n' in the same line at the same time.
    for (auto &c : reverse_view(State.Cursors)) {
        const auto coord = SanitizeCoordinates(c.End);
        coords.push_back(coord);
        UndoOperation added;
        added.Type = UndoOperationType::Add;
        added.Start = coord;

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
            AddGlyphs(coord.L + 1, new_line.size(), {line.cbegin() + ci, line.cend()});
            RemoveGlyphs(coord.L, ci);
            SetCursorPosition(LineCharToCoordinates(coord.L + 1, whitespace_size), c);
        } else {
            char buf[5];
            ImTextCharToUtf8(buf, ch);

            const auto &line = Lines[coord.L];
            const auto ci = GetCharIndexR(coord);
            if (Overwrite && ci < line.size()) {
                uint d = UTF8CharLength(line[ci].Char);

                UndoOperation removed;
                removed.Type = UndoOperationType::Delete;
                removed.Start = c.End;
                removed.End = LineCharToCoordinates(coord.L, ci + d);
                while (d-- > 0 && ci < line.size()) {
                    removed.Text += line[ci].Char;
                    RemoveGlyphs(coord.L, ci, ci + 1);
                }
                u.Operations.push_back(removed);
            }
            std::vector<Glyph> glyphs;
            for (auto p = buf; *p != '\0'; p++) glyphs.emplace_back(*p, PaletteIndex::Default);
            AddGlyphs(coord.L, ci, glyphs);
            added.Text = buf;

            SetCursorPosition(LineCharToCoordinates(coord.L, ci + glyphs.size()), c);
        }

        added.End = SanitizeCoordinates(c.End);
        u.Operations.push_back(added);
    }

    AddUndo(u);

    for (const auto &coord : coords) Colorize(coord.L, 3);
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
        for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);
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

void TextEditor::SetSelection(Coordinates start, Coordinates end, Cursor &c) {
    const uint max_line = Lines.size() - 1;
    const Coordinates min_coords{0, 0}, max_coords{max_line, GetLineMaxColumn(max_line)};
    c.Start = std::clamp(start, min_coords, max_coords);
    SetCursorPosition(std::clamp(end, min_coords, max_coords), c, false);
}

void TextEditor::AddCursorForNextOccurrence(bool case_sensitive) {
    const auto &c = State.GetLastAddedCursor();
    if (const auto match_range = FindNextOccurrence(GetSelectedText(c), c.SelectionEnd(), case_sensitive)) {
        State.AddCursor();
        SetSelection(match_range->Start, match_range->End, State.GetCursor());
        SortAndMergeCursors();
        EnsureCursorVisible(true);
    }
}

static char ToLower(char ch, bool case_sensitive) { return (!case_sensitive && ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch; }

std::optional<TextEditor::Cursor> TextEditor::FindNextOccurrence(const string &text, const Coordinates &from, bool case_sensitive) {
    if (text.empty()) return {};

    uint f_li, if_li;
    if_li = f_li = from.L;

    uint f_i, if_i;
    if_i = f_i = GetCharIndexR(from);

    do {
        /* Match */
        uint line_offset = 0;
        uint ci_inner = f_i, i = 0;
        for (; i < text.size(); i++) {
            if (ci_inner == Lines[f_li + line_offset].size()) {
                if (text[i] != '\n' || f_li + line_offset + 1 >= Lines.size()) break;

                ci_inner = 0;
                line_offset++;
            } else {
                const char ch_a = ToLower(Lines[f_li + line_offset][ci_inner].Char, case_sensitive);
                const char ch_b = ToLower(text[i], case_sensitive);
                if (ch_a != ch_b) break;

                ci_inner++;
            }
        }
        if (i == text.size()) return Cursor{LineCharToCoordinates(f_li, f_i), LineCharToCoordinates(f_li + line_offset, ci_inner)};

        /* Move forward */
        if (f_i == Lines[f_li].size()) {
            f_li = f_li == Lines.size() - 1 ? 0 : f_li + 1;
            f_i = 0;
        } else {
            f_i++;
        }
    } while (f_i != if_i || f_li != if_li);

    return {};
}

bool TextEditor::FindMatchingBracket(uint li, uint ci, Coordinates &out) {
    static const std::unordered_map<char, char>
        OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
        CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

    if (li >= Lines.size()) return false;

    const auto &line = Lines[li];
    if (ci >= line.size()) return false;

    uint li_inner = li, ci_inner = ci;
    uint counter = 1;
    if (CloseToOpenChar.contains(line[ci].Char)) {
        const char close_char = line[ci].Char, open_char = CloseToOpenChar.at(close_char);
        while (Move(li_inner, ci_inner, true)) {
            if (ci_inner < Lines[li_inner].size()) {
                const char ch = Lines[li_inner][ci_inner].Char;
                if (ch == open_char) {
                    counter--;
                    if (counter == 0) {
                        out = LineCharToCoordinates(li_inner, ci_inner);
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
            if (ci_inner < Lines[li_inner].size()) {
                const char ch = Lines[li_inner][ci_inner].Char;
                if (ch == close_char) {
                    counter--;
                    if (counter == 0) {
                        out = LineCharToCoordinates(li_inner, ci_inner);
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
    for (const auto &c : reverse_view(State.Cursors)) {
        for (uint li = c.SelectionStart().L; li <= c.SelectionEnd().L; li++) {
            // Check if selection ends at line start.
            if (c.HasSelection() && c.SelectionEnd() == Coordinates{li, 0}) continue;

            const auto &line = Lines[li];
            if (increase) {
                if (!line.empty()) {
                    const Coordinates line_start{li, 0};
                    const auto insertion_end = InsertTextAt(line_start, "\t");
                    u.Operations.emplace_back("\t", line_start, insertion_end, UndoOperationType::Add);
                    Colorize(line_start.L, 1);
                }
            } else {
                Coordinates start{li, 0}, end{li, TabSize};
                int ci = int(GetCharIndexL(end)) - 1;
                while (ci > -1 && (line[ci].Char == ' ' || line[ci].Char == '\t')) ci--;
                const bool only_space_chars_found = ci == -1;
                if (only_space_chars_found) {
                    u.Operations.emplace_back(GetText(start, end), start, end, UndoOperationType::Delete);
                    DeleteRange(start, end);
                    Colorize(li, 1);
                }
            }
        }
    }

    AddUndo(u);
}

void TextEditor::MoveCurrentLines(bool up) {
    UndoRecord u{State};
    std::set<uint> affected_lines;
    uint min_li = std::numeric_limits<uint>::max(), max_li = std::numeric_limits<uint>::min();
    for (const auto &c : State.Cursors) {
        for (uint li = c.SelectionStart().L; li <= c.SelectionEnd().L; li++) {
            // Check if selection ends at line start.
            if (c.HasSelection() && c.SelectionEnd() == Coordinates{li, 0}) continue;

            affected_lines.insert(li);
            min_li = std::min(li, min_li);
            max_li = std::max(li, max_li);
        }
    }
    if ((up && min_li == 0) || (!up && max_li == Lines.size() - 1)) return; // Can't move up/down anymore.

    const uint start_li = min_li - (up ? 1 : 0), end_li = max_li + (up ? 0 : 1);
    const Coordinates start{start_li, 0}, end{end_li, GetLineMaxColumn(end_li)};
    AddUndoOp(u, UndoOperationType::Delete, start, end);
    if (up) {
        for (const uint li : affected_lines) std::swap(Lines[li - 1], Lines[li]);
    } else {
        for (auto it = affected_lines.rbegin(); it != affected_lines.rend(); it++) std::swap(Lines[*it + 1], Lines[*it]);
    }
    for (auto &c : State.Cursors) {
        c.Start.L += (up ? -1 : 1);
        c.End.L += (up ? -1 : 1);
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
    std::unordered_set<uint> affected_line_indices;
    for (const auto &c : reverse_view(State.Cursors)) {
        for (uint li = c.SelectionStart().L; li <= c.SelectionEnd().L; li++) {
            // Check if selection ends at line start.
            if (c.HasSelection() && c.SelectionEnd() == Coordinates{li, 0}) continue;

            affected_line_indices.insert(li);

            const auto &line = Lines[li];
            uint i = 0;
            while (i < line.size() && (line[i].Char == ' ' || line[i].Char == '\t')) i++;
            if (i == line.size()) continue;

            uint i_inner = 0;
            while (i_inner < comment_str.length() && i + i_inner < line.size() && line[i + i_inner].Char == comment_str[i_inner]) i++;
            const bool matched = i_inner == comment_str.length();
            should_add_comment |= !matched;
        }
    }

    if (should_add_comment) {
        for (uint li : affected_line_indices) {
            const Coordinates line_start{li, 0};
            const Coordinates insertion_end = InsertTextAt(line_start, comment_str + ' ');
            u.Operations.emplace_back(comment_str + ' ', line_start, insertion_end, UndoOperationType::Add);
            Colorize(line_start.L, 1);
        }
    } else {
        for (uint li : affected_line_indices) {
            const auto &line = Lines[li];
            uint ci = 0;
            while (ci < line.size() && (line[ci].Char == ' ' || line[ci].Char == '\t')) ci++;
            if (ci == line.size()) continue;

            uint ci_2 = 0;
            while (ci_2 < comment_str.length() && ci + ci_2 < line.size() && line[ci + ci_2].Char == comment_str[ci_2]) ci_2++;
            const bool matched = ci_2 == comment_str.length();
            assert(matched);
            if (ci + ci_2 < line.size() && line[ci + ci_2].Char == ' ') ci_2++;

            const Coordinates start = LineCharToCoordinates(li, ci), end = LineCharToCoordinates(li, ci + ci_2);
            AddUndoOp(u, UndoOperationType::Delete, start, end);
            DeleteRange(start, end);
            Colorize(li, 1);
        }
    }
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u{State};
    for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);
    MoveHome();
    OnCursorPositionChanged(); // Might combine cursors.

    for (auto &c : reverse_view(State.Cursors)) {
        const uint li = c.End.L;
        Coordinates to_delete_start, to_delete_end;
        if (Lines.size() > li + 1) { // Next line exists.
            to_delete_start = {li, 0};
            to_delete_end = {li + 1, 0};
            SetCursorPosition({li, 0}, c);
        } else if (li > 0) { // Previous line exists.
            to_delete_start = {li - 1, GetLineMaxColumn(li - 1)};
            to_delete_end = {li, GetLineMaxColumn(li)};
            SetCursorPosition({li - 1, 0}, c);
        } else {
            to_delete_start = {li, 0};
            to_delete_end = {li, GetLineMaxColumn(li)};
            SetCursorPosition({li, 0}, c);
        }

        AddUndoOp(u, UndoOperationType::Delete, to_delete_start, to_delete_end);
        if (to_delete_start.L != to_delete_end.L) {
            Lines.erase(Lines.begin() + li);
            for (const auto &other_c : State.Cursors) {
                if (other_c == c) continue;
                if (other_c.End.L >= li) SetCursorPosition({other_c.End.L - 1, other_c.End.C}, c);
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
    if (coords.L >= Lines.size()) {
        if (Lines.empty()) return {0, 0};

        const uint li = Lines.size() - 1;
        return {li, GetLineMaxColumn(li)};
    }
    return {coords.L, Lines.empty() ? 0 : GetLineMaxColumn(coords.L, coords.C)};
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &screen_pos, bool insertion_mode, bool *is_over_li) const {
    static constexpr float PosToCoordsColumnOffset = 0.33;

    const auto local = ImVec2{screen_pos.x + 3.0f, screen_pos.y} - ImGui::GetCursorScreenPos();
    if (is_over_li != nullptr) *is_over_li = local.x < TextStart;

    Coordinates out{
        std::max(0u, uint(floor(local.y / CharAdvance.y))),
        std::max(0u, uint(floor((local.x - TextStart) / CharAdvance.x)))
    };
    const uint ci = GetCharIndexL(out);
    if (out.L < Lines.size() && ci < Lines[out.L].size() && Lines[out.L][ci].Char == '\t') {
        const uint column_to_left = GetCharColumn(out.L, ci), column_to_right = GetCharColumn(out.L, GetCharIndexR(out));
        out.C = out.C - column_to_left < column_to_right - out.C ? column_to_left : column_to_right;
    } else {
        out.C = std::max(0u, uint(floor((local.x - TextStart + PosToCoordsColumnOffset * CharAdvance.x) / CharAdvance.x)));
    }
    return SanitizeCoordinates(out);
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &from) const {
    if (from.L >= Lines.size()) return from;

    const auto &line = Lines[from.L];
    uint ci = GetCharIndexL(from);
    if (ci >= line.size()) return from;

    const bool initial_is_word_char = IsWordChar(line[ci].Char);
    const bool initial_is_space = isspace(line[ci].Char);
    const char initial_char = line[ci].Char;
    uint li = from.L; // Not modified.
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
    return LineCharToCoordinates(li, ci);
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &from) const {
    if (from.L >= Lines.size()) return from;

    const auto &line = Lines[from.L];
    uint ci = GetCharIndexL(from);
    if (ci >= line.size()) return from;

    const bool initial_is_word_char = IsWordChar(line[ci].Char);
    const bool initial_is_space = isspace(line[ci].Char);
    const char initial_char = line[ci].Char;
    uint li = from.L; // Not modified.
    while (Move(li, ci, false, true)) {
        if (ci == line.size()) break;

        const bool is_word_char = IsWordChar(line[ci].Char);
        const bool is_space = isspace(line[ci].Char);
        if ((initial_is_space && !is_space) ||
            (initial_is_word_char && !is_word_char) ||
            (!initial_is_word_char && !initial_is_space && initial_char != line[ci].Char))
            break;
    }
    return LineCharToCoordinates(li, ci);
}

uint TextEditor::GetCharIndexL(const Coordinates &coords) const {
    const uint li = std::min(coords.L, uint(Lines.size() - 1));
    const auto &line = Lines[li];
    uint i = 0, tab_coords_left = 0;
    for (uint column = 0; column < coords.C && i < line.size(); column++) {
        if (line[i].Char == '\t') {
            if (tab_coords_left == 0) tab_coords_left = TabSizeAtColumn(column);
            else tab_coords_left--;
        }
        if (tab_coords_left == 0) i += UTF8CharLength(line[i].Char);
    }
    return i;
}

uint TextEditor::GetCharIndexR(const Coordinates &coords) const {
    const uint li = std::min(coords.L, uint(Lines.size() - 1));
    uint c = 0, i = 0;
    for (; i < Lines[li].size() && c < coords.C;) MoveCharIndexAndColumn(li, i, c);
    return i;
}

uint TextEditor::GetCharColumn(uint li, uint ci) const {
    if (li >= Lines.size()) return 0;

    uint i = 0, c = 0;
    while (i < ci && i < Lines[li].size()) MoveCharIndexAndColumn(li, i, c);
    return c;
}

uint TextEditor::GetFirstVisibleCharIndex(uint li) const {
    if (li >= Lines.size()) return 0;

    uint i = 0, c = 0;
    while (c < FirstVisibleColumn && i < Lines[li].size()) MoveCharIndexAndColumn(li, i, c);
    return c > FirstVisibleColumn && i > 0 ? i - 1 : i;
}

uint TextEditor::GetLineMaxColumn(uint li) const {
    if (li >= Lines.size()) return 0;

    uint column = 0;
    for (uint i = 0; i < Lines[li].size();) MoveCharIndexAndColumn(li, i, column);
    return column;
}
uint TextEditor::GetLineMaxColumn(uint li, uint limit) const {
    if (li >= Lines.size()) return 0;

    uint column = 0;
    for (uint i = 0; i < Lines[li].size();) {
        MoveCharIndexAndColumn(li, i, column);
        if (column > limit) return limit;
    }
    return column;
}

TextEditor::LineT &TextEditor::InsertLine(uint li) {
    auto &result = *Lines.insert(Lines.begin() + li, LineT{});
    for (auto &c : State.Cursors) {
        if (c.End.L >= li) SetCursorPosition({c.End.L + 1, c.End.C}, c);
    }

    return result;
}

void TextEditor::DeleteRange(const Coordinates &start, const Coordinates &end, const Cursor *exclude_cursor) {
    if (end <= start) return;

    const auto start_ci = GetCharIndexL(start), end_ci = GetCharIndexR(end);
    if (start.L == end.L) return RemoveGlyphs(start.L, start_ci, end_ci);

    RemoveGlyphs(start.L, start_ci); // from start to end of line
    RemoveGlyphs(end.L, 0, end_ci);
    if (start.L == end.L) return;

    // At least one line completely removed.
    AddGlyphs(start.L, Lines[start.L].size(), Lines[end.L]);

    // Move up all cursors after the last removed line.
    const uint num_removed_lines = end.L - start.L;
    for (auto &c : State.Cursors) {
        if (exclude_cursor != nullptr && c == *exclude_cursor) continue;

        if (c.End.L >= end.L) {
            c.Start.L -= num_removed_lines;
            c.End.L -= num_removed_lines;
        }
    }

    Lines.erase(Lines.begin() + start.L + 1, Lines.begin() + end.L + 1);
}

void TextEditor::DeleteSelection(Cursor &c, UndoRecord &record) {
    if (!c.HasSelection()) return;

    AddUndoOp(record, UndoOperationType::Delete, c.SelectionStart(), c.SelectionEnd());
    // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
    DeleteRange(c.SelectionStart(), c.SelectionEnd(), &c);
    SetCursorPosition(c.SelectionStart(), c);
    Colorize(c.SelectionStart().L, 1);
}

void TextEditor::AddOrRemoveGlyphs(uint li, uint ci, std::span<const Glyph> glyphs, bool is_add) {
    const uint column = GetCharColumn(li, ci);
    auto &line = Lines[li];

    // New character index for each affected cursor when other cursor writes/deletes are on the same line.
    std::unordered_map<uint, uint> adjusted_ci_for_cursor;
    for (uint c = 0; c < State.Cursors.size(); c++) {
        const auto &cursor = State.Cursors[c];
        // Cursor needs adjusting if it's to the right of a change, without a selection.
        if (cursor.End.L == li && cursor.End.C > column && !cursor.HasSelection()) {
            adjusted_ci_for_cursor[c] = GetCharIndexR({li, cursor.End.C}) + (is_add ? glyphs.size() : -glyphs.size());
        }
    }

    if (is_add) line.insert(line.begin() + ci, glyphs.begin(), glyphs.end());
    else line.erase(glyphs.begin(), glyphs.end());

    for (const auto &[c, ci] : adjusted_ci_for_cursor) SetCursorPosition(LineCharToCoordinates(li, ci), State.Cursors[c]);
}
void TextEditor::AddGlyphs(uint li, uint ci, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(li, ci, glyphs, true); }
void TextEditor::RemoveGlyphs(uint li, uint ci, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(li, ci, glyphs, false); }
void TextEditor::RemoveGlyphs(uint li, uint ci, uint end_ci) { RemoveGlyphs(li, ci, {Lines[li].cbegin() + ci, Lines[li].cbegin() + end_ci}); }
void TextEditor::RemoveGlyphs(uint li, uint ci) { RemoveGlyphs(li, ci, {Lines[li].cbegin() + ci, Lines[li].cend()}); }

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
            for (const auto ch : io.InputQueueCharacters) {
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
                else State.ResetCursors();

                const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                const auto target_cursor_pos = cursor_coords.L < Lines.size() - 1 ?
                    Coordinates{cursor_coords.L + 1, 0} :
                    Coordinates{cursor_coords.L, GetLineMaxColumn(cursor_coords.L)};
                SetSelection({cursor_coords.L, 0}, target_cursor_pos, State.GetCursor());

                LastClickTime = -1.0f;
            } else if (is_double_click) {
                if (ctrl) State.AddCursor();
                else State.ResetCursors();

                const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(FindWordStart(cursor_coords), FindWordEnd(cursor_coords), State.GetCursor());

                LastClickTime = float(ImGui::GetTime());
                LastClickPos = io.MousePos;
            } else if (is_click) {
                if (ctrl) State.AddCursor();
                else State.ResetCursors();

                bool is_over_li;
                const auto cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite, &is_over_li);
                if (is_over_li) {
                    const auto target_cursor_pos = cursor_coords.L < Lines.size() - 1 ?
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

    VisibleLineCount = std::max(uint(ceil(ContentHeight / CharAdvance.y)), 0u);
    FirstVisibleLineI = std::max(uint(scroll_y / CharAdvance.y), 0u);
    LastVisibleLineI = std::max(uint((ContentHeight + scroll_y) / CharAdvance.y), 0u);

    VisibleColumnCount = std::max(uint(ceil((ContentWidth - std::max(TextStart - scroll_x, 0.0f)) / CharAdvance.x)), 0u);
    FirstVisibleColumn = std::max(uint(std::max(scroll_x - TextStart, 0.0f) / CharAdvance.x), 0u);
    LastVisibleColumn = std::max(uint((ContentWidth + scroll_x - TextStart) / CharAdvance.x), 0u);
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
        snprintf(li_buffer, 16, " %lu ", Lines.size());
        TextStart += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, li_buffer, nullptr, nullptr).x;
    }
    const ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
    ScrollX = ImGui::GetScrollX();
    ScrollY = ImGui::GetScrollY();
    UpdateViewVariables(ScrollX, ScrollY);

    uint max_column_limited = 0;
    if (!Lines.empty()) {
        auto dl = ImGui::GetWindowDrawList();
        const float font_size = ImGui::GetFontSize();
        const float space_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, " ").x;

        for (uint li = FirstVisibleLineI; li <= LastVisibleLineI && li < Lines.size(); li++) {
            const auto &line = Lines[li];
            max_column_limited = std::max(GetLineMaxColumn(li, LastVisibleColumn), max_column_limited);

            const ImVec2 line_start_screen_pos{cursor_screen_pos.x, cursor_screen_pos.y + li * CharAdvance.y};
            const float text_screen_pos_x = line_start_screen_pos.x + TextStart;
            const Coordinates line_start_coord{li, 0}, line_end_coord{li, max_column_limited};
            // Draw selection for the current line
            for (const auto &c : State.Cursors) {
                const auto selection_start = c.SelectionStart(), selection_end = c.SelectionEnd();
                float rect_start = -1.0f, rect_end = -1.0f;
                if (selection_start <= line_end_coord)
                    rect_start = selection_start > line_start_coord ? TextDistanceToLineStart(selection_start) : 0.0f;
                if (selection_end > line_start_coord)
                    rect_end = TextDistanceToLineStart(selection_end < line_end_coord ? selection_end : line_end_coord);
                if (selection_end.L > li || (selection_end.L == li && selection_end > line_end_coord))
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
            for (const auto &c : State.Cursors) {
                if (c.End.L == li) cursor_coords_in_this_line.push_back(c.End);
            }
            if (cursor_coords_in_this_line.size() > 0) {
                // Render the cursors
                if (ImGui::IsWindowFocused() || is_parent_focused) {
                    for (const auto &cursor_coords : cursor_coords_in_this_line) {
                        float width = 1.0f;
                        const uint ci = GetCharIndexR(cursor_coords);
                        const float cx = TextDistanceToLineStart(cursor_coords);
                        if (Overwrite && ci < line.size()) {
                            if (line[ci].Char == '\t') {
                                const auto x = (1.0f + std::floor((1.0f + cx) / (TabSize * space_size))) * (TabSize * space_size);
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
            uint ci = GetFirstVisibleCharIndex(li);
            uint column = FirstVisibleColumn; // can be in the middle of tab character
            while (ci < Lines[li].size() && column <= LastVisibleColumn) {
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
    CurrentSpaceHeight = (Lines.size() + std::min(VisibleLineCount - 1, uint(Lines.size()))) * CharAdvance.y;
    CurrentSpaceWidth = std::max((max_column_limited + std::min(VisibleColumnCount - 1, max_column_limited)) * CharAdvance.x, CurrentSpaceWidth);

    ImGui::SetCursorPos({0, 0});
    ImGui::Dummy({CurrentSpaceWidth, CurrentSpaceHeight});
    if (LastEnsureCursorVisible > -1) {
        // First pass for interactive end and second pass for interactive start.
        for (uint i = 0; i < (LastEnsureCursorVisibleStartToo ? 2 : 1); i++) {
            if (i) UpdateViewVariables(ScrollX, ScrollY); // Second pass depends on changes made in first pass
            const auto &cursor = State.GetCursor(LastEnsureCursorVisible);
            const auto target = SanitizeCoordinates(i > 0 ? cursor.Start : cursor.End);
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
    const auto &c = State.Cursors[0];
    CursorOnBracket = State.Cursors.size() == 1 && !c.HasSelection() ?
        FindMatchingBracket(c.End.L, GetCharIndexR(c.End), MatchingBracketCoords) :
        false;

    if (!IsDraggingSelection) SortAndMergeCursors();
}

void TextEditor::SortAndMergeCursors() {
    State.SortCursors();

    std::unordered_set<const Cursor *> delete_cursors;
    if (AnyCursorHasSelection()) {
        // Merge cursors if they overlap.
        for (uint c = 1; c < State.Cursors.size(); c++) {
            const auto &cursor = State.Cursors[c];
            auto &prev_cursor = State.Cursors[c - 1];
            if (prev_cursor.SelectionEnd() >= cursor.SelectionEnd()) {
                delete_cursors.insert(&cursor);
            } else if (prev_cursor.SelectionEnd() > cursor.SelectionStart()) {
                const auto pc_start = prev_cursor.SelectionStart(), pc_end = prev_cursor.SelectionEnd();
                prev_cursor.Start = pc_start;
                prev_cursor.End = pc_end;
                delete_cursors.insert(&cursor);
            }
        }
    } else {
        // Merge cursors if they are at the same position.
        for (uint c = 1; c < State.Cursors.size(); c++) {
            const auto &cursor = State.Cursors[c];
            if (State.Cursors[c - 1].End == cursor.End) delete_cursors.insert(&cursor);
        }
    }
    std::erase_if(State.Cursors, [&delete_cursors](const auto &c) { return delete_cursors.contains(&c); });
}

void TextEditor::AddUndo(UndoRecord &record) {
    if (record.Operations.empty()) return;

    record.After = State;
    UndoBuffer.resize(UndoIndex + 1);
    UndoBuffer.back() = record;
    ++UndoIndex;
}

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML
void TextEditor::Colorize(uint from_li, uint line_count) {
    const uint to_line = std::min(uint(Lines.size()), from_li + line_count);
    ColorRangeMin = std::min(ColorRangeMin, from_li);
    ColorRangeMax = std::max(ColorRangeMax, to_line);
    ColorRangeMin = std::max(0u, ColorRangeMin);
    ColorRangeMax = std::max(ColorRangeMin, ColorRangeMax);
    ShouldCheckComments = true;
}

void TextEditor::ColorizeRange(uint from_li, uint to_li) {
    if (Lines.empty() || from_li >= to_li || LanguageDef == nullptr) return;

    string buffer, id;
    std::cmatch results;
    const uint end_li = std::clamp(to_li, 0u, uint(Lines.size()));
    for (uint i = from_li; i < end_li; ++i) {
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
        uint li = 0, i = 0;
        uint end_i = 0, end_li = Lines.size();
        uint comment_start_li = end_li, comment_start_i = end_i;
        while (li < end_li || i < end_i) {
            auto &line = Lines[li];
            if (i == 0 && !concatenate) {
                within_single_line_comment = false;
                within_preproc = false;
                first_char = true;
            }

            concatenate = false;
            if (line.empty()) {
                i = 0;
                li++;
                continue;
            }

            const auto &g = line[i];
            const auto ch = g.Char;
            if (ch != LanguageDef->PreprocChar && !isspace(ch)) first_char = false;
            if (i == line.size() - 1 && line[line.size() - 1].Char == '\\') concatenate = true;

            bool in_comment = (comment_start_li < li || (comment_start_li == li && comment_start_i <= i));
            if (within_string) {
                line[i].IsMultiLineComment = in_comment;
                if (ch == '\"') {
                    if (i + 1 < line.size() && line[i + 1].Char == '\"') {
                        i += 1;
                        if (i < line.size()) line[i].IsMultiLineComment = in_comment;
                    } else {
                        within_string = false;
                    }
                } else if (ch == '\\') {
                    i += 1;
                    if (i < line.size()) line[i].IsMultiLineComment = in_comment;
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
                    if (i + 1 >= end_str.size() &&
                        ColorizerEquals(end_str.cbegin(), end_str.cend(), from + 1 - end_str.size(), from + 1, pred)) {
                        comment_start_i = end_i;
                        comment_start_li = end_li;
                    }
                }
            }

            if (i < line.size()) line[i].IsPreprocessor = within_preproc;
            i += UTF8CharLength(ch);
            if (i >= line.size()) {
                i = 0;
                li++;
            }
        }
        ShouldCheckComments = false;
    }

    if (ColorRangeMin < ColorRangeMax) {
        const uint increment = (LanguageDef->Tokenize == nullptr) ? 10 : 10000;
        const uint to = std::min(ColorRangeMin + increment, ColorRangeMax);
        ColorizeRange(ColorRangeMin, to);
        ColorRangeMin = to;
        if (ColorRangeMax == ColorRangeMin) {
            ColorRangeMin = std::numeric_limits<uint>::max();
            ColorRangeMax = std::numeric_limits<uint>::min();
        }
        return;
    }
}

TextEditor::Coordinates TextEditor::InsertTextAt(const Coordinates &at, const std::string &text) {
    uint ci = GetCharIndexR(at);
    Coordinates ret = at;
    for (auto it = text.begin(); it != text.end(); it++) {
        const char ch = *it;
        if (ch == '\r') continue;

        if (ch == '\n') {
            if (ci < Lines[ret.L].size()) {
                InsertLine(ret.L + 1);
                const auto &line = Lines[ret.L];
                AddGlyphs(ret.L + 1, 0, {line.cbegin() + ci, line.cend()});
                RemoveGlyphs(ret.L, ci);
            } else {
                InsertLine(ret.L + 1);
            }
            ci = 0;
            ret.L++;
            ret.C = 0;
        } else {
            std::vector<Glyph> glyphs;
            for (uint d = 0; d < UTF8CharLength(ch) && it != text.end(); d++) {
                glyphs.emplace_back(*it, PaletteIndex::Default);
                if (d > 0) it++;
            }
            AddGlyphs(ret.L, ci, glyphs);
            ci += glyphs.size();
            ret.C = GetCharColumn(ret.L, ci);
        }
    }

    return ret;
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
