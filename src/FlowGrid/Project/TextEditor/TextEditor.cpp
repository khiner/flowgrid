#include "TextEditor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <regex>
#include <string>

#include "imgui_internal.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!p(*first1, *first2)) return false;
    }
    return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor()
    : LineSpacing(1.0f), LongestLineLength(20.0f), UndoIndex(0), TabSize(4), Overwrite(false), ReadOnly(false), AutoIndent(true), WithinRender(false), ScrollToCursor(false), ScrollToTop(false), ColorizerEnabled(true), TextStart(20.0f), LeftMargin(10), ColorRangeMin(0), ColorRangeMax(0), ShouldCheckComments(true), ShowWhitespaces(true), StartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), LastClickTime(-1.0f) {
    SetPalette(GetDarkPalette());
    Lines.push_back(LineT());
}

TextEditor::~TextEditor() {}

void TextEditor::SetLanguageDefinition(const LanguageDefT &language_def) {
    LanguageDef = &language_def;
    RegexList.clear();

    for (const auto &r : LanguageDef->TokenRegexStrings) {
        RegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));
    }

    Colorize();
}

const char *TextEditor::GetLanguageDefinitionName() const {
    return LanguageDef != nullptr ? LanguageDef->Name.c_str() : "unknown";
}

void TextEditor::SetPalette(const PaletteT &palette) {
    PalletteBase = palette;
}

string TextEditor::GetText(const Coordinates &start, const Coordinates &end) const {
    assert(end >= start);
    if (end == start) return "";

    string result;

    auto line_start = start.Line;
    auto line_end = end.Line;
    auto start_char_index = GetCharacterIndexR(start);
    auto end_char_index = GetCharacterIndexR(end);

    size_t s = 0;
    for (size_t i = line_start; i < line_end; i++) s += Lines[i].size();

    result.reserve(s + s / 8);

    while (start_char_index < end_char_index || line_start < line_end) {
        if (line_start >= (int)Lines.size()) break;

        auto &line = Lines[line_start];
        if (start_char_index < (int)line.size()) {
            result += line[start_char_index].Char;
            start_char_index++;
        } else {
            start_char_index = 0;
            ++line_start;
            result += '\n';
        }
    }

    return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates(int cursor) const {
    return SanitizeCoordinates(State.Cursors[cursor == -1 ? State.CurrentCursor : cursor].InteractiveEnd);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates &coords) const {
    auto line = coords.Line;
    auto column = coords.Column;
    if (line >= (int)Lines.size()) {
        if (Lines.empty()) {
            line = 0;
            column = 0;
        } else {
            line = (int)Lines.size() - 1;
            column = GetLineMaxColumn(line);
        }
        return Coordinates(line, column);
    } else {
        column = Lines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
        return Coordinates(line, column);
    }
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::CharT c) {
    if ((c & 0xFE) == 0xFC) return 6;
    if ((c & 0xFC) == 0xF8) return 5;
    if ((c & 0xF8) == 0xF0) return 4;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xE0) == 0xC0) return 2;
    return 1;
}

// "Borrowed" from ImGui source
static inline int ITextCharToUtf8(char *buf, int buf_size, unsigned int c) {
    if (c < 0x80) {
        buf[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        if (buf_size < 2) return 0;
        buf[0] = (char)(0xc0 + (c >> 6));
        buf[1] = (char)(0x80 + (c & 0x3f));
        return 2;
    }
    if (c >= 0xdc00 && c < 0xe000) {
        return 0;
    }
    if (c >= 0xd800 && c < 0xdc00) {
        if (buf_size < 4) return 0;
        buf[0] = (char)(0xf0 + (c >> 18));
        buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[3] = (char)(0x80 + ((c)&0x3f));
        return 4;
    }
    // else if (c < 0x10000)
    {
        if (buf_size < 3) return 0;
        buf[0] = (char)(0xe0 + (c >> 12));
        buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 + ((c)&0x3f));
        return 3;
    }
}

void TextEditor::DeleteRange(const Coordinates &start, const Coordinates &end) {
    assert(end >= start);
    assert(!ReadOnly);
    if (end == start) return;

    auto start_char_index = GetCharacterIndexL(start);
    auto end_char_index = GetCharacterIndexR(end);
    if (start.Line == end.Line) {
        auto n = GetLineMaxColumn(start.Line);
        if (end.Column >= n) RemoveGlyphsFromLine(start.Line, start_char_index); // from start to end of line
        else RemoveGlyphsFromLine(start.Line, start_char_index, end_char_index);
    } else {
        RemoveGlyphsFromLine(start.Line, start_char_index); // from start to end of line
        RemoveGlyphsFromLine(end.Line, 0, end_char_index);
        auto &first_line = Lines[start.Line];
        auto &last_line = Lines[end.Line];
        if (start.Line < end.Line) {
            AddGlyphsToLine(start.Line, first_line.size(), last_line.begin(), last_line.end());

            // Move up cursors in line that is being moved up.
            for (int c = 0; c <= State.CurrentCursor; c++) {
                if (State.Cursors[c].InteractiveEnd.Line > end.Line) break;
                if (State.Cursors[c].InteractiveEnd.Line != end.Line) continue;

                int other_cursor_start_char_index = GetCharacterIndexR(State.Cursors[c].InteractiveStart);
                int other_cursor_end_char_index = GetCharacterIndexR(State.Cursors[c].InteractiveEnd);
                int other_cursor_new_start_char_index = GetCharacterIndexR(start) + other_cursor_start_char_index;
                int other_cursor_new_end_char_index = GetCharacterIndexR(start) + other_cursor_end_char_index;
                auto target_start_coords = Coordinates(start.Line, GetCharacterColumn(start.Line, other_cursor_new_start_char_index));
                auto target_end_coords = Coordinates(start.Line, GetCharacterColumn(start.Line, other_cursor_new_end_char_index));
                SetCursorPosition(target_start_coords, c, true);
                SetCursorPosition(target_end_coords, c, false);
            }
            RemoveLines(start.Line + 1, end.Line + 1);
        }
    }
}

int TextEditor::InsertTextAt(/* inout */ Coordinates &at, const char *text) {
    assert(!ReadOnly);

    int cindex = GetCharacterIndexR(at);
    int total_lines = 0;
    while (*text != '\0') {
        assert(!Lines.empty());
        if (*text == '\r') {
            ++text; // skip
        } else if (*text == '\n') {
            if (cindex < (int)Lines[at.Line].size()) {
                InsertLine(at.Line + 1);
                auto &line = Lines[at.Line];
                AddGlyphsToLine(at.Line + 1, 0, line.begin() + cindex, line.end());
                RemoveGlyphsFromLine(at.Line, cindex);
            } else {
                InsertLine(at.Line + 1);
            }
            ++at.Line;
            at.Column = 0;
            cindex = 0;
            ++total_lines;
            ++text;
        } else {
            auto d = UTF8CharLength(*text);
            while (d-- > 0 && *text != '\0')
                AddGlyphToLine(at.Line, cindex++, Glyph(*text++, PaletteIndexT::Default));
            at.Column = GetCharacterColumn(at.Line, cindex);
        }
    }

    return total_lines;
}

void TextEditor::AddUndo(UndoRecord &record) {
    assert(!ReadOnly);
    UndoBuffer.resize((size_t)(UndoIndex + 1));
    UndoBuffer.back() = record;
    ++UndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &position, bool is_insertion_mode, bool *is_over_line_number) const {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(position.x - origin.x + 3.0f, position.y - origin.y);

    if (is_over_line_number != nullptr) *is_over_line_number = local.x < TextStart;

    float space_size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
    int line_number = std::max(0, (int)floor(local.y / CharAdvance.y));
    int column_coord = 0;
    if (line_number >= 0 && line_number < (int)Lines.size()) {
        auto &line = Lines.at(line_number);
        float column_x = 0.0f;

        // First we find the hovered column coord.
        for (size_t column_index = 0; column_index < line.size(); ++column_index) {
            float column_width = 0.0f;
            int delta = 0;
            if (line[column_index].Char == '\t') {
                float old_x = column_x;
                column_x = (1.0f + std::floor((1.0f + column_x) / (float(TabSize) * space_size))) * (float(TabSize) * space_size);
                column_width = column_x - old_x;
                delta = TabSize - (column_coord % TabSize);
            } else {
                char buf[7];
                auto d = UTF8CharLength(line[column_index].Char);
                int i = 0;
                while (i < 6 && d-- > 0)
                    buf[i++] = line[column_index].Char;
                buf[i] = '\0';
                column_width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
                column_x += column_width;
                delta = 1;
            }

            if (TextStart + column_x - (is_insertion_mode ? 0.5f : 0.0f) * column_width < local.x) {
                column_coord += delta;
            } else {
                break;
            }
        }

        // Then we reduce by 1 column coord if cursor is on the left side of the hovered column.
        // if (is_insertion_mode && TextStart + column_x - column_width * 2.0f < local.x)
        //	column_index = std::min((int)line.size() - 1, column_index + 1);
    }

    return SanitizeCoordinates(Coordinates(line_number, column_coord));
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &from) const {
    Coordinates at = from;
    if (at.Line >= (int)Lines.size()) return at;

    auto &line = Lines[at.Line];
    auto cindex = GetCharacterIndexL(at);
    if (cindex >= (int)line.size()) return at;

    bool initial_is_word_char = IsGlyphWordChar(line[cindex]);
    bool initial_is_space = isspace(line[cindex].Char);
    uint8_t initial_char = line[cindex].Char;
    bool need_to_advance = false;
    while (true) {
        --cindex;
        if (cindex < 0) {
            cindex = 0;
            break;
        }

        auto c = line[cindex].Char;
        if (!IsUTFSequence(c)) {
            bool is_word_char = IsGlyphWordChar(line[cindex]);
            bool is_space = isspace(line[cindex].Char);
            if ((initial_is_space && !is_space) || (initial_is_word_char && !is_word_char) || (!initial_is_word_char && !initial_is_space && initial_char != line[cindex].Char)) {
                need_to_advance = true;
                break;
            }
        }
    }
    at.Column = GetCharacterColumn(at.Line, cindex);
    if (need_to_advance) MoveCoords(at, MoveDirection::Right);

    return at;
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &from) const {
    Coordinates at = from;
    if (at.Line >= (int)Lines.size()) return at;

    auto &line = Lines[at.Line];
    auto cindex = GetCharacterIndexL(at);
    if (cindex >= (int)line.size()) return at;

    bool initial_is_word_char = IsGlyphWordChar(line[cindex]);
    bool initial_is_space = isspace(line[cindex].Char);
    uint8_t initial_char = line[cindex].Char;
    while (true) {
        auto d = UTF8CharLength(line[cindex].Char);
        cindex += d;
        if (cindex >= (int)line.size()) break;

        bool is_word_char = IsGlyphWordChar(line[cindex]);
        bool is_space = isspace(line[cindex].Char);
        if ((initial_is_space && !is_space) || (initial_is_word_char && !is_word_char) || (!initial_is_word_char && !initial_is_space && initial_char != line[cindex].Char)) break;
    }
    at.Column = GetCharacterColumn(at.Line, cindex);
    return at;
}

int TextEditor::GetCharacterIndexL(const Coordinates &coords) const {
    if (coords.Line >= Lines.size()) return -1;

    auto &line = Lines[coords.Line];
    int c = 0;
    int i = 0;
    int tab_coords_left = 0;
    for (; i < line.size() && c < coords.Column;) {
        if (line[i].Char == '\t') {
            if (tab_coords_left == 0) tab_coords_left = TabSize - (c % TabSize);
            if (tab_coords_left > 0) tab_coords_left--;
        }
        c++;
        if (tab_coords_left == 0) i += UTF8CharLength(line[i].Char);
    }
    return i;
}

int TextEditor::GetCharacterIndexR(const Coordinates &coords) const {
    if (coords.Line >= Lines.size()) return -1;

    auto &line = Lines[coords.Line];
    int c = 0;
    int i = 0;
    for (; i < line.size() && c < coords.Column;) {
        if (line[i].Char == '\t') c = (c / TabSize) * TabSize + TabSize;
        else ++c;
        i += UTF8CharLength(line[i].Char);
    }
    return i;
}

int TextEditor::GetCharacterColumn(int line_number, int char_index) const {
    if (line_number >= Lines.size()) return 0;

    auto &line = Lines[line_number];
    int col = 0;
    int i = 0;
    while (i < char_index && i < (int)line.size()) {
        auto c = line[i].Char;
        i += UTF8CharLength(c);
        if (c == '\t') col = (col / TabSize) * TabSize + TabSize;
        else col++;
    }
    return col;
}

int TextEditor::GetLineMaxColumn(int line_number) const {
    if (line_number >= Lines.size()) return 0;

    auto &line = Lines[line_number];
    int col = 0;
    for (unsigned i = 0; i < line.size();) {
        auto c = line[i].Char;
        if (c == '\t') col = (col / TabSize) * TabSize + TabSize;
        else col++;
        i += UTF8CharLength(c);
    }
    return col;
}

void TextEditor::RemoveLines(int start, int end) {
    assert(!ReadOnly);
    assert(end >= start);
    assert(Lines.size() > (size_t)(end - start));

    Lines.erase(Lines.begin() + start, Lines.begin() + end);
    assert(!Lines.empty());

    OnLinesDeleted(start, end);
}

void TextEditor::RemoveLine(int line_number, const std::unordered_set<int> *handled_cursors) {
    assert(!ReadOnly);
    assert(Lines.size() > 1);

    Lines.erase(Lines.begin() + line_number);
    assert(!Lines.empty());

    OnLineDeleted(line_number, handled_cursors);
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u;
    u.Before = State;

    if (AnyCursorHasSelection()) {
        for (int c = State.CurrentCursor; c > -1; c--) {
            if (State.Cursors[c].HasSelection()) {
                u.Operations.push_back({GetSelectedText(c), State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd(), UndoOperationType::Delete});
                DeleteSelection(c);
            }
        }
    }
    MoveHome();
    OnCursorPositionChanged(); // Might combine cursors.

    for (int c = State.CurrentCursor; c > -1; c--) {
        int current_line = State.Cursors[c].InteractiveEnd.Line;
        int next_line = current_line + 1;
        int prev_line = current_line - 1;

        Coordinates to_delete_start, to_delete_end;
        if (Lines.size() > next_line) // next line exists
        {
            to_delete_start = Coordinates(current_line, 0);
            to_delete_end = Coordinates(next_line, 0);
            SetCursorPosition({State.Cursors[c].InteractiveEnd.Line, 0}, c);
        } else if (prev_line > -1) // previous line exists
        {
            to_delete_start = Coordinates(prev_line, GetLineMaxColumn(prev_line));
            to_delete_end = Coordinates(current_line, GetLineMaxColumn(current_line));
            SetCursorPosition({prev_line, 0}, c);
        } else {
            to_delete_start = Coordinates(current_line, 0);
            to_delete_end = Coordinates(current_line, GetLineMaxColumn(current_line));
            SetCursorPosition({current_line, 0}, c);
        }

        u.Operations.push_back({GetText(to_delete_start, to_delete_end), to_delete_start, to_delete_end, UndoOperationType::Delete});

        std::unordered_set<int> handled_cursors = {c};
        if (to_delete_start.Line != to_delete_end.Line) RemoveLine(current_line, &handled_cursors);
        else DeleteRange(to_delete_start, to_delete_end);
    }

    u.After = State;
    AddUndo(u);
}

// Adjusts cursor position when other cursor writes/deletes in the same line.
void TextEditor::OnLineChanged(bool before_change, int line_number, int column, int char_count, bool is_deleted) {
    static std::unordered_map<int, int> cursor_char_indices;
    if (before_change) {
        cursor_char_indices.clear();
        for (int c = 0; c <= State.CurrentCursor; c++) {
            if (State.Cursors[c].InteractiveEnd.Line == line_number && // Cursor is at the line.
                State.Cursors[c].InteractiveEnd.Column > column && // Cursor is to the right of changing part.
                State.Cursors[c].GetSelectionEnd() == State.Cursors[c].GetSelectionStart()) // Cursor does not have a selection.
            {
                cursor_char_indices[c] = GetCharacterIndexR({line_number, State.Cursors[c].InteractiveEnd.Column});
                cursor_char_indices[c] += is_deleted ? -char_count : char_count;
            }
        }
    } else {
        for (auto &item : cursor_char_indices) {
            SetCursorPosition({line_number, GetCharacterColumn(line_number, item.second)}, item.first);
        }
    }
}

void TextEditor::RemoveGlyphsFromLine(int line_number, int start_char_index, int end_char_index) {
    int column = GetCharacterColumn(line_number, start_char_index);
    auto &line = Lines[line_number];
    OnLineChanged(true, line_number, column, end_char_index - start_char_index, true);
    line.erase(line.begin() + start_char_index, end_char_index == -1 ? line.end() : line.begin() + end_char_index);
    OnLineChanged(false, line_number, column, end_char_index - start_char_index, true);
}

void TextEditor::AddGlyphsToLine(int line_number, int target_index, LineT::iterator source_start, LineT::iterator source_end) {
    int target_column = GetCharacterColumn(line_number, target_index);
    int chars_inserted = std::distance(source_start, source_end);
    auto &line = Lines[line_number];
    OnLineChanged(true, line_number, target_column, chars_inserted, false);
    line.insert(line.begin() + target_index, source_start, source_end);
    OnLineChanged(false, line_number, target_column, chars_inserted, false);
}

void TextEditor::AddGlyphToLine(int line_number, int target_index, Glyph glyph) {
    int target_column = GetCharacterColumn(line_number, target_index);
    auto &line = Lines[line_number];
    OnLineChanged(true, line_number, target_column, 1, false);
    line.insert(line.begin() + target_index, glyph);
    OnLineChanged(false, line_number, target_column, 1, false);
}

TextEditor::LineT &TextEditor::InsertLine(int line_number) {
    assert(!ReadOnly);

    auto &result = *Lines.insert(Lines.begin() + line_number, LineT());
    OnLineAdded(line_number);

    return result;
}

ImU32 TextEditor::GetGlyphColor(const Glyph &glyph) const {
    if (!ColorizerEnabled) return Pallette[(int)PaletteIndexT::Default];
    if (glyph.IsComment) return Pallette[(int)PaletteIndexT::Comment];
    if (glyph.IsMultiLineComment) return Pallette[(int)PaletteIndexT::MultiLineComment];

    const auto color = Pallette[(int)glyph.ColorIndex];
    if (glyph.IsPreprocessor) {
        const auto ppcolor = Pallette[(int)PaletteIndexT::Preprocessor];
        const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
        const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

bool TextEditor::IsGlyphWordChar(const Glyph &glyph) {
    const int sizeInBytes = UTF8CharLength(glyph.Char);
    return sizeInBytes > 1 ||
        (glyph.Char >= 'a' && glyph.Char <= 'z') ||
        (glyph.Char >= 'A' && glyph.Char <= 'Z') ||
        (glyph.Char >= '0' && glyph.Char <= '9') ||
        glyph.Char == '_';
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
        auto is_osx = io.ConfigMacOSXBehaviors;
        auto alt = io.KeyAlt;
        auto ctrl = io.KeyCtrl;
        auto shift = io.KeyShift;
        auto super = io.KeySuper;

        auto is_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift;
        auto is_shift_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && shift && !alt;
        auto is_wordmove_key = is_osx ? alt : ctrl;
        auto is_alt_only = alt && !ctrl && !shift && !super;
        auto is_ctrl_only = ctrl && !alt && !shift && !super;
        auto is_shift_only = shift && !alt && !ctrl && !super;

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
            MoveUp(GetPageSize() - 4, shift);
        else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_PageDown))
            MoveDown(GetPageSize() - 4, shift);
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
        else if (!alt && ctrl && shift && !super && IsPressed(ImGuiKey_UpArrow))
            MoveUpCurrentLines();
        else if (!alt && ctrl && shift && !super && IsPressed(ImGuiKey_DownArrow))
            MoveDownCurrentLines();
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
        else if (is_shortcut && IsPressed(ImGuiKey_X))
            Cut();
        else if (is_shift_only && IsPressed(ImGuiKey_Delete))
            Cut();
        else if (is_shortcut && IsPressed(ImGuiKey_A))
            SelectAll();
        else if (is_shortcut && IsPressed(ImGuiKey_D))
            AddCursorForNextOccurrence();
        else if (!ReadOnly && !alt && !ctrl && !shift && !super && (IsPressed(ImGuiKey_Enter) || IsPressed(ImGuiKey_KeypadEnter)))
            EnterCharacter('\n', false);
        else if (!ReadOnly && !alt && !ctrl && !super && IsPressed(ImGuiKey_Tab))
            EnterCharacter('\t', shift);
        if (!ReadOnly && !io.InputQueueCharacters.empty() && ctrl == alt && !super) {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                auto c = io.InputQueueCharacters[i];
                if (c != 0 && (c == '\n' || c >= 32))
                    EnterCharacter(c, shift);
            }
            io.InputQueueCharacters.resize(0);
        }
    }
}

void TextEditor::HandleMouseInputs() {
    ImGuiIO &io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    // Pan with middle mouse button
    State.Panning &= ImGui::IsMouseDown(2);
    if (State.Panning && ImGui::IsMouseDragging(2)) {
        ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
        ImVec2 current_mouse_pos = ImGui::GetMouseDragDelta(2);
        ImVec2 mouse_delta = current_mouse_pos = State.LastMousePos;
        ImGui::SetScrollY(scroll.y - mouse_delta.y);
        ImGui::SetScrollX(scroll.x - mouse_delta.x);
        State.LastMousePos = current_mouse_pos;
    }

    // Mouse left button dragging (=> update selection)
    State.IsDraggingSelection &= ImGui::IsMouseDown(0);
    if (State.IsDraggingSelection && ImGui::IsMouseDragging(0)) {
        io.WantCaptureMouse = true;
        Coordinates cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
        SetCursorPosition(cursor_coords, State.GetLastAddedCursorIndex(), false);
    }

    if (ImGui::IsWindowHovered()) {
        auto is_click = ImGui::IsMouseClicked(0);
        if (!shift && !alt) {
            if (is_click) State.IsDraggingSelection = true;

            // Pan with middle mouse button
            if (ImGui::IsMouseClicked(2)) {
                State.Panning = true;
                State.LastMousePos = ImGui::GetMouseDragDelta(2);
            }

            bool is_double_click = ImGui::IsMouseDoubleClicked(0);
            auto t = ImGui::GetTime();
            bool is_triple_click = is_click && !is_double_click && (LastClickTime != -1.0f && (t - LastClickTime) < io.MouseDoubleClickTime);
            if (is_triple_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                Coordinates cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                Coordinates target_cursor_pos = cursor_coords.Line < Lines.size() - 1 ?
                    Coordinates{cursor_coords.Line + 1, 0} :
                    Coordinates{cursor_coords.Line, GetLineMaxColumn(cursor_coords.Line)};
                SetSelection({cursor_coords.Line, 0}, target_cursor_pos, State.CurrentCursor);

                LastClickTime = -1.0f;
            } else if (is_double_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                Coordinates cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(FindWordStart(cursor_coords), FindWordEnd(cursor_coords), State.CurrentCursor);

                LastClickTime = float(ImGui::GetTime());
            } else if (is_click) {
                if (ctrl) State.AddCursor();
                else State.CurrentCursor = 0;

                bool is_over_line_number;
                Coordinates cursor_coords = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite, &is_over_line_number);
                if (is_over_line_number) {
                    Coordinates target_cursor_pos = cursor_coords.Line < Lines.size() - 1 ?
                        Coordinates{cursor_coords.Line + 1, 0} :
                        Coordinates{cursor_coords.Line, GetLineMaxColumn(cursor_coords.Line)};
                    SetSelection({cursor_coords.Line, 0}, target_cursor_pos, State.CurrentCursor);
                } else {
                    SetCursorPosition(cursor_coords, State.GetLastAddedCursorIndex());
                }

                LastClickTime = float(ImGui::GetTime());
            } else if (ImGui::IsMouseReleased(0)) {
                State.SortCursorsFromTopToBottom();
                MergeCursorsIfPossible();
            }
        } else if (shift && is_click) {
            Coordinates new_selection = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
            SetCursorPosition(SanitizeCoordinates(new_selection), State.CurrentCursor, false);
        }
    }
}

void TextEditor::UpdatePalette() {
    /* Update palette with the current alpha from style */
    for (int i = 0; i < (int)PaletteIndexT::Max; ++i) {
        auto color = U32ColorToVec4(PalletteBase[i]);
        color.w *= ImGui::GetStyle().Alpha;
        Pallette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }
}

void TextEditor::Render(bool is_parent_focused) {
    /* Compute CharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    CharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * LineSpacing);

    assert(LineBuffer.empty());

    auto content_size = ImGui::GetWindowContentRegionMax();
    auto dl = ImGui::GetWindowDrawList();

    if (ScrollToTop) {
        ScrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    auto scroll_x = ImGui::GetScrollX();
    auto scroll_y = ImGui::GetScrollY();

    auto line_number = (int)floor(scroll_y / CharAdvance.y);
    auto global_line_max = (int)Lines.size();
    auto line_max = std::max(0, std::min((int)Lines.size() - 1, line_number + (int)floor((scroll_y + content_size.y) / CharAdvance.y)));

    // Deduce TextStart by evaluating Lines size (global line_max) plus two spaces as text width
    char buf[16];
    snprintf(buf, 16, " %d ", global_line_max);
    TextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + LeftMargin;

    if (!Lines.empty()) {
        float space_size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

        while (line_number <= line_max) {
            ImVec2 line_start_screen_pos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + line_number * CharAdvance.y);
            ImVec2 text_screen_pos = ImVec2(line_start_screen_pos.x + TextStart, line_start_screen_pos.y);

            auto &line = Lines[line_number];
            LongestLineLength = std::max(TextStart + TextDistanceToLineStart(Coordinates(line_number, GetLineMaxColumn(line_number))), LongestLineLength);
            Coordinates line_start_coord(line_number, 0);
            Coordinates line_end_coord(line_number, GetLineMaxColumn(line_number));
            // Draw selection for the current line
            for (int c = 0; c <= State.CurrentCursor; c++) {
                float sstart = -1.0f;
                float ssend = -1.0f;
                assert(State.Cursors[c].GetSelectionStart() <= State.Cursors[c].GetSelectionEnd());
                if (State.Cursors[c].GetSelectionStart() <= line_end_coord) {
                    sstart = State.Cursors[c].GetSelectionStart() > line_start_coord ? TextDistanceToLineStart(State.Cursors[c].GetSelectionStart()) : 0.0f;
                }
                if (State.Cursors[c].GetSelectionEnd() > line_start_coord) {
                    ssend = TextDistanceToLineStart(State.Cursors[c].GetSelectionEnd() < line_end_coord ? State.Cursors[c].GetSelectionEnd() : line_end_coord);
                }
                if (State.Cursors[c].GetSelectionEnd().Line > line_number) {
                    ssend += CharAdvance.x;
                }

                if (sstart != -1 && ssend != -1 && sstart < ssend) {
                    ImVec2 vstart(line_start_screen_pos.x + TextStart + sstart, line_start_screen_pos.y);
                    ImVec2 vend(line_start_screen_pos.x + TextStart + ssend, line_start_screen_pos.y + CharAdvance.y);
                    dl->AddRectFilled(vstart, vend, Pallette[(int)PaletteIndexT::Selection]);
                }
            }

            // Draw line number (right aligned)
            snprintf(buf, 16, "%d  ", line_number + 1);

            auto line_numberWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
            dl->AddText(ImVec2(line_start_screen_pos.x + TextStart - line_numberWidth, line_start_screen_pos.y), Pallette[(int)PaletteIndexT::LineNumber], buf);

            std::vector<Coordinates> cursor_coords_in_this_line;
            for (int c = 0; c <= State.CurrentCursor; c++) {
                if (State.Cursors[c].InteractiveEnd.Line == line_number) {
                    cursor_coords_in_this_line.push_back(State.Cursors[c].InteractiveEnd);
                }
            }
            if (cursor_coords_in_this_line.size() > 0) {
                // Render the cursors
                auto focused = ImGui::IsWindowFocused() || is_parent_focused;
                if (focused) {
                    for (const auto &cursor_coords : cursor_coords_in_this_line) {
                        float width = 1.0f;
                        auto cindex = GetCharacterIndexR(cursor_coords);
                        float cx = TextDistanceToLineStart(cursor_coords);

                        if (Overwrite && cindex < (int)line.size()) {
                            auto c = line[cindex].Char;
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + cx) / (float(TabSize) * space_size))) * (float(TabSize) * space_size);
                                width = x - cx;
                            } else {
                                char buf2[2];
                                buf2[0] = line[cindex].Char;
                                buf2[1] = '\0';
                                width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
                            }
                        }
                        ImVec2 cstart(text_screen_pos.x + cx, line_start_screen_pos.y);
                        ImVec2 cend(text_screen_pos.x + cx + width, line_start_screen_pos.y + CharAdvance.y);
                        dl->AddRectFilled(cstart, cend, Pallette[(int)PaletteIndexT::Cursor]);
                    }
                }
            }

            // Render colorized text
            auto prev_color = line.empty() ? Pallette[(int)PaletteIndexT::Default] : GetGlyphColor(line[0]);
            ImVec2 buffer_offset;

            for (int i = 0; i < line.size();) {
                auto &glyph = line[i];
                auto color = GetGlyphColor(glyph);
                if ((color != prev_color || glyph.Char == '\t' || glyph.Char == ' ') && !LineBuffer.empty()) {
                    const ImVec2 new_offset(text_screen_pos.x + buffer_offset.x, text_screen_pos.y + buffer_offset.y);
                    dl->AddText(new_offset, prev_color, LineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, LineBuffer.c_str(), nullptr, nullptr);
                    buffer_offset.x += textSize.x;
                    LineBuffer.clear();
                }
                prev_color = color;

                if (glyph.Char == '\t') {
                    auto old_x = buffer_offset.x;
                    buffer_offset.x = (1.0f + std::floor((1.0f + buffer_offset.x) / (float(TabSize) * space_size))) * (float(TabSize) * space_size);
                    ++i;

                    if (ShowWhitespaces) {
                        const float s = ImGui::GetFontSize();
                        const float x1 = text_screen_pos.x + old_x + 1.0f;
                        const float y1 = text_screen_pos.y + buffer_offset.y + s * 0.5f;
                        const float x2 = text_screen_pos.x + buffer_offset.x - 1.0f;
                        const float gap = s * 0.2;
                        ImVec2 p1 = {x1, y1}, p2 = {x2, y1}, p3 = {x2 - gap, y1 - gap}, p4 = {x2 - gap, y1 + gap};
                        dl->AddLine(p1, p2, Pallette[(int)PaletteIndexT::ControlCharacter]);
                        dl->AddLine(p2, p3, Pallette[(int)PaletteIndexT::ControlCharacter]);
                        dl->AddLine(p2, p4, Pallette[(int)PaletteIndexT::ControlCharacter]);
                    }
                } else if (glyph.Char == ' ') {
                    if (ShowWhitespaces) {
                        const float s = ImGui::GetFontSize();
                        const float x = text_screen_pos.x + buffer_offset.x + space_size * 0.5f;
                        const float y = text_screen_pos.y + buffer_offset.y + s * 0.5f;
                        dl->AddCircleFilled({x, y}, 1.5f, Pallette[(int)PaletteIndexT::ControlCharacter], 4);
                    }
                    buffer_offset.x += space_size;
                    i++;
                } else {
                    auto l = UTF8CharLength(glyph.Char);
                    while (l-- > 0) LineBuffer.push_back(line[i++].Char);
                }
            }

            if (!LineBuffer.empty()) {
                const ImVec2 new_offset(text_screen_pos.x + buffer_offset.x, text_screen_pos.y + buffer_offset.y);
                dl->AddText(new_offset, prev_color, LineBuffer.c_str());
                LineBuffer.clear();
            }
            ++line_number;
        }
    }

    ImGui::SetCursorPos({0, 0});
    ImGui::Dummy({LongestLineLength + 15, Lines.size() * CharAdvance.y});
    if (ScrollToCursor) {
        EnsureCursorVisible();
        ScrollToCursor = false;
    }
}

bool TextEditor::FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out) {
    assert(text_size > 0);
    bool fmatches = false;
    int fline, ifline;
    int findex, ifindex;

    ifline = fline = from.Line;
    ifindex = findex = GetCharacterIndexR(from);

    while (true) {
        bool matches;
        {
            // Match function.
            int line_offset = 0;
            int current_char_index = findex;
            int i = 0;
            for (; i < text_size; i++) {
                if (current_char_index == Lines[fline + line_offset].size()) {
                    if (text[i] != '\n' || fline + line_offset + 1 >= Lines.size()) break;

                    current_char_index = 0;
                    line_offset++;
                } else {
                    if (Lines[fline + line_offset][current_char_index].Char != text[i]) break;

                    current_char_index++;
                }
            }
            matches = i == text_size;
            if (matches) {
                start_out = {fline, GetCharacterColumn(fline, findex)};
                end_out = {fline + line_offset, GetCharacterColumn(fline + line_offset, current_char_index)};
                return true;
            }
        }

        // Move forward.
        if (findex == Lines[fline].size()) { // Need to consider line breaks.
            fline = fline == Lines.size() - 1 ? 0 : fline + 1;
            findex = 0;
        } else {
            findex++;
        }

        // Detect complete scan.
        if (findex == ifindex && fline == ifline) return false;
    }

    return false;
}
bool TextEditor::Render(const char *title, bool is_parent_focused, const ImVec2 &size, bool border) {
    if (State.CursorPositionChanged) OnCursorPositionChanged();
    State.CursorPositionChanged = false;

    WithinRender = true;

    UpdatePalette();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(Pallette[(int)PaletteIndexT::Background]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(title, size, border, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

    bool is_focused = ImGui::IsWindowFocused();
    HandleKeyboardInputs(is_parent_focused);
    HandleMouseInputs();
    ColorizeInternal();
    Render(is_parent_focused);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    WithinRender = false;
    return is_focused;
}

void TextEditor::SetText(const string &text) {
    Lines.clear();
    Lines.emplace_back(LineT());
    for (auto chr : text) {
        if (chr == '\r') {
            // ignore the carriage return character
        } else if (chr == '\n') {
            Lines.emplace_back(LineT());
        } else {
            Lines.back().emplace_back(Glyph(chr, PaletteIndexT::Default));
        }
    }

    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::SetTextLines(const std::vector<string> &text_lines) {
    Lines.clear();

    if (text_lines.empty()) {
        Lines.emplace_back(LineT());
    } else {
        Lines.resize(text_lines.size());
        for (size_t i = 0; i < text_lines.size(); ++i) {
            const string &text_line = text_lines[i];
            Lines[i].reserve(text_line.size());
            for (size_t j = 0; j < text_line.size(); ++j) {
                Lines[i].emplace_back(Glyph(text_line[j], PaletteIndexT::Default));
            }
        }
    }

    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::ChangeCurrentLinesIndentation(bool increase) {
    assert(!ReadOnly);

    UndoRecord u;
    u.Before = State;

    for (int c = State.CurrentCursor; c > -1; c--) {
        for (int current_line = State.Cursors[c].GetSelectionEnd().Line; current_line >= State.Cursors[c].GetSelectionStart().Line; current_line--) {
            // Check if selection ends at line start.
            if (Coordinates{current_line, 0} == State.Cursors[c].GetSelectionEnd() && State.Cursors[c].GetSelectionEnd() != State.Cursors[c].GetSelectionStart())
                continue;

            if (increase) {
                if (Lines[current_line].size() > 0) {
                    Coordinates line_start = {current_line, 0};
                    Coordinates insertion_end = line_start;
                    InsertTextAt(insertion_end, "\t"); // Sets insertion end.
                    u.Operations.push_back({"\t", line_start, insertion_end, UndoOperationType::Add});
                    Colorize(line_start.Line, 1);
                }
            } else {
                Coordinates start = {current_line, 0};
                Coordinates end = {current_line, TabSize};
                int char_index = GetCharacterIndexL(end) - 1;
                while (char_index > -1 && (Lines[current_line][char_index].Char == ' ' || Lines[current_line][char_index].Char == '\t')) char_index--;
                bool only_space_chars_found = char_index == -1;
                if (only_space_chars_found) {
                    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});
                    DeleteRange(start, end);
                    Colorize(current_line, 1);
                }
            }
        }
    }

    EnsureCursorVisible();
    if (u.Operations.size() > 0) AddUndo(u);
}

void TextEditor::MoveUpCurrentLines() {
    assert(!ReadOnly);

    UndoRecord u;
    u.Before = State;

    std::set<int> affected_lines;
    int min_line = -1;
    int max_line = -1;
    // Cursors are expected to be sorted from top to bottom.
    for (int c = State.CurrentCursor; c > -1; c--) {
        for (int current_line = State.Cursors[c].GetSelectionEnd().Line; current_line >= State.Cursors[c].GetSelectionStart().Line; current_line--) {
            // Check if selection ends at line start.
            if (Coordinates{current_line, 0} == State.Cursors[c].GetSelectionEnd() && State.Cursors[c].GetSelectionEnd() != State.Cursors[c].GetSelectionStart())
                continue;
            affected_lines.insert(current_line);
            min_line = min_line == -1 ? current_line : (current_line < min_line ? current_line : min_line);
            max_line = max_line == -1 ? current_line : (current_line > max_line ? current_line : max_line);
        }
    }
    if (min_line == 0) return; // Can't move up anymore.

    Coordinates start = {min_line - 1, 0};
    Coordinates end = {max_line, GetLineMaxColumn(max_line)};
    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});

    // Lines should be sorted at this point.
    for (int line : affected_lines) std::swap(Lines[line - 1], Lines[line]);

    for (int c = State.CurrentCursor; c > -1; c--) {
        State.Cursors[c].InteractiveStart.Line -= 1;
        State.Cursors[c].InteractiveEnd.Line -= 1;
        // No need to set CursorPositionChanged as cursors will remain sorted.
    }

    end = {max_line, GetLineMaxColumn(max_line)}; // This line is swapped with line above. Need to find new max column.
    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Add});
    u.After = State;
    AddUndo(u);
}

void TextEditor::MoveDownCurrentLines() {
    assert(!ReadOnly);

    UndoRecord u;
    u.Before = State;

    std::set<int> affected_lines;
    int min_line = -1;
    int max_line = -1;

    // Cursors are expected to be sorted from top to bottom.
    for (int c = 0; c <= State.CurrentCursor; c++) {
        for (int current_line = State.Cursors[c].GetSelectionEnd().Line; current_line >= State.Cursors[c].GetSelectionStart().Line; current_line--) {
            if (Coordinates{current_line, 0} == State.Cursors[c].GetSelectionEnd() && State.Cursors[c].GetSelectionEnd() != State.Cursors[c].GetSelectionStart()) // when selection ends at line start
                continue;
            affected_lines.insert(current_line);
            min_line = min_line == -1 ? current_line : (current_line < min_line ? current_line : min_line);
            max_line = max_line == -1 ? current_line : (current_line > max_line ? current_line : max_line);
        }
    }
    if (max_line == Lines.size() - 1) return; // Can't move down anymore.

    Coordinates start = {min_line, 0};
    Coordinates end = {max_line + 1, GetLineMaxColumn(max_line + 1)};
    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});

    // Lines should be sorted at this point.
    std::set<int>::reverse_iterator rit;
    for (rit = affected_lines.rbegin(); rit != affected_lines.rend(); rit++) std::swap(Lines[*rit + 1], Lines[*rit]);

    for (int c = State.CurrentCursor; c > -1; c--) {
        State.Cursors[c].InteractiveStart.Line += 1;
        State.Cursors[c].InteractiveEnd.Line += 1;
        // no need to set CursorPositionChanged as cursors will remain sorted
    }

    end = {max_line + 1, GetLineMaxColumn(max_line + 1)}; // This line is swapped with line below. Need to find new max column.
    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Add});
    u.After = State;
    AddUndo(u);
}

void TextEditor::ToggleLineComment() {
    if (LanguageDef == nullptr) return;

    assert(!ReadOnly);
    const string &comment_str = LanguageDef->SingleLineComment;

    UndoRecord u;
    u.Before = State;

    bool should_add_comment = false;
    std::unordered_set<int> affected_lines;
    for (int c = State.CurrentCursor; c > -1; c--) {
        for (int current_line = State.Cursors[c].GetSelectionEnd().Line; current_line >= State.Cursors[c].GetSelectionStart().Line; current_line--) {
            if (Coordinates{current_line, 0} == State.Cursors[c].GetSelectionEnd() && State.Cursors[c].GetSelectionEnd() != State.Cursors[c].GetSelectionStart()) // when selection ends at line start
                continue;
            affected_lines.insert(current_line);
            int current_index = 0;
            while (current_index < Lines[current_line].size() && (Lines[current_line][current_index].Char == ' ' || Lines[current_line][current_index].Char == '\t')) current_index++;
            if (current_index == Lines[current_line].size()) continue;

            int i = 0;
            while (i < comment_str.length() && current_index + i < Lines[current_line].size() && Lines[current_line][current_index + i].Char == comment_str[i]) i++;
            bool matched = i == comment_str.length();
            should_add_comment |= !matched;
        }
    }

    if (should_add_comment) {
        for (int current_line : affected_lines) {
            Coordinates line_start = {current_line, 0};
            Coordinates insertion_end = line_start;
            InsertTextAt(insertion_end, (comment_str + ' ').c_str()); // sets insertion end
            u.Operations.push_back({(comment_str + ' '), line_start, insertion_end, UndoOperationType::Add});
            Colorize(line_start.Line, 1);
        }
    } else {
        for (int current_line : affected_lines) {
            int current_index = 0;
            while (current_index < Lines[current_line].size() && (Lines[current_line][current_index].Char == ' ' || Lines[current_line][current_index].Char == '\t')) current_index++;
            if (current_index == Lines[current_line].size()) continue;

            int i = 0;
            while (i < comment_str.length() && current_index + i < Lines[current_line].size() && Lines[current_line][current_index + i].Char == comment_str[i]) i++;
            bool matched = i == comment_str.length();
            assert(matched);
            if (current_index + i < Lines[current_line].size() && Lines[current_line][current_index + i].Char == ' ') {
                i++;
            }

            Coordinates start = {current_line, GetCharacterColumn(current_line, current_index)};
            Coordinates end = {current_line, GetCharacterColumn(current_line, current_index + i)};
            u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});
            DeleteRange(start, end);
            Colorize(current_line, 1);
        }
    }
}

void TextEditor::EnterCharacter(ImWchar character, bool is_shift) {
    assert(!ReadOnly);

    bool has_selection = AnyCursorHasSelection();
    bool any_cursor_has_multiline_selection = false;
    for (int c = State.CurrentCursor; c > -1; c--) {
        if (State.Cursors[c].GetSelectionStart().Line != State.Cursors[c].GetSelectionEnd().Line) {
            any_cursor_has_multiline_selection = true;
            break;
        }
    }
    bool is_indent_op = has_selection && any_cursor_has_multiline_selection && character == '\t';
    if (is_indent_op) {
        ChangeCurrentLinesIndentation(!is_shift);
        return;
    }

    UndoRecord u;
    u.Before = State;
    if (has_selection) {
        for (int c = State.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd(), UndoOperationType::Delete});
            DeleteSelection(c);
        }
    }

    std::vector<Coordinates> coords;
    for (int c = State.CurrentCursor; c > -1; c--) // order important here for typing \n in the same line at the same time
    {
        auto coord = GetActualCursorCoordinates(c);
        coords.push_back(coord);
        UndoOperation added;
        added.Type = UndoOperationType::Add;
        added.Start = coord;

        assert(!Lines.empty());

        if (character == '\n') {
            InsertLine(coord.Line + 1);
            auto &line = Lines[coord.Line];
            auto &new_line = Lines[coord.Line + 1];

            added.Text = "";
            added.Text += (char)character;
            if (AutoIndent) {
                for (int i = 0; i < line.size() && isascii(line[i].Char) && isblank(line[i].Char); i++) {
                    new_line.push_back(line[i]);
                    added.Text += line[i].Char;
                }
            }

            const size_t whitespace_size = new_line.size();
            auto cindex = GetCharacterIndexR(coord);
            AddGlyphsToLine(coord.Line + 1, new_line.size(), line.begin() + cindex, line.end());
            RemoveGlyphsFromLine(coord.Line, cindex);
            SetCursorPosition(Coordinates(coord.Line + 1, GetCharacterColumn(coord.Line + 1, (int)whitespace_size)), c);
        } else {
            char buf[7];
            int e = ITextCharToUtf8(buf, 7, character);
            if (e > 0) {
                buf[e] = '\0';
                auto &line = Lines[coord.Line];
                auto cindex = GetCharacterIndexR(coord);

                if (Overwrite && cindex < (int)line.size()) {
                    auto d = UTF8CharLength(line[cindex].Char);

                    UndoOperation removed;
                    removed.Type = UndoOperationType::Delete;
                    removed.Start = State.Cursors[c].InteractiveEnd;
                    removed.End = Coordinates(coord.Line, GetCharacterColumn(coord.Line, cindex + d));

                    while (d-- > 0 && cindex < (int)line.size()) {
                        removed.Text += line[cindex].Char;
                        RemoveGlyphsFromLine(coord.Line, cindex, cindex + 1);
                    }
                    u.Operations.push_back(removed);
                }

                for (auto p = buf; *p != '\0'; p++, ++cindex) {
                    AddGlyphToLine(coord.Line, cindex, Glyph(*p, PaletteIndexT::Default));
                }
                added.Text = buf;

                SetCursorPosition(Coordinates(coord.Line, GetCharacterColumn(coord.Line, cindex)), c);
            } else {
                continue;
            }
        }

        added.End = GetActualCursorCoordinates(c);
        u.Operations.push_back(added);
    }

    u.After = State;
    AddUndo(u);

    for (const auto &coord : coords) Colorize(coord.Line - 1, 3);
    EnsureCursorVisible();
}

void TextEditor::OnCursorPositionChanged() {
    if (State.IsDraggingSelection) return;

    State.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
}

void TextEditor::SetCursorPosition(const Coordinates &position, int cursor, bool clear_selection) {
    if (cursor == -1) cursor = State.CurrentCursor;

    State.CursorPositionChanged = true;
    if (clear_selection) State.Cursors[cursor].InteractiveStart = position;

    if (State.Cursors[cursor].InteractiveEnd != position) {
        State.Cursors[cursor].InteractiveEnd = position;
        EnsureCursorVisible();
    }
}

void TextEditor::SetCursorPosition(int line_number, int characterIndex, int cursor, bool clear_selection) {
    SetCursorPosition({line_number, GetCharacterColumn(line_number, characterIndex)}, cursor);
    (void)clear_selection;
}

void TextEditor::SetTabSize(int tab_size) {
    TabSize = std::max(0, std::min(32, tab_size));
}

void TextEditor::InsertText(const string &text, int cursor) {
    InsertText(text.c_str(), cursor);
}

void TextEditor::InsertText(const char *text, int cursor) {
    if (text == nullptr) return;

    if (cursor == -1) cursor = State.CurrentCursor;

    auto pos = GetActualCursorCoordinates(cursor);
    auto start = std::min(pos, State.Cursors[cursor].GetSelectionStart());
    int total_lines = pos.Line - start.Line + InsertTextAt(pos, text);
    SetCursorPosition(pos, cursor);
    Colorize(start.Line - 1, total_lines + 2);
}

void TextEditor::DeleteSelection(int cursor) {
    if (cursor == -1) cursor = State.CurrentCursor;

    if (State.Cursors[cursor].GetSelectionEnd() == State.Cursors[cursor].GetSelectionStart()) return;

    DeleteRange(State.Cursors[cursor].GetSelectionStart(), State.Cursors[cursor].GetSelectionEnd());
    SetCursorPosition(State.Cursors[cursor].GetSelectionStart(), cursor);
    Colorize(State.Cursors[cursor].GetSelectionStart().Line, 1);
}

void TextEditor::MoveCoords(Coordinates &coords, MoveDirection direction, bool is_word_mode, int line_count) const {
    int cindex = GetCharacterIndexR(coords);
    int lindex = coords.Line;
    auto &line = Lines[lindex];
    switch (direction) {
        case MoveDirection::Right:
            if (cindex >= line.size()) {
                if (lindex < Lines.size() - 1) {
                    coords.Line = std::max(0, std::min((int)Lines.size() - 1, lindex + 1));
                    coords.Column = 0;
                }
            } else {
                int delta = UTF8CharLength(line[cindex].Char);
                cindex = std::min(cindex + delta, (int)line.size());
                int one_step_right_column = GetCharacterColumn(lindex, cindex);
                if (is_word_mode) {
                    coords = FindWordEnd(coords);
                    coords.Column = std::max(coords.Column, one_step_right_column);
                } else {
                    coords.Column = one_step_right_column;
                }
            }
            break;
        case MoveDirection::Left:
            if (cindex == 0) {
                if (lindex > 0) {
                    coords.Line = lindex - 1;
                    coords.Column = GetLineMaxColumn(coords.Line);
                }
            } else {
                --cindex;
                if (cindex > 0) {
                    if ((int)Lines.size() > lindex) {
                        while (cindex > 0 && IsUTFSequence(Lines[lindex][cindex].Char))
                            --cindex;
                    }
                }
                coords.Column = GetCharacterColumn(lindex, cindex);
                if (is_word_mode)
                    coords = FindWordStart(coords);
            }
            break;
        case MoveDirection::Up:
            coords.Line = std::max(0, lindex - line_count);
            break;
        case MoveDirection::Down:
            coords.Line = std::max(0, std::min((int)Lines.size() - 1, lindex + line_count));
            break;
    }
}

void TextEditor::MoveUp(int amount, bool select) {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        Coordinates new_coords = State.Cursors[c].InteractiveEnd;
        MoveCoords(new_coords, MoveDirection::Up, false, amount);
        SetCursorPosition(new_coords, c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveDown(int amount, bool select) {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        assert(State.Cursors[c].InteractiveEnd.Column >= 0);
        Coordinates new_coords = State.Cursors[c].InteractiveEnd;
        MoveCoords(new_coords, MoveDirection::Down, false, amount);
        SetCursorPosition(new_coords, c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveLeft(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    if (AnyCursorHasSelection() && !select && !is_word_mode) {
        for (int c = 0; c <= State.CurrentCursor; c++) {
            SetCursorPosition(State.Cursors[c].GetSelectionStart(), c);
        }
    } else {
        for (int c = 0; c <= State.CurrentCursor; c++) {
            Coordinates new_coords = State.Cursors[c].InteractiveEnd;
            MoveCoords(new_coords, MoveDirection::Left, is_word_mode);
            SetCursorPosition(new_coords, c, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    if (AnyCursorHasSelection() && !select && !is_word_mode) {
        for (int c = 0; c <= State.CurrentCursor; c++) {
            SetCursorPosition(State.Cursors[c].GetSelectionEnd(), c);
        }
    } else {
        for (int c = 0; c <= State.CurrentCursor; c++) {
            Coordinates new_coords = State.Cursors[c].InteractiveEnd;
            MoveCoords(new_coords, MoveDirection::Right, is_word_mode);
            SetCursorPosition(new_coords, c, !select);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool select) {
    SetCursorPosition(Coordinates(0, 0), State.CurrentCursor, !select);
}

void TextEditor::TextEditor::MoveBottom(bool select) {
    int max_line = (int)Lines.size() - 1;
    Coordinates new_pos = Coordinates(max_line, GetLineMaxColumn(max_line));
    SetCursorPosition(new_pos, State.CurrentCursor, !select);
}

void TextEditor::MoveHome(bool select) {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        SetCursorPosition(Coordinates(State.Cursors[c].InteractiveEnd.Line, 0), c, !select);
    }
}

void TextEditor::MoveEnd(bool select) {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        int lindex = State.Cursors[c].InteractiveEnd.Line;
        SetCursorPosition(Coordinates(lindex, GetLineMaxColumn(lindex)), c, !select);
    }
}

void TextEditor::Delete(bool is_word_mode, const EditorState *editor_state) {
    assert(!ReadOnly);
    if (Lines.empty()) return;

    if (AnyCursorHasSelection()) {
        UndoRecord u;
        u.Before = editor_state == nullptr ? State : *editor_state;
        for (int c = State.CurrentCursor; c > -1; c--) {
            if (State.Cursors[c].HasSelection()) {
                u.Operations.push_back({GetSelectedText(c), State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd(), UndoOperationType::Delete});
                DeleteSelection(c);
            }
        }
        u.After = State;
        AddUndo(u);
    } else {
        EditorState state_before_deleting = State;
        MoveRight(true, is_word_mode);
        // Can't do delete if any cursor is at the end of the last line.
        if (!AllCursorsHaveSelection()) {
            if (AnyCursorHasSelection()) MoveLeft();
            return;
        }

        OnCursorPositionChanged(); // Might combine cursors.
        Delete(is_word_mode, &state_before_deleting);
    }
}

void TextEditor::Backspace(bool is_word_mode) {
    assert(!ReadOnly);
    if (Lines.empty()) return;

    if (AnyCursorHasSelection()) {
        Delete(is_word_mode);
    } else {
        EditorState state_before_deleting = State;
        MoveLeft(true, is_word_mode);
        // Can't do backspace if any cursor is at {0,0}.
        if (!AllCursorsHaveSelection()) {
            if (AnyCursorHasSelection()) MoveRight();
            return;
        }
        OnCursorPositionChanged(); // Might combine cursors.
        Delete(is_word_mode, &state_before_deleting);
    }
}

void TextEditor::SetSelection(int start_line_number, int start_char_index, int end_line_number, int end_char_index, int cursor) {
    Coordinates start_coords = {start_line_number, GetCharacterColumn(start_line_number, start_char_index)};
    Coordinates end_coors = {end_line_number, GetCharacterColumn(end_line_number, end_char_index)};
    SetSelection(start_coords, end_coors, cursor);
}

void TextEditor::SetSelection(Coordinates start, Coordinates end, int cursor) {
    if (cursor == -1) cursor = State.CurrentCursor;
    State.Cursors[cursor].InteractiveStart = start;
    SetCursorPosition(end, cursor, false);
}

void TextEditor::SelectAll() {
    State.CurrentCursor = 0;
    MoveTop();
    MoveBottom(true);
}

bool TextEditor::AnyCursorHasSelection() const {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        if (State.Cursors[c].HasSelection()) return true;
    }
    return false;
}

bool TextEditor::AllCursorsHaveSelection() const {
    for (int c = 0; c <= State.CurrentCursor; c++) {
        if (!State.Cursors[c].HasSelection()) return false;
    }
    return true;
}

void TextEditor::Copy() {
    if (AnyCursorHasSelection()) {
        ImGui::SetClipboardText(GetClipboardText().c_str());
    } else {
        if (!Lines.empty()) {
            string str;
            auto &line = Lines[GetActualCursorCoordinates().Line];
            for (auto &g : line) str.push_back(g.Char);
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut() {
    if (ReadOnly) {
        Copy();
    } else {
        if (AnyCursorHasSelection()) {
            UndoRecord u;
            u.Before = State;

            Copy();
            for (int c = State.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd(), UndoOperationType::Delete});
                DeleteSelection(c);
            }

            u.After = State;
            AddUndo(u);
        }
    }
}

void TextEditor::Paste() {
    if (ReadOnly) return;

    // check if we should do multicursor paste
    string clip_text = ImGui::GetClipboardText();
    bool can_paste_to_multiple_cursors = false;
    std::vector<std::pair<int, int>> clip_text_lines;
    if (State.CurrentCursor > 0) {
        clip_text_lines.push_back({0, 0});
        for (int i = 0; i < clip_text.length(); i++) {
            if (clip_text[i] == '\n') {
                clip_text_lines.back().second = i;
                clip_text_lines.push_back({i + 1, 0});
            }
        }
        clip_text_lines.back().second = clip_text.length();
        can_paste_to_multiple_cursors = clip_text_lines.size() == State.CurrentCursor + 1;
    }

    if (clip_text.length() > 0) {
        UndoRecord u;
        u.Before = State;

        if (AnyCursorHasSelection()) {
            for (int c = State.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd(), UndoOperationType::Delete});
                DeleteSelection(c);
            }
        }

        for (int c = State.CurrentCursor; c > -1; c--) {
            Coordinates start = GetActualCursorCoordinates(c);
            if (can_paste_to_multiple_cursors) {
                string clip_sub_text = clip_text.substr(clip_text_lines[c].first, clip_text_lines[c].second - clip_text_lines[c].first);
                InsertText(clip_sub_text, c);
                u.Operations.push_back({clip_sub_text, start, GetActualCursorCoordinates(c), UndoOperationType::Add});
            } else {
                InsertText(clip_text, c);
                u.Operations.push_back({clip_text, start, GetActualCursorCoordinates(c), UndoOperationType::Add});
            }
        }

        u.After = State;
        AddUndo(u);
    }
}

int TextEditor::GetUndoIndex() const { return UndoIndex; }
bool TextEditor::CanUndo() const { return !ReadOnly && UndoIndex > 0; }
bool TextEditor::CanRedo() const { return !ReadOnly && UndoIndex < (int)UndoBuffer.size(); }

void TextEditor::Undo(int aSteps) {
    while (CanUndo() && aSteps-- > 0) UndoBuffer[--UndoIndex].Undo(this);
}
void TextEditor::Redo(int aSteps) {
    while (CanRedo() && aSteps-- > 0) UndoBuffer[UndoIndex++].Redo(this);
}

void TextEditor::AddCursorForNextOccurrence() {
    const Cursor &current_cursor = State.Cursors[State.GetLastAddedCursorIndex()];
    if (current_cursor.GetSelectionStart() == current_cursor.GetSelectionEnd()) return;

    string selection_text = GetText(current_cursor.GetSelectionStart(), current_cursor.GetSelectionEnd());
    Coordinates next_start, next_end;
    if (!FindNextOccurrence(selection_text.c_str(), selection_text.length(), current_cursor.GetSelectionEnd(), next_start, next_end)) return;

    State.AddCursor();
    SetSelection(next_start, next_end, State.CurrentCursor);
    State.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
    EnsureCursorVisible();
}

const TextEditor::PaletteT &TextEditor::GetDarkPalette() {
    static const PaletteT p = {{
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
    return p;
}

const TextEditor::PaletteT &TextEditor::GetMarianaPalette() {
    static const PaletteT p = {{
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
    return p;
}

const TextEditor::PaletteT &TextEditor::GetLightPalette() {
    static const PaletteT p = {{
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
    return p;
}

const TextEditor::PaletteT &TextEditor::GetRetroBluePalette() {
    static const PaletteT p = {{
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
    return p;
}

void TextEditor::MergeCursorsIfPossible() {
    // requires the cursors to be sorted from top to bottom
    std::unordered_set<int> cursors_to_delete;
    if (AnyCursorHasSelection()) {
        // merge cursors if they overlap
        for (int c = State.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1; // pc for previous cursor
            bool pc_contains_c = State.Cursors[pc].GetSelectionEnd() >= State.Cursors[c].GetSelectionEnd();
            bool pc_contains_start_of_c = State.Cursors[pc].GetSelectionEnd() > State.Cursors[c].GetSelectionStart();
            if (pc_contains_c) {
                cursors_to_delete.insert(c);
            } else if (pc_contains_start_of_c) {
                Coordinates pc_start = State.Cursors[pc].GetSelectionStart();
                Coordinates c_end = State.Cursors[c].GetSelectionEnd();
                State.Cursors[pc].InteractiveEnd = c_end;
                State.Cursors[pc].InteractiveStart = pc_start;
                cursors_to_delete.insert(c);
            }
        }
    } else {
        // merge cursors if they are at the same position
        for (int c = State.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1;
            if (State.Cursors[pc].InteractiveEnd == State.Cursors[c].InteractiveEnd) {
                cursors_to_delete.insert(c);
            }
        }
    }
    for (int c = State.CurrentCursor; c > -1; c--) // iterate backwards through each of them
    {
        if (cursors_to_delete.find(c) != cursors_to_delete.end()) {
            State.Cursors.erase(State.Cursors.begin() + c);
        }
    }
    State.CurrentCursor -= cursors_to_delete.size();
}

string TextEditor::GetText() const {
    auto last_line = (int)Lines.size() - 1;
    auto last_line_length = GetLineMaxColumn(last_line);
    return GetText({}, {last_line, last_line_length});
}

std::vector<string> TextEditor::GetTextLines() const {
    std::vector<string> result;
    result.reserve(Lines.size());
    for (auto &line : Lines) {
        string text;
        text.resize(line.size());
        for (size_t i = 0; i < line.size(); ++i) text[i] = line[i].Char;
        result.emplace_back(std::move(text));
    }
    return result;
}

string TextEditor::GetClipboardText() const {
    string result;
    for (int c = 0; c <= State.CurrentCursor; c++) {
        if (State.Cursors[c].GetSelectionStart() < State.Cursors[c].GetSelectionEnd()) {
            if (result.length() != 0) result += '\n';
            result += GetText(State.Cursors[c].GetSelectionStart(), State.Cursors[c].GetSelectionEnd());
        }
    }
    return result;
}

string TextEditor::GetSelectedText(int cursor) const {
    if (cursor == -1) cursor = State.CurrentCursor;
    return GetText(State.Cursors[cursor].GetSelectionStart(), State.Cursors[cursor].GetSelectionEnd());
}

string TextEditor::GetCurrentLineText() const {
    auto line_length = GetLineMaxColumn(State.Cursors[State.CurrentCursor].InteractiveEnd.Line);
    return GetText(
        Coordinates(State.Cursors[State.CurrentCursor].InteractiveEnd.Line, 0),
        Coordinates(State.Cursors[State.CurrentCursor].InteractiveEnd.Line, line_length)
    );
}

void TextEditor::Colorize(int from_line_number, int line_count) {
    int to_line = line_count == -1 ? (int)Lines.size() : std::min((int)Lines.size(), from_line_number + line_count);
    ColorRangeMin = std::min(ColorRangeMin, from_line_number);
    ColorRangeMax = std::max(ColorRangeMax, to_line);
    ColorRangeMin = std::max(0, ColorRangeMin);
    ColorRangeMax = std::max(ColorRangeMin, ColorRangeMax);
    ShouldCheckComments = true;
}

void TextEditor::ColorizeRange(int from_line_number, int to_line_number) {
    if (Lines.empty() || from_line_number >= to_line_number || LanguageDef == nullptr) return;

    string buffer, id;
    std::cmatch results;
    int end_line = std::max(0, std::min((int)Lines.size(), to_line_number));
    for (int i = from_line_number; i < end_line; ++i) {
        auto &line = Lines[i];
        if (line.empty()) continue;

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j) {
            auto &col = line[j];
            buffer[j] = col.Char;
            col.ColorIndex = PaletteIndexT::Default;
        }

        const char *bufferBegin = &buffer.front();
        const char *bufferEnd = bufferBegin + buffer.size();
        auto last = bufferEnd;
        for (auto first = bufferBegin; first != last;) {
            const char *token_begin = nullptr;
            const char *token_end = nullptr;
            PaletteIndexT token_color = PaletteIndexT::Default;
            bool has_tokenize_results = LanguageDef->Tokenize != nullptr && LanguageDef->Tokenize(first, last, token_begin, token_end, token_color);
            if (!has_tokenize_results) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);
                for (const auto &p : RegexList) {
                    if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous)) {
                        has_tokenize_results = true;

                        auto &v = *results.begin();
                        token_begin = v.first;
                        token_end = v.second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (has_tokenize_results == false) {
                first++;
            } else {
                const size_t token_length = token_end - token_begin;
                if (token_color == PaletteIndexT::Identifier) {
                    id.assign(token_begin, token_end);

                    // todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                    if (!LanguageDef->IsCaseSensitive) {
                        std::transform(id.begin(), id.end(), id.begin(), ::toupper);
                    }
                    if (!line[first - bufferBegin].IsPreprocessor) {
                        if (LanguageDef->Keywords.count(id) != 0) token_color = PaletteIndexT::Keyword;
                        else if (LanguageDef->Identifiers.count(id) != 0) token_color = PaletteIndexT::KnownIdentifier;
                        else if (LanguageDef->PreprocIdentifiers.count(id) != 0) token_color = PaletteIndexT::PreprocIdentifier;
                    } else {
                        if (LanguageDef->PreprocIdentifiers.count(id) != 0) token_color = PaletteIndexT::PreprocIdentifier;
                    }
                }

                for (size_t j = 0; j < token_length; ++j) {
                    line[(token_begin - bufferBegin) + j].ColorIndex = token_color;
                }
                first = token_end;
            }
        }
    }
}

void TextEditor::ColorizeInternal() {
    if (Lines.empty() || !ColorizerEnabled || LanguageDef == nullptr) return;

    if (ShouldCheckComments) {
        auto end_line = Lines.size();
        auto end_index = 0;
        auto comment_start_line = end_line;
        auto comment_start_index = end_index;
        auto within_string = false;
        auto within_single_line_comment = false;
        auto within_preproc = false;
        auto first_char = true; // there is no other non-whitespace characters in the line before
        auto concatenate = false; // '\' on the very end of the line
        auto current_line = 0;
        auto current_index = 0;
        while (current_line < end_line || current_index < end_index) {
            auto &line = Lines[current_line];
            if (current_index == 0 && !concatenate) {
                within_single_line_comment = false;
                within_preproc = false;
                first_char = true;
            }

            concatenate = false;
            if (!line.empty()) {
                auto &g = line[current_index];
                auto c = g.Char;
                if (c != LanguageDef->PreprocChar && !isspace(c)) first_char = false;
                if (current_index == (int)line.size() - 1 && line[line.size() - 1].Char == '\\') concatenate = true;

                bool in_comment = (comment_start_line < current_line || (comment_start_line == current_line && comment_start_index <= current_index));
                if (within_string) {
                    line[current_index].IsMultiLineComment = in_comment;
                    if (c == '\"') {
                        if (current_index + 1 < (int)line.size() && line[current_index + 1].Char == '\"') {
                            current_index += 1;
                            if (current_index < (int)line.size()) {
                                line[current_index].IsMultiLineComment = in_comment;
                            }
                        } else {
                            within_string = false;
                        }
                    } else if (c == '\\') {
                        current_index += 1;
                        if (current_index < (int)line.size()) {
                            line[current_index].IsMultiLineComment = in_comment;
                        }
                    }
                } else {
                    if (first_char && c == LanguageDef->PreprocChar) {
                        within_preproc = true;
                    }
                    if (c == '\"') {
                        within_string = true;
                        line[current_index].IsMultiLineComment = in_comment;
                    } else {
                        auto pred = [](const char &a, const Glyph &b) { return a == b.Char; };
                        auto from = line.begin() + current_index;
                        auto &start_str = LanguageDef->CommentStart;
                        auto &single_start_str = LanguageDef->SingleLineComment;
                        if (!within_single_line_comment && current_index + start_str.size() <= line.size() &&
                            equals(start_str.begin(), start_str.end(), from, from + start_str.size(), pred)) {
                            comment_start_line = current_line;
                            comment_start_index = current_index;
                        } else if (single_start_str.size() > 0 && current_index + single_start_str.size() <= line.size() && equals(single_start_str.begin(), single_start_str.end(), from, from + single_start_str.size(), pred)) {
                            within_single_line_comment = true;
                        }

                        in_comment = (comment_start_line < current_line || (comment_start_line == current_line && comment_start_index <= current_index));
                        line[current_index].IsMultiLineComment = in_comment;
                        line[current_index].IsComment = within_single_line_comment;

                        auto &end_str = LanguageDef->CommentEnd;
                        if (current_index + 1 >= (int)end_str.size() &&
                            equals(end_str.begin(), end_str.end(), from + 1 - end_str.size(), from + 1, pred)) {
                            comment_start_index = end_index;
                            comment_start_line = end_line;
                        }
                    }
                }
                if (current_index < (int)line.size()) {
                    line[current_index].IsPreprocessor = within_preproc;
                }
                current_index += UTF8CharLength(c);
                if (current_index >= (int)line.size()) {
                    current_index = 0;
                    ++current_line;
                }
            } else {
                current_index = 0;
                ++current_line;
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

float TextEditor::TextDistanceToLineStart(const Coordinates &from) const {
    auto &line = Lines[from.Line];
    float distance = 0.0f;
    float space_size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int col_index = GetCharacterIndexR(from);
    for (size_t it = 0u; it < line.size() && it < col_index;) {
        if (line[it].Char == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(TabSize) * space_size))) * (float(TabSize) * space_size);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it].Char);
            char tempCString[7];
            int i = 0;
            for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++) tempCString[i] = line[it].Char;

            tempCString[i] = '\0';
            distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
        }
    }

    return distance;
}

void TextEditor::EnsureCursorVisible(int cursor) {
    if (cursor == -1) cursor = State.GetLastAddedCursorIndex();

    if (!WithinRender) {
        ScrollToCursor = true;
        return;
    }

    float scroll_x = ImGui::GetScrollX();
    float scroll_y = ImGui::GetScrollY();

    auto height = ImGui::GetWindowHeight();
    auto width = ImGui::GetWindowWidth();

    auto top = 1 + (int)ceil(scroll_y / CharAdvance.y);
    auto bottom = (int)ceil((scroll_y + height) / CharAdvance.y);

    auto left = (int)ceil(scroll_x / CharAdvance.x);
    auto right = (int)ceil((scroll_x + width) / CharAdvance.x);

    auto pos = GetActualCursorCoordinates(cursor);
    auto len = TextDistanceToLineStart(pos);
    if (pos.Line < top) ImGui::SetScrollY(std::max(0.0f, (pos.Line - 1) * CharAdvance.y));
    if (pos.Line > bottom - 4) ImGui::SetScrollY(std::max(0.0f, (pos.Line + 4) * CharAdvance.y - height));
    if (len + TextStart < left + 4) ImGui::SetScrollX(std::max(0.0f, len + TextStart - 4));
    if (len + TextStart > right - 4) ImGui::SetScrollX(std::max(0.0f, len + TextStart + 4 - width));
}

int TextEditor::GetPageSize() const {
    auto height = ImGui::GetWindowHeight() - 20.0f; // todo should this be TextLineHeight?
    return (int)floor(height / CharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
    const std::vector<UndoOperation> &operations,
    TextEditor::EditorState &before,
    TextEditor::EditorState &after
) : Operations(operations), Before(before), After(after) {
    for (const UndoOperation &o : Operations) assert(o.Start <= o.End);
}

void TextEditor::UndoRecord::Undo(TextEditor *editor) {
    for (int i = Operations.size() - 1; i > -1; i--) {
        const auto &op = Operations[i];
        if (!op.Text.empty()) {
            switch (op.Type) {
                case UndoOperationType::Delete: {
                    auto start = op.Start;
                    editor->InsertTextAt(start, op.Text.c_str());
                    editor->Colorize(op.Start.Line - 1, op.End.Line - op.Start.Line + 2);
                    break;
                }
                case UndoOperationType::Add: {
                    editor->DeleteRange(op.Start, op.End);
                    editor->Colorize(op.Start.Line - 1, op.End.Line - op.Start.Line + 2);
                    break;
                }
            }
        }
    }

    editor->State = Before;
    editor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *editor) {
    for (int i = 0; i < Operations.size(); i++) {
        const auto &op = Operations[i];
        if (!op.Text.empty()) {
            switch (op.Type) {
                case UndoOperationType::Delete: {
                    editor->DeleteRange(op.Start, op.End);
                    editor->Colorize(op.Start.Line - 1, op.End.Line - op.Start.Line + 1);
                    break;
                }
                case UndoOperationType::Add: {
                    auto start = op.Start;
                    editor->InsertTextAt(start, op.Text.c_str());
                    editor->Colorize(op.Start.Line - 1, op.End.Line - op.Start.Line + 1);
                    break;
                }
            }
        }
    }

    editor->State = After;
    editor->EnsureCursorVisible();
}
