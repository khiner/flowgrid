#include "TextEditor.h"

#include <algorithm>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range/conversion.hpp>
#include <ranges>
#include <set>
#include <string>

#include <tree_sitter/api.h>

#include "imgui_internal.h"

using std::string, std::ranges::reverse_view, std::ranges::any_of, std::ranges::all_of, std::ranges::subrange;

// Implemented by the grammar libraries in `lib/tree-sitter-grammars/`.
extern "C" TSLanguage *tree_sitter_json();
extern "C" TSLanguage *tree_sitter_cpp();

void AddTypes(LanguageDefinition::PaletteT &palette, PaletteIndex index, std::initializer_list<std::string> types) {
    for (const auto &type : types) palette[type] = index;
}

LanguageDefinition::PaletteT LanguageDefinition::CreatePalette(ID id) {
    PaletteT p;
    using PI = PaletteIndex;
    switch (id) {
        case ID::Cpp:
            AddTypes(p, PI::Keyword, {"auto", "break", "case", "const", "constexpr", "continue", "default", "do", "else", "extern", "false", "for", "if", "nullptr", "private", "return", "static", "struct", "switch", "this", "true", "using", "while"});
            AddTypes(p, PI::Operator, {"!", "!=", "&", "&&", "&=", "*", "++", "+=", "-", "--", "-=", "->", "/", "<", "<=", "=", "==", ">", ">=", "[", "]", "^=", "|", "||", "~"});
            AddTypes(p, PI::NumberLiteral, {"number_literal"});
            AddTypes(p, PI::CharLiteral, {"character"});
            AddTypes(p, PI::StringLiteral, {"string_content", "\"", "'", "system_lib_string"});
            AddTypes(p, PI::Identifier, {"identifier", "field_identifier", "namespace_identifier", "translation_unit", "type_identifier"});
            AddTypes(p, PI::Type, {"primitive_type"});
            AddTypes(p, PI::Preprocessor, {"#define", "#include", "preproc_arg"});
            AddTypes(p, PI::Punctuation, {"(", ")", "+", ",", ".", ":", "::", ";", "?", "{", "}"});
            AddTypes(p, PI::Comment, {"escape_sequence", "comment"});
            break;
        case ID::Json:
            AddTypes(p, PI::Type, {"true", "false", "null"});
            AddTypes(p, PI::NumberLiteral, {"number"});
            AddTypes(p, PI::StringLiteral, {"string_content", "\""});
            AddTypes(p, PI::Punctuation, {",", ":", "[", "]", "{", "}"});
            break;
        default:
    }

    return p;
}

LanguageDefinitions::LanguageDefinitions() {
    ById = {
        {ID::None, {ID::None, "None"}},
        {ID::Cpp, {ID::Cpp, "C++", tree_sitter_cpp(), {".h", ".hpp", ".cpp"}, "//"}},
        {ID::Json, {ID::Json, "JSON", tree_sitter_json(), {".json"}}},
    };
    for (const auto &[id, lang_def] : ById) {
        for (const auto &ext : lang_def.FileExtensions) ByFileExtension[ext] = id;
    }
}

struct TextEditor::CodeParser {
    CodeParser() : Parser(ts_parser_new()) {}
    ~CodeParser() { ts_parser_delete(Parser); }

    void SetLanguage(TSLanguage *language) {
        Language = language;
        ts_parser_set_language(Parser, Language);
    }
    TSLanguage *GetLanguage() const { return Language; }

    TSParser *get() const { return Parser; }

private:
    TSParser *Parser;
    TSLanguage *Language;
};

TextEditor::TextEditor() : Parser(std::make_unique<CodeParser>()) {
    SetPalette(DefaultPaletteId);
}

TextEditor::~TextEditor() {
    ts_tree_delete(Tree);
}

const char *TSReadText(void *payload, uint32_t byte_index, TSPoint position, uint32_t *bytes_read) {
    (void)byte_index; // Unused.
    static const char newline = '\n';

    const auto *editor = static_cast<TextEditor *>(payload);
    if (position.row >= editor->LineCount()) {
        *bytes_read = 0;
        return nullptr;
    }
    const auto &line = editor->GetLine(position.row);
    if (position.column > line.size()) { // Sanity check - shouldn't happen.
        *bytes_read = 0;
        return nullptr;
    }

    if (position.column == line.size()) {
        *bytes_read = 1;
        return &newline;
    }

    // Read until the end of the line.
    *bytes_read = line.size() - position.column;
    return line.data() + position.column;
}

void TextEditor::Parse() {
    TSInput input{this, TSReadText, TSInputEncodingUTF8};
    Tree = ts_parser_parse(Parser->get(), Tree, std::move(input));
}

string TextEditor::GetSyntaxTreeSExp() const {
    char *c_string = ts_node_string(ts_tree_root_node(Tree));
    string s_expression(c_string);
    free(c_string);
    return s_expression;
}

void TextEditor::Highlight() {
    if (Parser->GetLanguage() == nullptr) return;

    Parse();
    const auto &palette = GetLanguage().Palette;
    TSNode root_node = ts_tree_root_node(Tree);
    TSTreeCursor cursor = ts_tree_cursor_new(root_node);
    while (true) {
        TSNode node = ts_tree_cursor_current_node(&cursor);
        const TSPoint start_point = ts_node_start_point(node), end_point = ts_node_end_point(node);
        const string type_name = ts_node_type(node);
        // todo Handle node group types other than comments.
        if (ts_node_child_count(node) == 0 || type_name == "comment") {
            const auto palette_index = palette.contains(type_name) ? palette.at(type_name) : PaletteIndex::Default;
            // if (!palette.contains(type_name)) std::println("Unknown type name: {}", type_name);

            // Add palette index for each char in the node.
            for (auto b = start_point; b.row < end_point.row || (b.row == end_point.row && b.column < end_point.column);) {
                if (b.row >= Lines.size()) break;
                if (b.column >= Lines[b.row].size()) {
                    b.row++;
                    b.column = 0;
                    continue;
                }
                PaletteIndices[b.row][b.column] = palette_index;
                b.column++;
            }
        }

        if (ts_tree_cursor_goto_first_child(&cursor)) continue;

        while (!ts_tree_cursor_goto_next_sibling(&cursor)) {
            if (!ts_tree_cursor_goto_parent(&cursor)) {
                ts_tree_cursor_delete(&cursor);
                return;
            }
        }
    }
}

static bool Equals(const auto &c1, const auto &c2, std::size_t c2_offset = 0) {
    if (c2.size() + c2_offset < c1.size()) return false;

    const auto begin = c2.cbegin() + c2_offset;
    return std::ranges::equal(c1, subrange(begin, begin + c1.size()), [](const auto &a, const auto &b) { return a == b; });
}

const TextEditor::PaletteT *TextEditor::GetPalette(PaletteIdT palette_id) {
    switch (palette_id) {
        case PaletteIdT::Dark:
            return &DarkPalette;
        case PaletteIdT::Light:
            return &LightPalette;
        case PaletteIdT::Mariana:
            return &MarianaPalette;
        case PaletteIdT::RetroBlue:
            return &RetroBluePalette;
    }
}

void TextEditor::SetPalette(PaletteIdT palette_id) {
    const PaletteT *palette_base = GetPalette(palette_id);
    for (uint i = 0; i < uint(PaletteIndex::Max); ++i) {
        const ImVec4 color = U32ColorToVec4((*palette_base)[i]);
        // color.w *= ImGui::GetStyle().Alpha; todo bring this back.
        Palette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }
}

void TextEditor::SetLanguage(LanguageDefinition::ID language_def_id) {
    if (LanguageId == language_def_id) return;

    LanguageId = language_def_id;
    Parser->SetLanguage(GetLanguage().TsLanguage);
    Tree = nullptr;
    TextChanged = true;
}

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
    } else {
        string str;
        for (const auto &g : Lines[GetCursorPosition().L]) str.push_back(g);
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
            const auto before_end = SanitizeCoords(cursor.End);
            const string insert_text = can_paste_to_multiple_cursors ? clip_text.substr(clip_text_lines[c].first, clip_text_lines[c].second - clip_text_lines[c].first) : clip_text;
            InsertTextAtCursor(insert_text, cursor);
            u.Operations.emplace_back(insert_text, before_end, SanitizeCoords(cursor.End), UndoOperationType::Add);
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
    const LineChar old_end{uint(Lines.size() - 1), uint(Lines.back().size())};
    Lines.clear();
    Lines.push_back({});
    PaletteIndices.clear();
    PaletteIndices.push_back({});
    for (auto chr : text) {
        if (chr == '\r') continue; // Ignore the carriage return character.
        if (chr == '\n') {
            Lines.push_back({});
            PaletteIndices.push_back({});
        } else {
            Lines.back().emplace_back(chr);
            PaletteIndices.back().emplace_back(PaletteIndex::Default);
        }
    }

    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    OnTextChanged({0, 0}, old_end, {uint(Lines.size() - 1), uint(Lines.back().size())});
}

void TextEditor::SetFilePath(const fs::path &file_path) {
    const string extension = file_path.extension();
    if (extension.empty()) return;

    SetLanguage(Languages.ByFileExtension.contains(extension) ? Languages.ByFileExtension.at(extension) : LanguageDefinition::ID::None);
}

void TextEditor::AddUndoOp(UndoRecord &record, UndoOperationType type, const Coords &start, const Coords &end) {
    record.Operations.emplace_back(GetText(start, end), start, end, type);
}

string TextEditor::GetText(const Coords &start, const Coords &end) const {
    if (end <= start) return "";

    const uint start_li = start.L, end_li = std::min(uint(Lines.size()) - 1, end.L);
    const uint start_ci = GetCharIndex(start), end_ci = GetCharIndex(end);
    string result;
    for (uint ci = start_ci, li = start_li; li < end_li || ci < end_ci;) {
        const auto &line = Lines[li];
        if (ci < line.size()) {
            result += line[ci++];
        } else {
            ++li;
            ci = 0;
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

    if (TextChanged) {
        Highlight();
        TextChanged = false;
    }

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

void TextEditor::UndoRecord::Undo(TextEditor *editor) {
    for (const auto &op : reverse_view(Operations)) {
        if (op.Text.empty()) continue;

        switch (op.Type) {
            case UndoOperationType::Delete: {
                editor->InsertTextAt(op.Start, op.Text);
                break;
            }
            case UndoOperationType::Add: {
                editor->DeleteRange(op.Start, op.End);
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
                break;
            }
            case UndoOperationType::Add: {
                editor->InsertTextAt(op.Start, op.Text);
                break;
            }
        }
    }

    editor->State = After;
    editor->EnsureCursorVisible();
}

void TextEditor::SetCursorPosition(const Coords &position, Cursor &c, bool clear_selection) {
    CursorPositionChanged = true;

    if (clear_selection) c.Start = position;

    if (c.End != position) {
        c.End = position;
        EnsureCursorVisible();
    }
}

void TextEditor::InsertTextAtCursor(const string &text, Cursor &c) {
    if (text.empty()) return;

    const auto start = std::min(SanitizeCoords(c.End), c.SelectionStart());
    SetCursorPosition(InsertTextAt(start, text), c);
}

void TextEditor::LineCharIter::MoveRight() {
    if (LC == End) return;

    if (LC.C == Lines[LC.L].size()) {
        LC.L++;
        LC.C = 0;
    } else {
        LC.C = std::min(LC.C + UTF8CharLength(Lines[LC.L][LC.C]), uint(Lines[LC.L].size()));
    }
}
void TextEditor::LineCharIter::MoveLeft() {
    if (LC == Begin) return;

    if (LC.C == 0) {
        LC.L--;
        LC.C = Lines[LC.L].size();
    } else {
        do { LC.C--; } while (LC.C > 0 && IsUTFSequence(Lines[LC.L][LC.C]));
    }
}

static uint NextTabstop(uint column, uint tab_size) { return ((column / tab_size) + 1) * tab_size; }

void TextEditor::MoveCharIndexAndColumn(uint li, uint &ci, uint &column) const {
    const char ch = Lines[li][ci];
    ci += UTF8CharLength(ch);
    column = ch == '\t' ? NextTabstop(column, TabSize) : column + 1;
}

uint TextEditor::NumStartingSpaceColumns(uint li) const {
    const auto &line = Lines[li];
    uint column = 0;
    for (uint ci = 0; ci < line.size() && isblank(line[ci]);) MoveCharIndexAndColumn(li, ci, column);
    return column;
}

TextEditor::Coords TextEditor::MoveCoords(const Coords &coords, MoveDirection direction, bool is_word_mode, uint line_count) const {
    LineCharIter lci{Lines, ToLineChar(coords)};
    switch (direction) {
        case MoveDirection::Right:
            if (lci == lci.end()) return coords;
            ++lci;
            if (is_word_mode) return FindWordBoundary(ToCoords(*lci), false);
            return ToCoords(*lci);
        case MoveDirection::Left:
            if (lci == lci.begin()) return coords;
            --lci;
            if (is_word_mode) return FindWordBoundary(ToCoords(*lci), true);
            return ToCoords(*lci);
        case MoveDirection::Up:
            return {uint(std::max(0, int(coords.L) - int(line_count))), coords.C};
        case MoveDirection::Down:
            return {std::min(coords.L + line_count, uint(Lines.size() - 1)), coords.C};
    }
}

void TextEditor::MoveUp(uint amount, bool select) {
    for (auto &c : State.Cursors) SetCursorPosition(MoveCoords(c.End, MoveDirection::Up, false, amount), c, !select);
    EnsureCursorVisible();
}

void TextEditor::MoveDown(uint amount, bool select) {
    for (auto &c : State.Cursors) SetCursorPosition(MoveCoords(c.End, MoveDirection::Down, false, amount), c, !select);
    EnsureCursorVisible();
}

void TextEditor::MoveLeft(bool select, bool is_word_mode) {
    for (auto &c : State.Cursors) {
        if (AnyCursorHasSelection() && !select && !is_word_mode) SetCursorPosition(c.SelectionStart(), c);
        else SetCursorPosition(MoveCoords(c.End, MoveDirection::Left, is_word_mode), c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(bool select, bool is_word_mode) {
    for (auto &c : State.Cursors) {
        if (AnyCursorHasSelection() && !select && !is_word_mode) SetCursorPosition(c.SelectionEnd(), c);
        else SetCursorPosition(MoveCoords(c.End, MoveDirection::Right, is_word_mode), c, !select);
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool select) { SetCursorPosition({0, 0}, State.GetCursor(), !select); }
void TextEditor::TextEditor::MoveBottom(bool select) { SetCursorPosition(LineMaxCoords(Lines.size() - 1), State.GetCursor(), !select); }

void TextEditor::MoveHome(bool select) {
    for (auto &c : State.Cursors) SetCursorPosition({c.End.L, 0}, c, !select);
}
void TextEditor::MoveEnd(bool select) {
    for (auto &c : State.Cursors) SetCursorPosition(LineMaxCoords(c.End.L), c, !select);
}

void TextEditor::EnterChar(ImWchar ch, bool is_shift) {
    if (ch == '\t' && AnyCursorHasMultilineSelection()) return ChangeCurrentLinesIndentation(!is_shift);

    UndoRecord u{State};
    for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);

    // Order is important here for typing '\n' in the same line with multiple cursors.
    for (auto &c : reverse_view(State.Cursors)) {
        const auto coord = SanitizeCoords(c.End);
        string insert_text;
        if (ch == '\n') {
            insert_text = "\n";
            if (AutoIndent) {
                // Match the indentation of the current or next line, whichever has more indentation.
                const uint li = coord.L;
                const uint indent_li = li < Lines.size() - 1 && NumStartingSpaceColumns(li + 1) > NumStartingSpaceColumns(li) ? li + 1 : li;
                const auto &indent_line = Lines[indent_li];
                for (uint i = 0; i < indent_line.size() && isblank(indent_line[i]); i++) insert_text += indent_line[i];
            }
        } else {
            char buf[5];
            ImTextCharToUtf8(buf, ch);
            insert_text = buf;
        }

        InsertTextAtCursor(insert_text, c);
        u.Operations.emplace_back(insert_text, coord, SanitizeCoords(c.End), UndoOperationType::Add);
    }

    AddUndo(u);

    EnsureCursorVisible();
}

void TextEditor::Backspace(bool is_word_mode) {
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

void TextEditor::SetSelection(Coords start, Coords end, Cursor &c) {
    const Coords min_coords{0, 0}, max_coords{LineMaxCoords(Lines.size() - 1)};
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

std::optional<TextEditor::Cursor> TextEditor::FindNextOccurrence(const string &text, const Coords &start, bool case_sensitive) {
    if (text.empty()) return {};

    const LineChar start_lc = ToLineChar(start);
    LineCharIter find_lci{Lines, start_lc};
    do {
        LineCharIter match_lci = find_lci;
        for (uint i = 0; i < text.size(); i++) {
            const auto &match_lc = *match_lci;
            if (match_lc.C == Lines[match_lc.L].size()) {
                if (text[i] != '\n' || match_lc.L + 1 >= Lines.size()) break;
            } else if (ToLower(match_lci, case_sensitive) != ToLower(text[i], case_sensitive)) {
                break;
            }

            ++match_lci;
            if (i == text.size() - 1) return Cursor{ToCoords(*find_lci), ToCoords(*match_lci)};
        }

        ++find_lci;
        if (find_lci == find_lci.end()) find_lci = find_lci.begin();
    } while (*find_lci != start_lc);

    return {};
}

std::optional<TextEditor::Cursor> TextEditor::FindMatchingBrackets(const Cursor &c) {
    static const std::unordered_map<char, char>
        OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
        CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

    const LineChar cursor_lc = ToLineChar(c.End);
    const auto &line = Lines[cursor_lc.L];
    if (c.HasSelection() || line.empty()) return {};

    uint ci = cursor_lc.C;
    // Considered on bracket if cursor is to the left or right of it.
    if (ci > 0 && (CloseToOpenChar.contains(line[ci - 1]) || OpenToCloseChar.contains(line[ci - 1]))) ci--;

    const char ch = line[ci];
    const bool is_close_char = CloseToOpenChar.contains(ch), is_open_char = OpenToCloseChar.contains(ch);
    if (!is_close_char && !is_open_char) return {};

    const char other_ch = is_close_char ? CloseToOpenChar.at(ch) : OpenToCloseChar.at(ch);
    uint match_count = 0;
    for (LineCharIter lci{Lines, cursor_lc}; lci != (is_close_char ? lci.begin() : lci.end()); is_close_char ? --lci : ++lci) {
        const char ch_inner = lci;
        if (ch_inner == ch) match_count++;
        else if (ch_inner == other_ch && (match_count == 0 || --match_count == 0)) return Cursor{ToCoords(cursor_lc), ToCoords(*lci)};
    }

    return {};
}

void TextEditor::ChangeCurrentLinesIndentation(bool increase) {
    UndoRecord u{State};
    for (const auto &c : reverse_view(State.Cursors)) {
        for (uint li = c.SelectionStart().L; li <= c.SelectionEnd().L; li++) {
            // Check if selection ends at line start.
            if (c.HasSelection() && c.SelectionEnd() == Coords{li, 0}) continue;

            const auto &line = Lines[li];
            if (increase) {
                if (!line.empty()) {
                    const Coords start{li, 0}, end = InsertTextAt(start, "\t");
                    u.Operations.emplace_back("\t", start, end, UndoOperationType::Add);
                }
            } else {
                const Coords start{li, 0}, end{li, TabSize};
                int ci = int(GetCharIndex(end)) - 1;
                while (ci > -1 && isblank(line[ci])) ci--;
                const bool only_space_chars_found = ci == -1;
                if (only_space_chars_found) {
                    u.Operations.emplace_back(GetText(start, end), start, end, UndoOperationType::Delete);
                    DeleteRange(start, end);
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
            if (c.HasSelection() && c.SelectionEnd() == Coords{li, 0}) continue;

            affected_lines.insert(li);
            min_li = std::min(li, min_li);
            max_li = std::max(li, max_li);
        }
    }
    if ((up && min_li == 0) || (!up && max_li == Lines.size() - 1)) return; // Can't move up/down anymore.

    const uint start_li = min_li - (up ? 1 : 0), end_li = max_li + (up ? 0 : 1);
    const Coords start{start_li, 0}, end{LineMaxCoords(end_li)};
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
    const string &comment = GetLanguage().SingleLineComment;
    if (comment.empty()) return;

    static const auto FindFirstNonSpace = [](const LineT &line) {
        return std::distance(line.begin(), std::ranges::find_if_not(line, isblank));
    };

    std::unordered_set<uint> affected_lines;
    for (const auto &c : State.Cursors) {
        for (uint li = c.SelectionStart().L; li <= c.SelectionEnd().L; li++) {
            if (!(c.HasSelection() && c.SelectionEnd() == Coords{li, 0}) && !Lines[li].empty()) affected_lines.insert(li);
        }
    }

    UndoRecord u{State};
    const bool should_add_comment = any_of(affected_lines, [&](uint li) {
        return !Equals(comment, Lines[li], FindFirstNonSpace(Lines[li]));
    });
    for (uint li : affected_lines) {
        if (should_add_comment) {
            const Coords line_start{li, 0}, insertion_end = InsertTextAt(line_start, comment + ' ');
            u.Operations.emplace_back(comment + ' ', line_start, insertion_end, UndoOperationType::Add);
        } else {
            const auto &line = Lines[li];
            const uint ci = FindFirstNonSpace(line);
            uint comment_ci = ci + comment.length();
            if (comment_ci < line.size() && line[comment_ci] == ' ') comment_ci++;

            const Coords start = ToCoords({li, ci}), end = ToCoords({li, comment_ci});
            AddUndoOp(u, UndoOperationType::Delete, start, end);
            DeleteRange(start, end);
        }
    }
    AddUndo(u);
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u{State};
    for (auto &c : reverse_view(State.Cursors)) DeleteSelection(c, u);
    MoveHome();
    OnCursorPositionChanged(); // Might combine cursors.

    for (auto &c : reverse_view(State.Cursors)) {
        const uint li = c.End.L;
        Coords to_delete_start, to_delete_end;
        if (Lines.size() > li + 1) { // Next line exists.
            to_delete_start = {li, 0};
            to_delete_end = {li + 1, 0};
            SetCursorPosition({li, 0}, c);
        } else if (li > 0) { // Previous line exists.
            to_delete_start = LineMaxCoords(li - 1);
            to_delete_end = LineMaxCoords(li);
            SetCursorPosition({li - 1, 0}, c);
        } else {
            to_delete_start = {li, 0};
            to_delete_end = LineMaxCoords(li);
            SetCursorPosition({li, 0}, c);
        }

        AddUndoOp(u, UndoOperationType::Delete, to_delete_start, to_delete_end);
        if (to_delete_start.L != to_delete_end.L) {
            Lines.erase(Lines.begin() + li);
            PaletteIndices.erase(PaletteIndices.begin() + li);
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

float TextEditor::TextDistanceToLineStart(const Coords &from, bool sanitize_coords) const {
    return (sanitize_coords ? SanitizeCoords(from) : from).C * CharAdvance.x;
}

void TextEditor::EnsureCursorVisible(bool start_too) {
    LastEnsureCursorVisible = State.GetLastAddedCursorIndex();
    LastEnsureCursorVisibleStartToo = start_too;
}

TextEditor::Coords TextEditor::SanitizeCoords(const Coords &coords) const {
    if (coords.L >= Lines.size()) return LineMaxCoords(Lines.size() - 1);
    return {coords.L, Lines.empty() ? 0 : GetLineMaxColumn(coords.L, coords.C)};
}

TextEditor::Coords TextEditor::ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li) const {
    static constexpr float PosToCoordsColumnOffset = 0.33;

    const auto local = ImVec2{screen_pos.x + 3.0f, screen_pos.y} - ImGui::GetCursorScreenPos();
    if (is_over_li != nullptr) *is_over_li = local.x < TextStart;

    Coords out{
        std::max(0u, uint(floor(local.y / CharAdvance.y))),
        std::max(0u, uint(floor((local.x - TextStart) / CharAdvance.x)))
    };
    const uint ci = GetCharIndex(out);
    if (out.L < Lines.size() && ci < Lines[out.L].size() && Lines[out.L][ci] == '\t') {
        out.C -= GetCharColumn({out.L, ci});
    } else {
        out.C = std::max(0u, uint(floor((local.x - TextStart + PosToCoordsColumnOffset * CharAdvance.x) / CharAdvance.x)));
    }
    return SanitizeCoords(out);
}

TextEditor::Coords TextEditor::FindWordBoundary(const Coords &from, bool is_start) const {
    if (from.L >= Lines.size()) return from;

    const auto &line = Lines[from.L];
    uint ci = GetCharIndex(from);
    if (ci >= line.size()) return from;

    const bool init_is_word_char = IsWordChar(line[ci]), init_is_space = isspace(line[ci]);
    const char init_char = line[ci];
    for (; is_start ? ci > 0 : ci < line.size(); is_start ? --ci : ++ci) {
        if (ci == line.size() ||
            (init_is_space && !isspace(line[ci])) ||
            (init_is_word_char && !IsWordChar(line[ci])) ||
            (!init_is_word_char && !init_is_space && init_char != line[ci])) {
            if (is_start) ++ci; // Undo one left step before returning line/char.
            break;
        }
    }
    return ToCoords({from.L, ci});
}

uint TextEditor::GetCharIndex(const Coords &coords) const {
    const uint li = std::min(coords.L, uint(Lines.size() - 1));
    uint ci = 0;
    for (uint column = 0; ci < Lines[li].size() && column < coords.C;) MoveCharIndexAndColumn(li, ci, column);
    return ci;
}

uint TextEditor::GetCharColumn(LineChar lc) const {
    if (lc.L >= Lines.size()) return 0;

    uint column = 0;
    for (uint ci = 0; ci < lc.C && ci < Lines[lc.L].size();) MoveCharIndexAndColumn(lc.L, ci, column);
    return column;
}

uint TextEditor::GetFirstVisibleCharIndex(uint li) const {
    if (li >= Lines.size()) return 0;

    uint ci = 0, column = 0;
    while (column < FirstVisibleCoords.C && ci < Lines[li].size()) MoveCharIndexAndColumn(li, ci, column);
    return column > FirstVisibleCoords.C && ci > 0 ? ci - 1 : ci;
}

uint TextEditor::GetLineMaxColumn(uint li) const {
    if (li >= Lines.size()) return 0;

    uint column = 0;
    for (uint ci = 0; ci < Lines[li].size();) MoveCharIndexAndColumn(li, ci, column);
    return column;
}
uint TextEditor::GetLineMaxColumn(uint li, uint limit) const {
    if (li >= Lines.size()) return 0;

    uint column = 0;
    for (uint ci = 0; ci < Lines[li].size() && column < limit;) MoveCharIndexAndColumn(li, ci, column);
    return column;
}

void TextEditor::DeleteRange(const Coords &start, const Coords &end, const Cursor *exclude_cursor) {
    if (end <= start) return;

    const LineChar start_lc = ToLineChar(start), end_lc = ToLineChar(end);
    if (start.L == end.L) {
        RemoveGlyphs(start_lc, end_lc.C);
    } else {
        // At least one line completely removed.
        RemoveGlyphs(start_lc); // from start to end of line
        RemoveGlyphs({end_lc.L, 0}, end_lc.C);
        AddGlyphs({start_lc.L, uint(Lines[start_lc.L].size())}, Lines[end_lc.L]);

        // Move up all cursors after the last removed line.
        const uint num_removed_lines = end_lc.L - start_lc.L;
        for (auto &c : State.Cursors) {
            if (exclude_cursor != nullptr && c == *exclude_cursor) continue;

            if (c.End.L >= end_lc.L) {
                c.Start.L -= num_removed_lines;
                c.End.L -= num_removed_lines;
            }
        }

        Lines.erase(Lines.begin() + start.L + 1, Lines.begin() + end.L + 1);
        PaletteIndices.erase(PaletteIndices.begin() + start.L + 1, PaletteIndices.begin() + end.L + 1);
    }
    OnTextChanged(start_lc, end_lc, start_lc);
}

void TextEditor::DeleteSelection(Cursor &c, UndoRecord &record) {
    if (!c.HasSelection()) return;

    AddUndoOp(record, UndoOperationType::Delete, c.SelectionStart(), c.SelectionEnd());
    // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
    DeleteRange(c.SelectionStart(), c.SelectionEnd(), &c);
    SetCursorPosition(c.SelectionStart(), c);
}

void TextEditor::InsertLine(uint li) {
    Lines.insert(Lines.begin() + li, LineT{});
    PaletteIndices.insert(PaletteIndices.begin() + li, std::vector<PaletteIndex>{});
    for (auto &c : State.Cursors) {
        if (c.End.L >= li) SetCursorPosition({c.End.L + 1, c.End.C}, c);
    }
}
void TextEditor::AddOrRemoveGlyphs(LineChar lc, std::span<const char> glyphs, bool is_add) {
    auto &line = Lines[lc.L];
    const uint column = GetCharColumn(lc);

    // New character index for each affected cursor when other cursor writes/deletes are on the same line.
    std::unordered_map<uint, uint> adjusted_ci_for_cursor;
    for (uint c = 0; c < State.Cursors.size(); c++) {
        const auto &cursor = State.Cursors[c];
        // Cursor needs adjusting if it's to the right of a change, without a selection.
        if (cursor.End.L == lc.L && cursor.End.C > column && !cursor.HasSelection()) {
            adjusted_ci_for_cursor[c] = GetCharIndex({lc.L, cursor.End.C}) + (is_add ? glyphs.size() : -glyphs.size());
        }
    }

    auto &palette_line = PaletteIndices[lc.L];
    if (is_add) {
        line.insert(line.begin() + lc.C, glyphs.begin(), glyphs.end());
        palette_line.insert(palette_line.begin() + lc.C, glyphs.size(), PaletteIndex::Default);
    } else {
        line.erase(glyphs.begin(), glyphs.end());
        palette_line.erase(palette_line.begin() + lc.C, palette_line.begin() + lc.C + glyphs.size());
    }

    for (const auto &[c, new_ci] : adjusted_ci_for_cursor) SetCursorPosition(ToCoords({lc.L, new_ci}), State.Cursors[c]);
}

ImU32 TextEditor::GetGlyphColor(LineChar lc) const {
    return Palette[int(LanguageId == LanguageDefinition::ID::None ? PaletteIndex::Default : PaletteIndices[lc.L][lc.C])];
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
        const auto cursor_coords = ScreenPosToCoords(ImGui::GetMousePos());
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

                const auto cursor_coords = ScreenPosToCoords(ImGui::GetMousePos());
                SetSelection(
                    {cursor_coords.L, 0},
                    cursor_coords.L < Lines.size() - 1 ? Coords{cursor_coords.L + 1, 0} : LineMaxCoords(cursor_coords.L),
                    State.GetCursor()
                );

                LastClickTime = -1.0f;
            } else if (is_double_click) {
                if (ctrl) State.AddCursor();
                else State.ResetCursors();

                const auto cursor_coords = ScreenPosToCoords(ImGui::GetMousePos());
                SetSelection(FindWordBoundary(cursor_coords, true), FindWordBoundary(cursor_coords, false), State.GetCursor());

                LastClickTime = float(ImGui::GetTime());
                LastClickPos = io.MousePos;
            } else if (is_click) {
                if (ctrl) State.AddCursor();
                else State.ResetCursors();

                bool is_over_li;
                const auto cursor_coords = ScreenPosToCoords(ImGui::GetMousePos(), &is_over_li);
                if (is_over_li) {
                    SetSelection(
                        {cursor_coords.L, 0},
                        cursor_coords.L < Lines.size() - 1 ? Coords{cursor_coords.L + 1, 0} : LineMaxCoords(cursor_coords.L),
                        State.GetCursor()
                    );
                } else {
                    SetCursorPosition(cursor_coords, State.GetLastAddedCursor());
                }

                LastClickTime = float(ImGui::GetTime());
                LastClickPos = io.MousePos;
            } else if (ImGui::IsMouseReleased(0)) {
                SortAndMergeCursors();
            }
        } else if (shift && is_click) {
            const auto new_selection = ScreenPosToCoords(ImGui::GetMousePos());
            SetCursorPosition(SanitizeCoords(new_selection), State.GetCursor(), false);
        }
    }
}

void TextEditor::UpdateViewVariables(float scroll_x, float scroll_y) {
    static constexpr float ImGuiScrollbarWidth = 14;

    ContentHeight = ImGui::GetWindowHeight() - (IsHorizontalScrollbarVisible() ? ImGuiScrollbarWidth : 0.0f);
    ContentWidth = ImGui::GetWindowWidth() - (IsVerticalScrollbarVisible() ? ImGuiScrollbarWidth : 0.0f);

    VisibleLineCount = std::max(uint(ceil(ContentHeight / CharAdvance.y)), 0u);
    VisibleColumnCount = std::max(uint(ceil((ContentWidth - std::max(TextStart - scroll_x, 0.0f)) / CharAdvance.x)), 0u);
    FirstVisibleCoords = {uint(scroll_y / CharAdvance.y), uint(std::max(scroll_x - TextStart, 0.0f) / CharAdvance.x)};
    LastVisibleCoords = {uint((ContentHeight + scroll_y) / CharAdvance.y), uint((ContentWidth + scroll_x - TextStart) / CharAdvance.x)};
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
    auto dl = ImGui::GetWindowDrawList();
    const float font_size = ImGui::GetFontSize();
    const float space_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, " ").x;

    for (uint li = FirstVisibleCoords.L; li <= LastVisibleCoords.L && li < Lines.size(); li++) {
        const auto &line = Lines[li];
        max_column_limited = std::max(GetLineMaxColumn(li, LastVisibleCoords.C), max_column_limited);

        const ImVec2 line_start_screen_pos{cursor_screen_pos.x, cursor_screen_pos.y + li * CharAdvance.y};
        const float text_screen_pos_x = line_start_screen_pos.x + TextStart;
        const Coords line_start_coord{li, 0}, line_end_coord{li, max_column_limited};
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

        std::vector<Coords> cursor_coords_in_this_line;
        for (const auto &c : State.Cursors) {
            if (c.End.L == li) cursor_coords_in_this_line.push_back(c.End);
        }
        if (cursor_coords_in_this_line.size() > 0) {
            // Render the cursors
            if (ImGui::IsWindowFocused() || is_parent_focused) {
                for (const auto &cursor_coords : cursor_coords_in_this_line) {
                    float width = 1.0f;
                    const uint ci = GetCharIndex(cursor_coords);
                    const float cx = TextDistanceToLineStart(cursor_coords);
                    if (Overwrite && ci < line.size()) {
                        if (line[ci] == '\t') {
                            const auto x = (1.0f + std::floor((1.0f + cx) / (TabSize * space_size))) * (TabSize * space_size);
                            width = x - cx;
                        } else {
                            width = CharAdvance.x;
                        }
                    }
                    dl->AddRectFilled(
                        {text_screen_pos_x + cx, line_start_screen_pos.y},
                        {text_screen_pos_x + cx + width, line_start_screen_pos.y + CharAdvance.y},
                        Palette[int(PaletteIndex::Cursor)]
                    );
                }
            }
        }

        // Render colorized text
        for (uint ci = GetFirstVisibleCharIndex(li), column = FirstVisibleCoords.C; ci < Lines[li].size() && column <= LastVisibleCoords.C;) {
            const char ch = line[ci];
            const ImVec2 glyph_pos = line_start_screen_pos + ImVec2{TextStart + TextDistanceToLineStart({li, column}, false), 0};
            if (ch == '\t') {
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
            } else if (ch == ' ') {
                if (ShowWhitespaces) {
                    dl->AddCircleFilled(
                        glyph_pos + ImVec2{space_size, ImGui::GetFontSize()} * 0.5f,
                        1.5f, Palette[int(PaletteIndex::ControlCharacter)], 4
                    );
                }
            } else {
                const uint seq_length = UTF8CharLength(ch);
                if (seq_length == 1 && MatchingBrackets && (MatchingBrackets->Start == Coords{li, column} || MatchingBrackets->End == Coords{li, column})) {
                    const ImVec2 top_left{glyph_pos + ImVec2{0, font_height + 1.0f}};
                    dl->AddRectFilled(top_left, top_left + ImVec2{CharAdvance.x, 1.0f}, Palette[int(PaletteIndex::Cursor)]);
                }
                string glyph_buffer;
                for (uint i = 0; i < seq_length; i++) glyph_buffer.push_back(line[ci + i]);
                dl->AddText(glyph_pos, GetGlyphColor({li, ci}), glyph_buffer.c_str());
            }
            MoveCharIndexAndColumn(li, ci, column);
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
            const auto target = SanitizeCoords(i > 0 ? cursor.Start : cursor.End);
            if (target.L <= FirstVisibleCoords.L) {
                float scroll = std::max(0.0f, (target.L - 0.5f) * CharAdvance.y);
                if (scroll < ScrollY) ImGui::SetScrollY(scroll);
            }
            if (target.L >= LastVisibleCoords.L) {
                float scroll = std::max(0.0f, (target.L + 1.5f) * CharAdvance.y - ContentHeight);
                if (scroll > ScrollY) ImGui::SetScrollY(scroll);
            }
            if (target.C <= FirstVisibleCoords.C) {
                if (target.C >= LastVisibleCoords.C) {
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
                scroll = std::max(0.0f, (float(SetViewAtLineI) - float((LastVisibleCoords - FirstVisibleCoords).L)) * CharAdvance.y);
                break;
            case SetViewAtLineMode::Centered:
                scroll = std::max(0.0f, (float(SetViewAtLineI) - float((LastVisibleCoords - FirstVisibleCoords).L) * 0.5f) * CharAdvance.y);
                break;
        }
        ImGui::SetScrollY(scroll);
        SetViewAtLineI = -1;
    }
}

uint TextEditor::ToByteIndex(LineChar lc) const {
    return ranges::accumulate(subrange(Lines.begin(), Lines.begin() + lc.L), 0u, [](uint sum, const auto &line) { return sum + line.size() + 1; }) + lc.C;
}

void TextEditor::OnTextChanged(LineChar start, LineChar old_end, LineChar new_end) {
    if (Tree) {
        TSInputEdit edit;
        // Seems we only need to provide the bytes (without points), despite the documentation: https://github.com/tree-sitter/tree-sitter/issues/445
        edit.start_byte = ToByteIndex(start);
        edit.old_end_byte = ToByteIndex(old_end);
        edit.new_end_byte = ToByteIndex(new_end);
        ts_tree_edit(Tree, &edit);
    }
    TextChanged = true;
}

void TextEditor::OnCursorPositionChanged() {
    const auto &c = State.Cursors[0];
    MatchingBrackets = State.Cursors.size() == 1 ? FindMatchingBrackets(c) : std::nullopt;

    if (!IsDraggingSelection) SortAndMergeCursors();
}

void TextEditor::SortAndMergeCursors() {
    if (State.Cursors.size() <= 1) return;

    // Sort cursors.
    const auto last_added_cursor_end = State.GetLastAddedCursor().End;
    std::ranges::sort(State.Cursors, [](const auto &a, const auto &b) { return a.SelectionStart() < b.SelectionStart(); });

    // Merge overlapping cursors.
    std::vector<Cursor> merged;
    Cursor current = State.Cursors.front();
    for (size_t c = 1; c < State.Cursors.size(); ++c) {
        const auto &next = State.Cursors[c];
        if (current.SelectionEnd() >= next.SelectionStart()) {
            // Overlap. Extend the current cursor to to include the next.
            const auto start = std::min(current.SelectionStart(), next.SelectionStart());
            const auto end = std::max(current.SelectionEnd(), next.SelectionEnd());
            current.Start = start;
            current.End = end;
        } else {
            // No overlap. Finalize the current cursor and start a new merge.
            merged.push_back(current);
            current = next;
        }
    }

    merged.push_back(current);
    State.Cursors = std::move(merged);

    // Update last added cursor index to be valid after sort/merge.
    const auto it = std::ranges::find_if(State.Cursors, [&last_added_cursor_end](const auto &c) { return c.End == last_added_cursor_end; });
    State.LastAddedCursorIndex = it != State.Cursors.end() ? std::distance(State.Cursors.begin(), it) : 0;
}

void TextEditor::AddUndo(UndoRecord &record) {
    if (record.Operations.empty()) return;

    record.After = State;
    UndoBuffer.resize(UndoIndex + 1);
    UndoBuffer.back() = record;
    ++UndoIndex;
}

TextEditor::Coords TextEditor::InsertTextAt(const Coords &at, const string &text) {
    const LineChar start = ToLineChar(at);
    LineChar end = start;
    for (auto it = text.begin(); it != text.end(); it++) {
        const char ch = *it;
        if (ch == '\r') continue;

        if (ch == '\n') {
            if (end.C < Lines[end.L].size()) {
                InsertLine(end.L + 1);
                const auto &line = Lines[end.L];
                AddGlyphs({end.L + 1, 0}, {line.cbegin() + end.C, line.cend()});
                RemoveGlyphs(end);
            } else {
                InsertLine(end.L + 1);
            }
            end.C = 0;
            end.L++;
        } else {
            std::vector<char> chars;
            for (uint d = 0; d < UTF8CharLength(ch) && it != text.end(); d++) {
                chars.emplace_back(*it);
                if (d > 0) it++;
            }
            AddGlyphs(end, chars);
            end.C += chars.size();
        }
    }

    OnTextChanged(start, start, end);
    return ToCoords(end);
}

const TextEditor::PaletteT TextEditor::DarkPalette = {{
    0xdcdfe4ff, // Default
    0xe06c75ff, // Keyword
    0xe5c07bff, // Number
    0x98c379ff, // String
    0xe0a070ff, // Char
    0x6a7384ff, // Punctuation
    0x808040ff, // Preprocessor
    0x61afefff, // Operator
    0xdcdfe4ff, // Identifier
    0xc678ddff, // Type
    0x3696a2ff, // Comment

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
    0xe0a070ff, // Char
    0x5fb4b4ff, // Punctuation
    0x808040ff, // Preprocessor
    0x4dc69bff, // Operator
    0xffffffff, // Identifier
    0xe0a0ffff, // Type
    0xa6acb9ff, // Comment (single line)

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
    0x404040ff, // Default
    0x060cffff, // Keyword
    0x008000ff, // Number
    0xa02020ff, // String
    0x704030ff, // Char
    0x000000ff, // Punctuation
    0x606040ff, // Preprocessor
    0x106060ff, // Operator
    0x404040ff, // Identifier
    0xa040c0ff, // Type
    0x205020ff, // Comment

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
    0xffff00ff, // Default
    0x00ffffff, // Keyword
    0x00ff00ff, // Number
    0x008080ff, // String
    0x008080ff, // Char
    0xffffffff, // Punctuation
    0x008000ff, // Preprocessor
    0xffffffff, // Operator
    0xffff00ff, // Identifier
    0xff00ffff, // Type
    0x808080ff, // Comment

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
