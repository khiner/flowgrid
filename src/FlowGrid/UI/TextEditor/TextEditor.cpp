#include "TextEditor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <regex>
#include <string>

#include "imgui.h"

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
    : LineSpacing(1.0f), UndoIndex(0), TabSize(4), Overwrite(false), ReadOnly(false), WithinRender(false), ScrollToCursor(false), ScrollToTop(false), TextChanged(false), ColorizerEnabled(true), TextStart(20.0f), LeftMargin(10), ColorRangeMin(0), ColorRangeMax(0), SelectionMode(SelectionModeT::Normal), ShouldCheckComments(true), ShouldHandleKeyboardInputs(true), ShouldHandleMouseInputs(true), IgnoreImGuiChild(false), ShowWhitespaces(true), ShowShortTabGlyphs(false), StartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), LastClickTime(-1.0f) {
    SetPalette(GetMarianaPalette());
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
    return SanitizeCoordinates(EditorState.Cursors[cursor == -1 ? EditorState.CurrentCursor : cursor].CursorPosition);
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

void TextEditor::Advance(Coordinates &coords) const {
    if (coords.Line >= (int)Lines.size()) return;

    auto &line = Lines[coords.Line];
    auto cindex = GetCharacterIndexL(coords);
    if (cindex < (int)line.size()) {
        auto delta = UTF8CharLength(line[cindex].Char);
        cindex = std::min(cindex + delta, (int)line.size());
    } else if (Lines.size() > coords.Line + 1) {
        ++coords.Line;
        cindex = 0;
    }
    coords.Column = GetCharacterColumn(coords.Line, cindex);
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
        if (start.Line < end.Line) AddGlyphsToLine(start.Line, first_line.size(), last_line.begin(), last_line.end());
        if (start.Line < end.Line) RemoveLines(start.Line + 1, end.Line + 1);
    }

    TextChanged = true;
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

        TextChanged = true;
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
        if ((c & 0xC0) != 0x80) // not UTF code sequence 10xxxxxx
        {
            bool is_word_char = IsGlyphWordChar(line[cindex]);
            bool is_space = isspace(line[cindex].Char);
            if ((initial_is_space && !is_space) || (initial_is_word_char && !is_word_char) || (!initial_is_word_char && !initial_is_space && initial_char != line[cindex].Char)) {
                need_to_advance = true;
                break;
            }
        }
    }
    at.Column = GetCharacterColumn(at.Line, cindex);
    if (need_to_advance) Advance(at);

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

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates &from) const {
    Coordinates at = from;
    if (at.Line >= (int)Lines.size()) return at;

    // skip to the next non-word character
    auto cindex = GetCharacterIndexR(from);
    bool isword = false;
    bool skip = false;
    if (cindex < (int)Lines[at.Line].size()) {
        auto &line = Lines[at.Line];
        isword = !!isalnum(line[cindex].Char);
        skip = isword;
    }

    while (!isword || skip) {
        if (at.Line >= Lines.size()) {
            auto l = std::max(0, (int)Lines.size() - 1);
            return Coordinates(l, GetLineMaxColumn(l));
        }

        auto &line = Lines[at.Line];
        if (cindex < (int)line.size()) {
            isword = isalnum(line[cindex].Char);
            if (isword && !skip) return Coordinates(at.Line, GetCharacterColumn(at.Line, cindex));

            if (!isword) skip = false;
            cindex++;
        } else {
            cindex = 0;
            ++at.Line;
            skip = false;
            isword = false;
        }
    }

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

int TextEditor::GetLineCharacterCount(int line_number) const {
    if (line_number >= Lines.size()) return 0;

    auto &line = Lines[line_number];
    int c = 0;
    for (unsigned i = 0; i < line.size(); c++) i += UTF8CharLength(line[i].Char);
    return c;
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

bool TextEditor::IsOnWordBoundary(const Coordinates &at) const {
    if (at.Line >= (int)Lines.size() || at.Column == 0) return true;

    auto &line = Lines[at.Line];
    auto cindex = GetCharacterIndexR(at);
    if (cindex >= (int)line.size()) return true;
    if (ColorizerEnabled) return line[cindex].ColorIndex != line[size_t(cindex - 1)].ColorIndex;

    return isspace(line[cindex].Char) != isspace(line[cindex - 1].Char);
}

void TextEditor::RemoveLines(int start, int end) {
    assert(!ReadOnly);
    assert(end >= start);
    assert(Lines.size() > (size_t)(end - start));

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers) {
        ErrorMarkersT::value_type e(i.first >= start ? i.first - 1 : i.first, i.second);
        if (e.first < start || e.first > end) etmp.insert(e);
    }
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints) {
        if (i < start || i > end) btmp.insert(i >= start ? i - 1 : i);
    }
    Breakpoints = std::move(btmp);
    Lines.erase(Lines.begin() + start, Lines.begin() + end);
    assert(!Lines.empty());

    TextChanged = true;
    OnLinesDeleted(start, end);
}

void TextEditor::RemoveLine(int line_number, const std::unordered_set<int> *handled_cursors) {
    assert(!ReadOnly);
    assert(Lines.size() > 1);

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers) {
        ErrorMarkersT::value_type e(i.first > line_number ? i.first - 1 : i.first, i.second);
        if (e.first - 1 != line_number) etmp.insert(e);
    }
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints) {
        if (i != line_number) btmp.insert(i >= line_number ? i - 1 : i);
    }
    Breakpoints = std::move(btmp);

    Lines.erase(Lines.begin() + line_number);
    assert(!Lines.empty());

    TextChanged = true;
    OnLineDeleted(line_number, handled_cursors);
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    }

    for (int c = EditorState.CurrentCursor; c > -1; c--) {
        int current_line = EditorState.Cursors[c].CursorPosition.Line;
        int next_line = current_line + 1;
        int prev_line = current_line - 1;

        Coordinates to_delete_start, to_delete_end;
        if (Lines.size() > next_line) // next line exists
        {
            to_delete_start = Coordinates(current_line, 0);
            to_delete_end = Coordinates(next_line, 0);
            SetCursorPosition({EditorState.Cursors[c].CursorPosition.Line, 0}, c);
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

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::OnLineChanged(bool before_change, int line_number, int column, int char_count, bool is_deleted) {
    static std::unordered_map<int, int> cursor_char_indices;
    if (before_change) {
        cursor_char_indices.clear();
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.Line == line_number) {
                if (EditorState.Cursors[c].CursorPosition.Column > column) {
                    cursor_char_indices[c] = GetCharacterIndexR({line_number, EditorState.Cursors[c].CursorPosition.Column});
                    cursor_char_indices[c] += is_deleted ? -char_count : char_count;
                }
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

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers) etmp.insert(ErrorMarkersT::value_type(i.first >= line_number ? i.first + 1 : i.first, i.second));
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints) btmp.insert(i >= line_number ? i + 1 : i);
    Breakpoints = std::move(btmp);

    OnLineAdded(line_number);

    return result;
}

string TextEditor::GetWordUnderCursor() const { return GetWordAt(GetCursorPosition()); }

string TextEditor::GetWordAt(const Coordinates &coords) const {
    auto start = FindWordStart(coords);
    auto end = FindWordEnd(coords);

    string r;
    auto start_char_index = GetCharacterIndexR(start);
    auto end_char_index = GetCharacterIndexR(end);
    for (auto it = start_char_index; it < end_char_index; ++it) {
        r.push_back(Lines[coords.Line][it].Char);
    }

    return r;
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

void TextEditor::HandleKeyboardInputs(bool is_parent_focused) {
    if (ImGui::IsWindowFocused() || is_parent_focused) {
        if (ImGui::IsWindowHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        // ImGui::CaptureKeyboardFromApp(true);

        ImGuiIO &io = ImGui::GetIO();
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

        if (!ReadOnly && is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
            Undo();
        else if (!ReadOnly && is_alt_only && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
            Undo();
        else if (!ReadOnly && is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Y)))
            Redo();
        else if (!ReadOnly && is_shift_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
            Redo();
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
            MoveUp(1, shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
            MoveDown(1, shift);
        else if ((is_osx ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
            MoveLeft(1, shift, is_wordmove_key);
        else if ((is_osx ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
            MoveRight(1, shift, is_wordmove_key);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
            MoveUp(GetPageSize() - 4, shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
            MoveDown(GetPageSize() - 4, shift);
        else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
            MoveTop(shift);
        else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
            MoveBottom(shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
            MoveHome(shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
            MoveEnd(shift);
        else if (!ReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
            Delete(ctrl);
        else if (!ReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
            Backspace(ctrl);
        else if (!ReadOnly && !alt && ctrl && shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_K)))
            RemoveCurrentLines();
        else if (!ReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftBracket)))
            ChangeCurrentLinesIndentation(false);
        else if (!ReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightBracket)))
            ChangeCurrentLinesIndentation(true);
        else if (!alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Overwrite ^= true;
        else if (is_ctrl_only && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Copy();
        else if (is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C)))
            Copy();
        else if (!ReadOnly && is_shift_only && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Paste();
        else if (!ReadOnly && is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)))
            Paste();
        else if (is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X)))
            Cut();
        else if (is_shift_only && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
            Cut();
        else if (is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
            SelectAll();
        else if (is_shortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_D)))
            AddCursorForNextOccurrence();
        else if (!ReadOnly && !alt && !ctrl && !shift && !super && (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_KeypadEnter))))
            EnterCharacter('\n', false);
        else if (!ReadOnly && !alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
            EnterCharacter('\t', shift);
        if (!ReadOnly && !io.InputQueueCharacters.empty() && !ctrl && !super) {
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
    if (ImGui::IsWindowHovered()) {
        auto click = ImGui::IsMouseClicked(0);
        if (!shift && !alt) {
            bool is_double_click = ImGui::IsMouseDoubleClicked(0);
            auto t = ImGui::GetTime();
            bool is_triple_click = click && !is_double_click && (LastClickTime != -1.0f && (t - LastClickTime) < io.MouseDoubleClickTime);

            if (is_triple_click) {
                if (ctrl) EditorState.AddCursor();
                else EditorState.CurrentCursor = 0;

                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                SelectionMode = SelectionModeT::Line;
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);

                LastClickTime = -1.0f;
            } else if (is_double_click) {
                if (ctrl) EditorState.AddCursor();
                else EditorState.CurrentCursor = 0;

                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = FindWordStart(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition);
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = FindWordEnd(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition);
                SelectionMode = SelectionMode == SelectionModeT::Line ? SelectionModeT::Normal : SelectionModeT::Word;
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);

                LastClickTime = (float)ImGui::GetTime();
            } else if (click) {
                if (ctrl) EditorState.AddCursor();
                else EditorState.CurrentCursor = 0;

                bool is_over_line_number;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite, &is_over_line_number);
                SelectionMode = is_over_line_number ? SelectionModeT::Line : (ctrl ? SelectionModeT::Word : SelectionModeT::Normal);
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode, -1, ctrl);

                LastClickTime = (float)ImGui::GetTime();
            } else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                IsDraggingSelection = true;
                io.WantCaptureMouse = true;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);
            } else if (ImGui::IsMouseReleased(0)) {
                IsDraggingSelection = false;
                EditorState.SortCursorsFromTopToBottom();
                MergeCursorsIfPossible();
            }
        } else if (shift && click) {
            Coordinates oldCursorPosition = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
            Coordinates newSelection = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
            if (newSelection > EditorState.Cursors[EditorState.CurrentCursor].CursorPosition) SetSelectionEnd(newSelection);
            else SetSelectionStart(newSelection);

            EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = EditorState.Cursors[EditorState.CurrentCursor].SelectionEnd;
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].SelectionStart;
            EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = newSelection;
            EditorState.Cursors[EditorState.CurrentCursor].CursorPositionChanged = oldCursorPosition != newSelection;
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
    float longest(TextStart);

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
            longest = std::max(TextStart + TextDistanceToLineStart(Coordinates(line_number, GetLineMaxColumn(line_number))), longest);
            Coordinates line_start_coord(line_number, 0);
            Coordinates line_end_coord(line_number, GetLineMaxColumn(line_number));
            // Draw selection for the current line
            for (int c = 0; c <= EditorState.CurrentCursor; c++) {
                float sstart = -1.0f;
                float ssend = -1.0f;
                assert(EditorState.Cursors[c].SelectionStart <= EditorState.Cursors[c].SelectionEnd);
                if (EditorState.Cursors[c].SelectionStart <= line_end_coord) {
                    sstart = EditorState.Cursors[c].SelectionStart > line_start_coord ? TextDistanceToLineStart(EditorState.Cursors[c].SelectionStart) : 0.0f;
                }
                if (EditorState.Cursors[c].SelectionEnd > line_start_coord) {
                    ssend = TextDistanceToLineStart(EditorState.Cursors[c].SelectionEnd < line_end_coord ? EditorState.Cursors[c].SelectionEnd : line_end_coord);
                }
                if (EditorState.Cursors[c].SelectionEnd.Line > line_number) {
                    ssend += CharAdvance.x;
                }

                if (sstart != -1 && ssend != -1 && sstart < ssend) {
                    ImVec2 vstart(line_start_screen_pos.x + TextStart + sstart, line_start_screen_pos.y);
                    ImVec2 vend(line_start_screen_pos.x + TextStart + ssend, line_start_screen_pos.y + CharAdvance.y);
                    dl->AddRectFilled(vstart, vend, Pallette[(int)PaletteIndexT::Selection]);
                }
            }

            // Draw breakpoints
            auto start = ImVec2(line_start_screen_pos.x + scroll_x, line_start_screen_pos.y);
            if (Breakpoints.count(line_number + 1) != 0) {
                auto end = ImVec2(line_start_screen_pos.x + content_size.x + 2.0f * scroll_x, line_start_screen_pos.y + CharAdvance.y);
                dl->AddRectFilled(start, end, Pallette[(int)PaletteIndexT::Breakpoint]);
            }

            // Draw error markers
            auto error_it = ErrorMarkers.find(line_number + 1);
            if (error_it != ErrorMarkers.end()) {
                auto end = ImVec2(line_start_screen_pos.x + content_size.x + 2.0f * scroll_x, line_start_screen_pos.y + CharAdvance.y);
                dl->AddRectFilled(start, end, Pallette[(int)PaletteIndexT::ErrorMarker]);

                if (ImGui::IsMouseHoveringRect(line_start_screen_pos, end)) {
                    ImGui::BeginTooltip();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    ImGui::Text("Error at line %d:", error_it->first);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
                    ImGui::Text("%s", error_it->second.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndTooltip();
                }
            }

            // Draw line number (right aligned)
            snprintf(buf, 16, "%d  ", line_number + 1);

            auto line_numberWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
            dl->AddText(ImVec2(line_start_screen_pos.x + TextStart - line_numberWidth, line_start_screen_pos.y), Pallette[(int)PaletteIndexT::LineNumber], buf);

            std::vector<Coordinates> cursor_coords_in_this_line;
            for (int c = 0; c <= EditorState.CurrentCursor; c++) {
                if (EditorState.Cursors[c].CursorPosition.Line == line_number) {
                    cursor_coords_in_this_line.push_back(EditorState.Cursors[c].CursorPosition);
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
                        const float x2 = text_screen_pos.x + (ShowShortTabGlyphs ? old_x + CharAdvance.x : buffer_offset.x) - 1.0f;
                        const float gap = s * (ShowShortTabGlyphs ? 0.16 : 0.2);
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
                        dl->AddCircleFilled({x, y}, 1.5f, 0x80808080, 4);
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

        // Draw a tooltip on known identifiers/preprocessor symbols
        if (ImGui::IsMousePosValid() && ImGui::IsWindowHovered() && LanguageDef != nullptr) {
            auto mpos = ImGui::GetMousePos();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImVec2 local(mpos.x - origin.x, mpos.y - origin.y);
            if (local.x >= TextStart) {
                auto pos = ScreenPosToCoordinates(mpos);
                auto id = GetWordAt(pos);
                if (!id.empty()) {
                    auto it = LanguageDef->Identifiers.find(id);
                    if (it != LanguageDef->Identifiers.end()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(it->second.Declaration.c_str());
                        ImGui::EndTooltip();
                    } else {
                        auto pi = LanguageDef->PreprocIdentifiers.find(id);
                        if (pi != LanguageDef->PreprocIdentifiers.end()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(pi->second.Declaration.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
            }
        }
    }

    ImGui::SetCursorPos({0, 0});
    ImGui::Dummy({(longest + 2), Lines.size() * CharAdvance.y});
    if (ScrollToCursor) {
        EnsureCursorVisible();
        ScrollToCursor = false;
    }
}

bool TextEditor::FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out) {
    assert(text_size > 0);
    for (int i = 0; i < Lines.size(); i++) {
        int current_line = (from.Line + i) % Lines.size();
        int line_start_index = i == 0 ? GetCharacterIndexR(from) : 0;
        int text_index = 0;
        int j = line_start_index;
        for (; j < Lines[current_line].size(); j++) {
            if (text_index == text_size || text[text_index] == '\0') break;
            text_index = text[text_index] == Lines[current_line][j].Char ? text_index + 1 : 0;
        }
        if (text_index == text_size || text[text_index] == '\0') {
            if (text[text_index] == '\0') text_size = text_index;
            start_out = {current_line, GetCharacterColumn(current_line, j - text_size)};
            end_out = {current_line, GetCharacterColumn(current_line, j)};
            return true;
        }
    }
    // in line where we started again but from char index 0 to from.Column
    {
        int text_index = 0;
        int j = 0;
        for (; j < GetCharacterIndexR(from); j++) {
            if (text_index == text_size || text[text_index] == '\0') break;
            text_index = text[text_index] == Lines[from.Line][j].Char ? text_index + 1 : 0;
        }
        if (text_index == text_size || text[text_index] == '\0') {
            if (text[text_index] == '\0') text_size = text_index;
            start_out = {from.Line, GetCharacterColumn(from.Line, j - text_size)};
            end_out = {from.Line, GetCharacterColumn(from.Line, j)};
            return true;
        }
    }
    return false;
}

bool TextEditor::Render(const char *title, bool is_parent_focused, const ImVec2 &size, bool border) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        if (EditorState.Cursors[c].CursorPositionChanged) OnCursorPositionChanged();
        if (c <= EditorState.CurrentCursor) EditorState.Cursors[c].CursorPositionChanged = false;
    }

    WithinRender = true;
    TextChanged = false;

    UpdatePalette();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(Pallette[(int)PaletteIndexT::Background]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!IgnoreImGuiChild) {
        ImGui::BeginChild(title, size, border, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);
    }

    bool is_focused = ImGui::IsWindowFocused();
    if (ShouldHandleKeyboardInputs) {
        HandleKeyboardInputs(is_parent_focused);
        ImGui::PushAllowKeyboardFocus(true);
    }

    if (ShouldHandleMouseInputs) HandleMouseInputs();

    ColorizeInternal();
    Render(is_parent_focused);

    if (ShouldHandleKeyboardInputs) ImGui::PopAllowKeyboardFocus();
    if (!IgnoreImGuiChild) ImGui::EndChild();

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

    TextChanged = true;
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

    TextChanged = true;
    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::ChangeCurrentLinesIndentation(bool increase) {
    assert(!ReadOnly);

    UndoRecord u;
    u.Before = EditorState;

    for (int c = EditorState.CurrentCursor; c > -1; c--) {
        auto start = EditorState.Cursors[c].SelectionStart;
        auto end = EditorState.Cursors[c].SelectionEnd;
        auto original_end = end;
        if (start > end) std::swap(start, end);

        start.Column = 0;
        //			end.Column = end.Line < Lines.size() ? Lines[end.Line].size() : 0;
        if (end.Column == 0 && end.Line > 0) --end.Line;
        if (end.Line >= (int)Lines.size()) end.Line = Lines.empty() ? 0 : (int)Lines.size() - 1;
        end.Column = GetLineMaxColumn(end.Line);

        // if (end.Column >= GetLineMaxColumn(end.Line))
        //	end.Column = GetLineMaxColumn(end.Line) - 1;

        UndoOperation remove_op = {GetText(start, end), start, end, UndoOperationType::Delete};

        bool modified = false;
        for (int i = start.Line; i <= end.Line; i++) {
            auto &line = Lines[i];
            if (!increase) {
                if (!line.empty()) {
                    if (line.front().Char == '\t') {
                        RemoveGlyphsFromLine(i, 0, 1);
                        modified = true;
                    } else {
                        for (int j = 0; j < TabSize && !line.empty() && line.front().Char == ' '; j++) {
                            RemoveGlyphsFromLine(i, 0, 1);
                            modified = true;
                        }
                    }
                }
            } else if (Lines[i].size() > 0) {
                AddGlyphToLine(i, 0, Glyph('\t', TextEditor::PaletteIndexT::Background));
                modified = true;
            }
        }

        if (modified) {
            start = Coordinates(start.Line, GetCharacterColumn(start.Line, 0));
            Coordinates range_end;
            string added_text;
            if (original_end.Column != 0) {
                end = Coordinates(end.Line, GetLineMaxColumn(end.Line));
                range_end = end;
                added_text = GetText(start, end);
            } else {
                end = Coordinates(original_end.Line, 0);
                range_end = Coordinates(end.Line - 1, GetLineMaxColumn(end.Line - 1));
                added_text = GetText(start, range_end);
            }

            u.Operations.push_back(remove_op);
            u.Operations.push_back({added_text, start, range_end, UndoOperationType::Add});
            u.After = EditorState;

            EditorState.Cursors[c].SelectionStart = start;
            EditorState.Cursors[c].SelectionEnd = end;

            TextChanged = true;
        }
    }

    EnsureCursorVisible();
    if (u.Operations.size() > 0) AddUndo(u);
}

void TextEditor::EnterCharacter(ImWchar character, bool is_shift) {
    assert(!ReadOnly);

    bool has_selection = HasSelection();
    bool any_cursor_has_multiline_selection = false;
    for (int c = EditorState.CurrentCursor; c > -1; c--) {
        if (EditorState.Cursors[c].SelectionStart.Line != EditorState.Cursors[c].SelectionEnd.Line) {
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
    u.Before = EditorState;
    if (has_selection) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    }

    std::vector<Coordinates> coords;
    for (int c = EditorState.CurrentCursor; c > -1; c--) // order important here for typing \n in the same line at the same time
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
            if (LanguageDef != nullptr && LanguageDef->AudioIndentation) {
                for (size_t it = 0; it < line.size() && isascii(line[it].Char) && isblank(line[it].Char); ++it) {
                    new_line.push_back(line[it]);
                    added.Text += line[it].Char;
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
                    removed.Start = EditorState.Cursors[c].CursorPosition;
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

        TextChanged = true;

        added.End = GetActualCursorCoordinates(c);
        u.Operations.push_back(added);
    }

    u.After = EditorState;
    AddUndo(u);

    for (const auto &coord : coords) Colorize(coord.Line - 1, 3);
    EnsureCursorVisible();
}

void TextEditor::OnCursorPositionChanged() {
    if (IsDraggingSelection) return;

    EditorState.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
}

void TextEditor::SetCursorPosition(const Coordinates &position, int cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    if (EditorState.Cursors[cursor].CursorPosition != position) {
        EditorState.Cursors[cursor].CursorPosition = position;
        EditorState.Cursors[cursor].CursorPositionChanged = true;
        EnsureCursorVisible();
    }
}

void TextEditor::SetCursorPosition(int line_number, int characterIndex, int cursor) {
    SetCursorPosition({line_number, GetCharacterColumn(line_number, characterIndex)}, cursor);
}

void TextEditor::SetSelectionStart(const Coordinates &position, int cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    EditorState.Cursors[cursor].SelectionStart = SanitizeCoordinates(position);
    if (EditorState.Cursors[cursor].SelectionStart > EditorState.Cursors[cursor].SelectionEnd)
        std::swap(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates &position, int cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    EditorState.Cursors[cursor].SelectionEnd = SanitizeCoordinates(position);
    if (EditorState.Cursors[cursor].SelectionStart > EditorState.Cursors[cursor].SelectionEnd)
        std::swap(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionEnd);
}

void TextEditor::SetSelection(const Coordinates &start, const Coordinates &end, SelectionModeT mode, int cursor, bool is_spawning_new_cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    auto ols_sel_start = EditorState.Cursors[cursor].SelectionStart;
    auto old_sel_end = EditorState.Cursors[cursor].SelectionEnd;

    EditorState.Cursors[cursor].SelectionStart = SanitizeCoordinates(start);
    EditorState.Cursors[cursor].SelectionEnd = SanitizeCoordinates(end);
    if (EditorState.Cursors[cursor].SelectionStart > EditorState.Cursors[cursor].SelectionEnd) {
        std::swap(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionEnd);
    }

    switch (mode) {
        case TextEditor::SelectionModeT::Normal:
        case TextEditor::SelectionModeT::Word:
            break;
        case TextEditor::SelectionModeT::Line: {
            const auto line_number = EditorState.Cursors[cursor].SelectionEnd.Line;
            EditorState.Cursors[cursor].SelectionStart = Coordinates(EditorState.Cursors[cursor].SelectionStart.Line, 0);
            EditorState.Cursors[cursor].SelectionEnd = Lines.size() > line_number + 1 ? Coordinates(line_number + 1, 0) : Coordinates(line_number, GetLineMaxColumn(line_number));
            EditorState.Cursors[cursor].CursorPosition = EditorState.Cursors[cursor].SelectionEnd;
            break;
        }
        default:
            break;
    }

    if ((EditorState.Cursors[cursor].SelectionStart != ols_sel_start || EditorState.Cursors[cursor].SelectionEnd != old_sel_end) && !is_spawning_new_cursor) {
        EditorState.Cursors[cursor].CursorPositionChanged = true;
    }
}

void TextEditor::SetSelection(int start_line_number, int start_char_index, int end_line_number, int end_char_indexIndex, SelectionModeT mode, int cursor, bool is_spawning_new_cursor) {
    SetSelection(
        {start_line_number, GetCharacterColumn(start_line_number, start_char_index)},
        {end_line_number, GetCharacterColumn(end_line_number, end_char_indexIndex)},
        mode, cursor, is_spawning_new_cursor
    );
}

void TextEditor::SetTabSize(int tab_size) {
    TabSize = std::max(0, std::min(32, tab_size));
}

void TextEditor::InsertText(const string &text, int cursor) {
    InsertText(text.c_str(), cursor);
}

void TextEditor::InsertText(const char *text, int cursor) {
    if (text == nullptr) return;

    if (cursor == -1) cursor = EditorState.CurrentCursor;

    auto pos = GetActualCursorCoordinates(cursor);
    auto start = std::min(pos, EditorState.Cursors[cursor].SelectionStart);
    int total_lines = pos.Line - start.Line + InsertTextAt(pos, text);
    SetSelection(pos, pos, SelectionModeT::Normal, cursor);
    SetCursorPosition(pos, cursor);
    Colorize(start.Line - 1, total_lines + 2);
}

void TextEditor::DeleteSelection(int cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    assert(EditorState.Cursors[cursor].SelectionEnd >= EditorState.Cursors[cursor].SelectionStart);
    if (EditorState.Cursors[cursor].SelectionEnd == EditorState.Cursors[cursor].SelectionStart) return;

    DeleteRange(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionEnd);

    SetSelection(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionStart, SelectionModeT::Normal, cursor);
    SetCursorPosition(EditorState.Cursors[cursor].SelectionStart, cursor);
    EditorState.Cursors[cursor].InteractiveStart = EditorState.Cursors[cursor].SelectionStart;
    EditorState.Cursors[cursor].InteractiveEnd = EditorState.Cursors[cursor].SelectionEnd;
    Colorize(EditorState.Cursors[cursor].SelectionStart.Line, 1);
}

void TextEditor::MoveUp(int amount, bool select) {
    if (HasSelection() && !select) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionStart, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionStart);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto old_pos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition.Line = std::max(0, EditorState.Cursors[c].CursorPosition.Line - amount);
            if (old_pos != EditorState.Cursors[c].CursorPosition) {
                if (select) {
                    if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    } else if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    } else {
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                        EditorState.Cursors[c].InteractiveEnd = old_pos;
                    }
                } else {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                }
                SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
            }
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveDown(int amount, bool select) {
    if (HasSelection() && !select) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionEnd, EditorState.Cursors[c].SelectionEnd, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionEnd);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            assert(EditorState.Cursors[c].CursorPosition.Column >= 0);
            auto old_pos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition.Line = std::max(0, std::min((int)Lines.size() - 1, EditorState.Cursors[c].CursorPosition.Line + amount));

            if (EditorState.Cursors[c].CursorPosition != old_pos) {
                if (select) {
                    if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    } else if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    } else {
                        EditorState.Cursors[c].InteractiveStart = old_pos;
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    }
                } else {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                }
                SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
            }
        }
    }
    EnsureCursorVisible();
}

static bool IsUTFSequence(char c) { return (c & 0xC0) == 0x80; }

void TextEditor::MoveLeft(int amount, bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    if (HasSelection() && !select && !is_word_mode) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionStart, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionStart);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto old_pos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition = GetActualCursorCoordinates(c);
            auto line = EditorState.Cursors[c].CursorPosition.Line;
            auto cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);
            while (amount-- > 0) {
                if (cindex == 0) {
                    if (line > 0) {
                        --line;
                        cindex = (int)Lines.size() > line ? (int)Lines[line].size() : 0;
                    }
                } else {
                    --cindex;
                    if (cindex > 0) {
                        if ((int)Lines.size() > line) {
                            while (cindex > 0 && IsUTFSequence(Lines[line][cindex].Char)) --cindex;
                        }
                    }
                }

                EditorState.Cursors[c].CursorPosition = {line, GetCharacterColumn(line, cindex)};
                if (is_word_mode) {
                    EditorState.Cursors[c].CursorPosition = FindWordStart(EditorState.Cursors[c].CursorPosition);
                    cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);
                }
            }

            EditorState.Cursors[c].CursorPosition = {line, GetCharacterColumn(line, cindex)};

            assert(EditorState.Cursors[c].CursorPosition.Column >= 0);
            if (select) {
                if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                } else if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                    EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                } else {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    EditorState.Cursors[c].InteractiveEnd = old_pos;
                }
            } else {
                if (HasSelection() && !is_word_mode) {
                    EditorState.Cursors[c].CursorPosition = EditorState.Cursors[c].InteractiveStart;
                }
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
            SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, select && is_word_mode ? SelectionModeT::Word : SelectionModeT::Normal, c);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(int amount, bool select, bool is_word_mode) {
    if (Lines.empty()) return;

    if (HasSelection() && !select && !is_word_mode) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionEnd, EditorState.Cursors[c].SelectionEnd, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionEnd);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto old_pos = EditorState.Cursors[c].CursorPosition;
            if (old_pos.Line >= Lines.size()) continue;

            auto cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);
            while (amount-- > 0) {
                auto lindex = EditorState.Cursors[c].CursorPosition.Line;
                auto &line = Lines[lindex];

                if (cindex >= line.size()) {
                    if (EditorState.Cursors[c].CursorPosition.Line < Lines.size() - 1) {
                        EditorState.Cursors[c].CursorPosition.Line = std::max(0, std::min((int)Lines.size() - 1, EditorState.Cursors[c].CursorPosition.Line + 1));
                        EditorState.Cursors[c].CursorPosition.Column = 0;
                    } else continue;
                } else {
                    cindex += UTF8CharLength(line[cindex].Char);
                    EditorState.Cursors[c].CursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
                    if (is_word_mode) {
                        EditorState.Cursors[c].CursorPosition = FindWordEnd(EditorState.Cursors[c].CursorPosition);
                    }
                }
            }

            if (select) {
                if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                    EditorState.Cursors[c].InteractiveEnd = SanitizeCoordinates(EditorState.Cursors[c].CursorPosition);
                } else if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                } else {
                    EditorState.Cursors[c].InteractiveStart = old_pos;
                    EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                }
            } else {
                if (HasSelection() && !is_word_mode) {
                    EditorState.Cursors[c].CursorPosition = EditorState.Cursors[c].InteractiveEnd;
                }
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
            SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, select && is_word_mode ? SelectionModeT::Word : SelectionModeT::Normal, c);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool select) {
    EditorState.CurrentCursor = 0;
    auto old_pos = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
    SetCursorPosition(Coordinates(0, 0));

    if (EditorState.Cursors[EditorState.CurrentCursor].CursorPosition != old_pos) {
        if (select) {
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = old_pos;
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
        } else {
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
        }
        SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd);
    }
}

void TextEditor::TextEditor::MoveBottom(bool select) {
    EditorState.CurrentCursor = 0;
    auto old_pos = GetCursorPosition();
    auto new_pos = Coordinates((int)Lines.size() - 1, 0);
    SetCursorPosition(new_pos);
    if (select) {
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = old_pos;
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = new_pos;
    } else {
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = new_pos;
    }
    SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd);
}

void TextEditor::MoveHome(bool select) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        auto old_pos = EditorState.Cursors[c].CursorPosition;
        SetCursorPosition(Coordinates(EditorState.Cursors[c].CursorPosition.Line, 0), c);

        if (select) {
            if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
            } else if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            } else {
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                EditorState.Cursors[c].InteractiveEnd = old_pos;
            }
        } else {
            EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
        }
        SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
    }
}

void TextEditor::MoveEnd(bool select) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        auto old_pos = EditorState.Cursors[c].CursorPosition;
        SetCursorPosition(Coordinates(EditorState.Cursors[c].CursorPosition.Line, GetLineMaxColumn(old_pos.Line)), c);

        if (select) {
            if (old_pos == EditorState.Cursors[c].InteractiveEnd) {
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            } else if (old_pos == EditorState.Cursors[c].InteractiveStart) {
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
            } else {
                EditorState.Cursors[c].InteractiveStart = old_pos;
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
        } else {
            EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
        }
        SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
    }
}

void TextEditor::Delete(bool is_word_mode) {
    assert(!ReadOnly);
    if (Lines.empty()) return;

    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    } else {
        std::vector<Coordinates> positions;
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto pos = GetActualCursorCoordinates(c);
            positions.push_back(pos);
            SetCursorPosition(pos, c);
            auto &line = Lines[pos.Line];
            if (pos.Column == GetLineMaxColumn(pos.Line)) {
                if (pos.Line == (int)Lines.size() - 1) continue;

                Coordinates start_coords = GetActualCursorCoordinates(c);
                Coordinates end_coords = start_coords;
                Advance(end_coords);
                u.Operations.push_back({"\n", start_coords, end_coords, UndoOperationType::Delete});

                auto &next_line = Lines[pos.Line + 1];
                AddGlyphsToLine(pos.Line, line.size(), next_line.begin(), next_line.end());
                for (int other_cursor = c + 1;
                     other_cursor <= EditorState.CurrentCursor && EditorState.Cursors[other_cursor].CursorPosition.Line == pos.Line + 1;
                     other_cursor++) // move up cursors in next line
                {
                    int other_cursor_char_index = GetCharacterIndexR(EditorState.Cursors[other_cursor].CursorPosition);
                    int other_cursor_new_char_index = GetCharacterIndexR(pos) + other_cursor_char_index;
                    auto target_coords = Coordinates(pos.Line, GetCharacterColumn(pos.Line, other_cursor_new_char_index));
                    SetCursorPosition(target_coords, other_cursor);
                }
                RemoveLine(pos.Line + 1);
            } else {
                if (is_word_mode) {
                    Coordinates end = FindWordEnd(EditorState.Cursors[c].CursorPosition);
                    u.Operations.push_back({GetText(EditorState.Cursors[c].CursorPosition, end), EditorState.Cursors[c].CursorPosition, end, UndoOperationType::Delete});
                    DeleteRange(EditorState.Cursors[c].CursorPosition, end);
                } else {
                    auto cindex = GetCharacterIndexR(pos);

                    Coordinates start = GetActualCursorCoordinates(c);
                    Coordinates end = start;
                    end.Column++;
                    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});

                    auto d = UTF8CharLength(line[cindex].Char);
                    while (d-- > 0 && cindex < (int)line.size()) {
                        RemoveGlyphsFromLine(pos.Line, cindex, cindex + 1);
                    }
                }
            }
        }

        TextChanged = true;
        for (const auto &pos : positions) Colorize(pos.Line, 1);
    }

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::Backspace(bool is_word_mode) {
    assert(!ReadOnly);
    if (Lines.empty()) return;

    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto pos = GetActualCursorCoordinates(c);
            SetCursorPosition(pos, c);

            if (EditorState.Cursors[c].CursorPosition.Column == 0) {
                if (EditorState.Cursors[c].CursorPosition.Line == 0) continue;

                Coordinates start_coords = {pos.Line - 1, GetLineMaxColumn(pos.Line - 1)};
                Coordinates end_coords = start_coords;
                Advance(end_coords);
                u.Operations.push_back({"\n", start_coords, end_coords, UndoOperationType::Delete});

                auto &line = Lines[EditorState.Cursors[c].CursorPosition.Line];
                int prev_line_index = EditorState.Cursors[c].CursorPosition.Line - 1;
                auto &prev_line = Lines[prev_line_index];
                auto prev_size = GetLineMaxColumn(prev_line_index);
                AddGlyphsToLine(prev_line_index, prev_line.size(), line.begin(), line.end());
                std::unordered_set<int> cursorsHandled = {c};
                for (int other_cursor = c + 1;
                     other_cursor <= EditorState.CurrentCursor && EditorState.Cursors[other_cursor].CursorPosition.Line == EditorState.Cursors[c].CursorPosition.Line;
                     other_cursor++) // move up cursors in same line
                {
                    int other_cursor_char_index = GetCharacterIndexR(EditorState.Cursors[other_cursor].CursorPosition);
                    int other_cursor_new_char_index = GetCharacterIndexR({prev_line_index, prev_size}) + other_cursor_char_index;
                    auto target_coords = Coordinates(prev_line_index, GetCharacterColumn(prev_line_index, other_cursor_new_char_index));
                    SetCursorPosition(target_coords, other_cursor);
                    cursorsHandled.insert(other_cursor);
                }

                ErrorMarkersT etmp;
                for (auto &i : ErrorMarkers) {
                    etmp.insert(ErrorMarkersT::value_type(i.first - 1 == EditorState.Cursors[c].CursorPosition.Line ? i.first - 1 : i.first, i.second));
                }
                ErrorMarkers = std::move(etmp);

                RemoveLine(EditorState.Cursors[c].CursorPosition.Line, &cursorsHandled);
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.Line - 1, prev_size}, c);
            } else {
                auto &line = Lines[EditorState.Cursors[c].CursorPosition.Line];

                if (is_word_mode) {
                    Coordinates start = FindWordStart(EditorState.Cursors[c].CursorPosition - Coordinates(0, 1));
                    u.Operations.push_back({GetText(start, EditorState.Cursors[c].CursorPosition), start, EditorState.Cursors[c].CursorPosition, UndoOperationType::Delete});
                    DeleteRange(start, EditorState.Cursors[c].CursorPosition);
                    int chars_deleted = EditorState.Cursors[c].CursorPosition.Column - start.Column;
                    EditorState.Cursors[c].CursorPosition.Column -= chars_deleted;
                } else {
                    auto cindex = GetCharacterIndexR(pos) - 1;
                    auto cend = cindex + 1;
                    while (cindex > 0 && IsUTFSequence(line[cindex].Char)) --cindex;

                    // if (cindex > 0 && UTF8CharLength(line[cindex].Char) > 1)
                    //	--cindex;

                    UndoOperation removed;
                    removed.Type = UndoOperationType::Delete;
                    removed.Start = removed.End = GetActualCursorCoordinates(c);
                    if (line[cindex].Char == '\t') {
                        int tab_start_column = GetCharacterColumn(removed.Start.Line, cindex);
                        int tab_length = removed.Start.Column - tab_start_column;
                        EditorState.Cursors[c].CursorPosition.Column -= tab_length;
                        removed.Start.Column -= tab_length;
                    } else {
                        --EditorState.Cursors[c].CursorPosition.Column;
                        --removed.Start.Column;
                    }

                    while (cindex < line.size() && cend-- > cindex) {
                        removed.Text += line[cindex].Char;
                        RemoveGlyphsFromLine(EditorState.Cursors[c].CursorPosition.Line, cindex, cindex + 1);
                    }
                    u.Operations.push_back(removed);
                }
                EditorState.Cursors[c].CursorPositionChanged = true;
            }
        }

        TextChanged = true;

        EnsureCursorVisible();
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            Colorize(EditorState.Cursors[c].CursorPosition.Line, 1);
        }
    }

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::SelectWordUnderCursor() {
    auto c = GetCursorPosition();
    SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll() {
    SetSelection(Coordinates(0, 0), Coordinates((int)Lines.size(), 0), SelectionModeT::Line);
}

bool TextEditor::HasSelection() const {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        if (EditorState.Cursors[c].SelectionEnd > EditorState.Cursors[c].SelectionStart) return true;
    }
    return false;
}

void TextEditor::Copy() {
    if (HasSelection()) {
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
        if (HasSelection()) {
            UndoRecord u;
            u.Before = EditorState;

            Copy();
            for (int c = EditorState.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
                DeleteSelection(c);
            }

            u.After = EditorState;
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
    if (EditorState.CurrentCursor > 0) {
        clip_text_lines.push_back({0, 0});
        for (int i = 0; i < clip_text.length(); i++) {
            if (clip_text[i] == '\n') {
                clip_text_lines.back().second = i;
                clip_text_lines.push_back({i + 1, 0});
            }
        }
        clip_text_lines.back().second = clip_text.length();
        can_paste_to_multiple_cursors = clip_text_lines.size() == EditorState.CurrentCursor + 1;
    }

    if (clip_text.length() > 0) {
        UndoRecord u;
        u.Before = EditorState;

        if (HasSelection()) {
            for (int c = EditorState.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
                DeleteSelection(c);
            }
        }

        for (int c = EditorState.CurrentCursor; c > -1; c--) {
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

        u.After = EditorState;
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

void TextEditor::ClearExtrcursors() { EditorState.CurrentCursor = 0; }

void TextEditor::ClearSelections() {
    for (int c = EditorState.CurrentCursor; c > -1; c--)
        EditorState.Cursors[c].InteractiveEnd =
            EditorState.Cursors[c].InteractiveStart =
                EditorState.Cursors[c].SelectionEnd =
                    EditorState.Cursors[c].SelectionStart = EditorState.Cursors[c].CursorPosition;
}

void TextEditor::SelectNextOccurrenceOf(const char *text, int text_size, int cursor) {
    if (cursor == -1) cursor = EditorState.CurrentCursor;

    Coordinates next_start, next_end;
    FindNextOccurrence(text, text_size, EditorState.Cursors[cursor].CursorPosition, next_start, next_end);
    EditorState.Cursors[cursor].InteractiveStart = next_start;
    EditorState.Cursors[cursor].CursorPosition = EditorState.Cursors[cursor].InteractiveEnd = next_end;
    SetSelection(EditorState.Cursors[cursor].InteractiveStart, EditorState.Cursors[cursor].InteractiveEnd, SelectionMode, cursor);
    EnsureCursorVisible(cursor);
}

void TextEditor::AddCursorForNextOccurrence() {
    const Cursor &current_cursor = EditorState.Cursors[EditorState.GetLastAddedCursorIndex()];
    if (current_cursor.SelectionStart == current_cursor.SelectionEnd) return;

    string selection_text = GetText(current_cursor.SelectionStart, current_cursor.SelectionEnd);
    Coordinates next_start, next_end;
    if (!FindNextOccurrence(selection_text.c_str(), selection_text.length(), current_cursor.SelectionEnd, next_start, next_end)) return;

    EditorState.AddCursor();
    EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = next_start;
    EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = next_end;
    SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode, -1, true);
    EditorState.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
    EnsureCursorVisible();
}

const TextEditor::PaletteT &TextEditor::GetDarkPalette() {
    const static PaletteT p = {{
        0xb0b0b0ff, // Default
        0x569cd6ff, // Keyword
        0x00ff00ff, // Number
        0xe07070ff, // String
        0xe0a070ff, // Char literal
        0xffffffff, // Punctuation
        0x808040ff, // Preprocessor
        0xaaaaaaff, // Identifier
        0x4dc69bff, // Known identifier
        0xa040c0ff, // Preproc identifier
        0x206020ff, // Comment (single line)
        0x206040ff, // Comment (multi line)
        0x101010ff, // Background
        0xe0e0e0ff, // Cursor
        0x2060a080, // Selection
        0xff200080, // ErrorMarker
        0x90909090, // ControlCharacter
        0x0080f040, // Breakpoint
        0x007070ff, // Line number
        0x00000040, // Current line fill
        0x80808040, // Current line fill (inactive)
        0xa0a0a040, // Current line edge
    }};
    return p;
}

const TextEditor::PaletteT &TextEditor::GetMarianaPalette() {
    const static PaletteT p = {{
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
    const static PaletteT p = {{
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
    const static PaletteT p = {{
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
    if (HasSelection()) {
        // merge cursors if they overlap
        for (int c = EditorState.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1;
            bool pc_contains_c = EditorState.Cursors[pc].SelectionEnd >= EditorState.Cursors[c].SelectionEnd;
            bool pc_contains_start_of_c = EditorState.Cursors[pc].SelectionEnd >= EditorState.Cursors[c].SelectionStart;
            if (pc_contains_c) {
                cursors_to_delete.insert(c);
            } else if (pc_contains_start_of_c) {
                EditorState.Cursors[pc].SelectionEnd = EditorState.Cursors[c].SelectionEnd;
                EditorState.Cursors[pc].InteractiveEnd = EditorState.Cursors[c].SelectionEnd;
                EditorState.Cursors[pc].InteractiveStart = EditorState.Cursors[pc].SelectionStart;
                EditorState.Cursors[pc].CursorPosition = EditorState.Cursors[c].SelectionEnd;
                cursors_to_delete.insert(c);
            }
        }
    } else {
        // merge cursors if they are at the same position
        for (int c = EditorState.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1;
            if (EditorState.Cursors[pc].CursorPosition == EditorState.Cursors[c].CursorPosition) {
                cursors_to_delete.insert(c);
            }
        }
    }
    for (int c = EditorState.CurrentCursor; c > -1; c--) // iterate backwards through each of them
    {
        if (cursors_to_delete.find(c) != cursors_to_delete.end()) {
            EditorState.Cursors.erase(EditorState.Cursors.begin() + c);
        }
    }
    EditorState.CurrentCursor -= cursors_to_delete.size();
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
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        if (EditorState.Cursors[c].SelectionStart < EditorState.Cursors[c].SelectionEnd) {
            if (result.length() != 0) result += '\n';
            result += GetText(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd);
        }
    }
    return result;
}

string TextEditor::GetSelectedText(int cursor) const {
    if (cursor == -1) cursor = EditorState.CurrentCursor;
    return GetText(EditorState.Cursors[cursor].SelectionStart, EditorState.Cursors[cursor].SelectionEnd);
}

string TextEditor::GetCurrentLineText() const {
    auto line_length = GetLineMaxColumn(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.Line);
    return GetText(
        Coordinates(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.Line, 0),
        Coordinates(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.Line, line_length)
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
    if (cursor == -1) cursor = EditorState.GetLastAddedCursorIndex();

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
    TextEditor::EditorStateT &before,
    TextEditor::EditorStateT &after
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

    editor->EditorState = Before;
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

    editor->EditorState = After;
    editor->EnsureCursorVisible();
}
