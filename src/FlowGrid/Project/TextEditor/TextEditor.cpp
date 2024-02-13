#include "TextEditor.h"

#include <algorithm>
#include <print>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>
#include <ranges>
#include <set>

#include "imgui_internal.h"
#include "nlohmann/json.hpp"
#include <tree_sitter/api.h>

#include "Application/ApplicationPreferences.h"
#include "Helper/File.h"
#include "Helper/Hex.h"
#include "Helper/Time.h"

using json = nlohmann::json;

using std::string, std::views::filter, std::ranges::reverse_view, std::ranges::any_of, std::ranges::all_of, std::ranges::subrange;

// Implemented by the grammar libraries in `lib/tree-sitter-grammars/`.
extern "C" TSLanguage *tree_sitter_cpp();
extern "C" TSLanguage *tree_sitter_faust();
extern "C" TSLanguage *tree_sitter_json();

TSQuery *LanguageDefinition::GetQuery() const {
    if (ShortName.empty()) return nullptr;

    const fs::path highlights_path = QueriesDir / ShortName / "highlights.scm";
    const string highlights = FileIO::read(highlights_path);

    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    auto *query = ts_query_new(
        TsLanguage,
        highlights.c_str(),
        highlights.size(),
        &error_offset,
        &error_type
    );
    // todo better error handling
    if (error_type != TSQueryErrorNone) {
        std::println("Error parsing tree-sitter query: {}", int(error_type));
    }
    return query;
}

LanguageDefinitions::LanguageDefinitions()
    : ById{
          {ID::None, {ID::None, "None"}},
          {ID::Cpp, {ID::Cpp, "C++", "cpp", tree_sitter_cpp(), {".h", ".hpp", ".cpp"}, "//"}},
          {ID::Faust, {ID::Faust, "Faust", "faust", tree_sitter_faust(), {".dsp"}, "//"}},
          {ID::Json, {ID::Json, "JSON", "json", tree_sitter_json(), {".json"}}},
      } {
    for (const auto &[id, lang] : ById) {
        for (const auto &ext : lang.FileExtensions) ByFileExtension[ext] = id;
    }
    for (const auto &ext : ByFileExtension | std::views::keys) AllFileExtensionsFilter += ext + ',';
}

constexpr ImU32 Col32(uint r, uint g, uint b, uint a = 255) { return IM_COL32(r, g, b, a); }

static ImU32 AnsiToRgb(uint code) {
    // Standard ANSI colors in hex, mapped directly to ImU32
    static const ImU32 StandardColors[16] = {
        Col32(0, 0, 0), // Black
        Col32(128, 0, 0), // Red
        Col32(0, 128, 0), // Green
        Col32(128, 128, 0), // Yellow
        Col32(0, 0, 128), // Blue
        Col32(128, 0, 128), // Magenta
        Col32(0, 128, 128), // Cyan
        Col32(192, 192, 192), // White
        Col32(128, 128, 128), // Black (bright)
        Col32(255, 0, 0), // Red (bright)
        Col32(0, 255, 0), // Green (bright)
        Col32(255, 255, 0), // Yellow (bright)
        Col32(0, 0, 255), // Blue (bright)
        Col32(255, 0, 255), // Magenta (bright)
        Col32(0, 255, 255), // Cyan (bright)
        Col32(255, 255, 255), // White (bright)
    };

    if (code < 16) return StandardColors[code];
    // All codes >= 16 are left up to the terminal implementation.
    // The following is a programmatic strategy to convert the >= 16 range to RGB.
    if (code < 232) {
        // 6x6x6 color cube
        static const uint step = 255 / 5;
        const uint red = (code - 16) / 36, green = (code - 16) / 6 % 6, blue = (code - 16) % 6;
        return IM_COL32(red * step, green * step, blue * step, 255);
    }
    if (code <= 255) {
        // Grayscale ramp, starts at 8 and increases by 10 up to 238.
        const uint shade = 8 + (code - 232) * 10;
        return IM_COL32(shade, shade, shade, 255);
    }
    return IM_COL32(0, 0, 0, 255); // Default to black if out of range.
}

ImU32 CharStyleColorValuetoU32(const json &j) {
    static const std::unordered_map<std::string, ImU32> ColorByName = {
        {"black", Col32(0, 0, 0)},
        {"blue", Col32(0, 0, 255)},
        {"cyan", Col32(0, 255, 255)},
        {"green", Col32(0, 255, 0)},
        {"purple", Col32(128, 0, 128)},
        {"red", Col32(255, 0, 0)},
        {"white", Col32(255, 255, 255)},
        {"yellow", Col32(255, 255, 0)},
    };

    if (j.is_string()) {
        const auto str_value = j.get<std::string>();
        if (IsHex(str_value)) return HexToU32(str_value);
        if (auto it = ColorByName.find(str_value); it != ColorByName.end()) return it->second;
        throw std::runtime_error("Unsupported color name in tree-sitter config JSON.");
    }
    if (j.is_number()) return AnsiToRgb(j.get<uint>());

    throw std::runtime_error("Invalid color type in tree-sitter config JSON.");
}

void from_json(const json &j, TextEditorStyle::CharStyle &style) {
    if (j.is_object()) {
        if (j.contains("color")) style.Color = CharStyleColorValuetoU32(j.at("color"));
        if (j.contains("bold")) j.at("bold").get_to(style.Bold);
        if (j.contains("italic")) j.at("italic").get_to(style.Italic);
    } else if (j.is_number() || j.is_string()) {
        style.Color = CharStyleColorValuetoU32(j);
    } else {
        throw std::runtime_error("Invalid theme style type in tree-sitter config JSON.");
    }
}
void from_json(const json &j, TSConfig &config) {
    j.at("parser-directories").get_to(config.ParserDirectories);
    const auto &theme = j.at("theme");
    for (const auto &[key, value] : theme.items()) {
        if (!value.is_null()) {
            config.StyleByHighlightName[key] = value.get<TextEditorStyle::CharStyle>();
        }
    }
}

TextEditor::TextEditor(std::string_view text, LanguageID language_id) : Parser(ts_parser_new()) {
    SetLanguage(language_id);
    SetText(string(text));
    Commit();
}
TextEditor::TextEditor(const fs::path &file_path) : Parser(ts_parser_new()) {
    SetFilePath(file_path);
    SetText(FileIO::read(file_path));
    Commit();
}

TextEditor::~TextEditor() {
    if (QueryCursor) ts_query_cursor_delete(QueryCursor);
    if (Parser) ts_parser_delete(Parser);
    if (Tree) ts_tree_delete(Tree);
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
    return &line.front() + position.column;
}

static TextEditor::ByteRange ToByteRange(const TSNode &node) { return {ts_node_start_byte(node), ts_node_end_byte(node)}; }

void TextEditor::ApplyEdits() {
    // xxx Apply edits one at a time for now.
    // E.g. moving lines (which is an insert and a delete) currently breaks highlighting when applied together.
    // But moving lines is still broken for undo after minimal query updates.
    for (const auto &edit : Edits) ApplyEdits({edit});
    Edits.clear();
}

void TextEditor::ApplyEdits(const std::set<InputEdit> &edits) {
    ChangedCaptureRanges.clear(); // For debugging
    if (edits.empty()) return;

    if (Tree != nullptr) {
        for (const auto &edit : edits) {
            const TSInputEdit ts_edit{.start_byte = edit.StartByte, .old_end_byte = edit.OldEndByte, .new_end_byte = edit.NewEndByte};
            ts_tree_edit(Tree, &ts_edit);
        }
    }

    auto *old_tree = Tree;
    Tree = ts_parser_parse(Parser, Tree, {this, TSReadText, TSInputEncodingUTF8});
    if (!Query) return;

    /* Update capture ID transition points (used for highlighting) based on the query and the edits. */

    // Find the minimum range needed to span all nodes whose syntactic structure has changed.
    ByteRange changed_range = {UINT32_MAX, 0u};
    if (old_tree != nullptr) {
        uint num_changed_ranges;
        const TSRange *changed_ranges = ts_tree_get_changed_ranges(old_tree, Tree, &num_changed_ranges);
        for (uint i = 0; i < num_changed_ranges; ++i) {
            changed_range.Start = std::min(changed_range.Start, changed_ranges[i].start_byte);
            changed_range.End = std::max(changed_range.End, changed_ranges[i].end_byte);
        }
        free((void *)changed_ranges);
    }

    const bool any_changed_captures = changed_range.Start < changed_range.End;
    if (any_changed_captures) {
        // std::println("Changed capture range: [{},{}]", changed_range.Start, changed_range.End);
        ts_query_cursor_set_byte_range(QueryCursor, changed_range.Start, changed_range.End);

        // Note: We don't delete all transitions in this range, since it might include ancestor nodes
        // with transitions that are still valid. We delete within replaced terminal node ranges below.
    }

    auto transition_it = CaptureIdTransitions.begin();

    // Delete all transitions within deleted ranges.
    for (const auto &edit : reverse_view(edits) | filter([](const auto &edit) { return edit.IsDelete(); })) {
        CaptureIdTransitions.Delete(transition_it, edit.NewEndByte, edit.OldEndByte);
    }
    // Adjust transitions based on the edited ranges.
    if (CaptureIdTransitions.size() > 1) {
        for (const auto &edit : reverse_view(edits)) {
            transition_it.MoveTo(edit.StartByte);
            CaptureIdTransitions.Increment(transition_it, edit.NewEndByte - edit.OldEndByte);
        }
    }

    if (old_tree != nullptr && !any_changed_captures) return;

    // Either this is the first parse, or the edit(s) affect existing node captures.
    // Execute the query and add all capture transitions.
    ts_query_cursor_exec(QueryCursor, Query, ts_tree_root_node(Tree));

    TSQueryMatch match;
    uint capture_index;
    while (ts_query_cursor_next_capture(QueryCursor, &match, &capture_index)) {
        const TSQueryCapture &capture = match.captures[capture_index];
        // We only store the points at which there is a _transition_ from one style to another.
        // This can happen either at the beginning or the end of the capture node.
        TSNode node = capture.node;
        const auto node_byte_range = ToByteRange(node);
        ChangedCaptureRanges.insert(node_byte_range); // For debugging.
        if (ts_node_child_count(node) > 0) continue; // Only highlight terminal nodes.

        // Delete invalidated transitions.
        CaptureIdTransitions.Delete(transition_it, node_byte_range.Start, node_byte_range.End);

        if (*transition_it != capture.index) {
            // uint length;
            // const char *capture_name = ts_query_capture_name_for_id(Query, capture.index, &length);
            // std::println("\t'{}'[{}:{}]: {}", ts_node_type(node), node_byte_range.Start, node_byte_range.End, string(capture_name, length));
            CaptureIdTransitions.Insert(transition_it, node_byte_range.Start, capture.index);
            if (node_byte_range.End != changed_range.End) {
                CaptureIdTransitions.Insert(transition_it, node_byte_range.End, NoneCaptureId);
            }
        }
    }
}

string TextEditor::GetSyntaxTreeSExp() const {
    char *c_string = ts_node_string(ts_tree_root_node(Tree));
    string s_expression(c_string);
    free(c_string);
    return s_expression;
}

const TextEditor::PaletteT &TextEditor::GetPalette() const {
    switch (PaletteId) {
        case PaletteIdT::Dark: return DarkPalette;
        case PaletteIdT::Light: return LightPalette;
        case PaletteIdT::Mariana: return MarianaPalette;
        case PaletteIdT::RetroBlue: return RetroBluePalette;
    }
}

void TextEditor::SetPalette(PaletteIdT palette_id) { PaletteId = palette_id; }

void TextEditor::SetLanguage(LanguageID language_id) {
    if (LanguageId == language_id) return;

    LanguageId = language_id;
    if (LanguageId != LanguageID::None && !Preferences.TreeSitterConfigPath.empty()) {
        const auto config_json = json::parse(FileIO::read(Preferences.TreeSitterConfigPath));
        HighlightConfig = config_json.get<TSConfig>();
    } else if (LanguageId == LanguageID::None) {
        HighlightConfig = {};
    }
    ts_parser_set_language(Parser, GetLanguage().TsLanguage);
    Query = GetLanguage().GetQuery();
    const uint capture_count = ts_query_capture_count(Query);
    StyleByCaptureId.clear();
    StyleByCaptureId.reserve(capture_count + 1);
    StyleByCaptureId[NoneCaptureId] = HighlightConfig.DefaultCharStyle;
    for (uint i = 0; i < capture_count; ++i) {
        uint length;
        const char *capture_name = ts_query_capture_name_for_id(Query, i, &length);
        StyleByCaptureId[i] = HighlightConfig.FindStyleByCaptureName(string(capture_name, length));
    }

    Tree = nullptr;
    QueryCursor = ts_query_cursor_new();
    CaptureIdTransitions.clear();
    ApplyEdits();
}

void TextEditor::SetNumTabSpaces(uint tab_size) { NumTabSpaces = std::clamp(tab_size, 1u, 8u); }
void TextEditor::SetLineSpacing(float line_spacing) { LineSpacing = std::clamp(line_spacing, 1.f, 2.f); }

void TextEditor::SelectAll() {
    Cursors.Reset();
    Cursors.MoveTop();
    Cursors.MoveBottom(*this, true);
}

void TextEditor::Copy() {
    const string str = Cursors.AnyRanged() ?
        Cursors |
            // Using range-v3 here since clang's libc++ doesn't yet implement `join_with` (which is just `join` in range-v3).
            ranges::views::filter([](const auto &c) { return c.IsRange(); }) |
            ranges::views::transform([this](const auto &c) { return GetSelectedText(c); }) |
            ranges::views::join('\n') | ranges::to<string> :
        Text[GetCursorPosition().L] | ranges::to<string>;
    ImGui::SetClipboardText(str.c_str());
}

void TextEditor::Cut() {
    if (!Cursors.AnyRanged()) return;

    BeforeCursors = Cursors;
    Copy();
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
    Commit();
}

bool TextEditor::CanPaste() const { return !ReadOnly && ImGui::GetClipboardText() != nullptr; }

void TextEditor::Paste() {
    // todo store clipboard text manually in a `Lines`?
    const char *clip_text = ImGui::GetClipboardText();
    if (*clip_text == '\0') return;

    BeforeCursors = Cursors;

    TransientLines insert_text_lines_trans{};
    const char *ptr = clip_text;
    while (*ptr != '\0') {
        uint str_length = 0;
        while (ptr[str_length] != '\n' && ptr[str_length] != '\0') ++str_length;
        insert_text_lines_trans.push_back({ptr, ptr + str_length});
        // Special case: Last pasted char is a newline.
        if (*(ptr + str_length) == '\n' && *(ptr + str_length + 1) == '\0') insert_text_lines_trans.push_back({});
        ptr += str_length + 1;
    }
    const auto insert_text_lines = insert_text_lines_trans.persistent();
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
    if (Cursors.size() > 1 && insert_text_lines.size() == Cursors.size()) {
        // Paste each line at the corresponding cursor.
        for (int c = Cursors.size() - 1; c > -1; --c) InsertTextAtCursor({insert_text_lines[c]}, Cursors[c]);
    } else {
        for (auto &c : reverse_view(Cursors)) InsertTextAtCursor(insert_text_lines, c);
    }
    Commit();
}

void TextEditor::Undo() {
    if (!CanUndo()) return;

    const auto &current = History[HistoryIndex];
    const auto restore = History[--HistoryIndex];
    Text = restore.Text;
    Cursors = current.BeforeCursors;
    Cursors.MarkEdited();
    assert(Edits.empty());
    for (const auto &edit : reverse_view(current.Edits)) Edits.push_back(edit.Invert());
    ApplyEdits();
}
void TextEditor::Redo() {
    if (!CanRedo()) return;

    const auto restore = History[++HistoryIndex];
    Text = restore.Text;
    Cursors = restore.Cursors;
    Cursors.MarkEdited();
    assert(Edits.empty());
    Edits = restore.Edits;
    ApplyEdits();
}

void TextEditor::Commit() {
    if (Edits.empty()) return;

    History = History.take(++HistoryIndex);
    History = History.push_back({Text, Cursors, BeforeCursors, Edits});
    ApplyEdits();
}

void TextEditor::SetText(const string &text) {
    const uint old_end_byte = EndByteIndex();
    TransientLines transient_lines{};
    TransientLine current_line{};
    for (auto ch : text) {
        if (ch == '\r') continue; // Ignore the carriage return character.
        if (ch == '\n') {
            transient_lines.push_back(current_line.persistent());
            current_line = {};
        } else {
            current_line.push_back(ch);
        }
    }
    transient_lines.push_back(current_line.persistent());
    Text = transient_lines.persistent();

    ScrollToTop = true;

    History = {};
    HistoryIndex = -1;

    Edits.emplace_back(0, old_end_byte, EndByteIndex());
}

void TextEditor::SetFilePath(const fs::path &file_path) {
    const string extension = file_path.extension();
    SetLanguage(!extension.empty() && Languages.ByFileExtension.contains(extension) ? Languages.ByFileExtension.at(extension) : LanguageID::None);
}

string TextEditor::GetText(LineChar start, LineChar end) const {
    if (end <= start) return "";

    const uint start_li = start.L, end_li = std::min(uint(Text.size()) - 1, end.L);
    const uint start_ci = start.C, end_ci = end.C;
    string result;
    for (uint ci = start_ci, li = start_li; li < end_li || ci < end_ci;) {
        const auto &line = Text[li];
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

static bool IsUTFSequence(char c) { return (c & 0xC0) == 0x80; }

static bool IsWordChar(char ch) {
    return UTF8CharLength(ch) > 1 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

bool TextEditor::Cursors::AnyRanged() const {
    return any_of(Cursors, [](const auto &c) { return c.IsRange(); });
}
bool TextEditor::Cursors::AllRanged() const {
    return all_of(Cursors, [](const auto &c) { return c.IsRange(); });
}
bool TextEditor::Cursors::AnyMultiline() const {
    return any_of(Cursors, [](const auto &c) { return c.IsMultiline(); });
}
bool TextEditor::Cursors::AnyEdited() const {
    return any_of(Cursors, [](const auto &c) { return c.IsEdited(); });
}

void TextEditor::Cursors::Add() {
    Cursors.push_back({});
    LastAddedIndex = size() - 1;
}
void TextEditor::Cursors::Reset() {
    Cursors.clear();
    Add();
}
void TextEditor::Cursors::SortAndMerge() {
    if (size() <= 1) return;

    // Sort cursors.
    const auto last_added_cursor_lc = GetLastAdded().LC();
    std::ranges::sort(Cursors, [](const auto &a, const auto &b) { return a.Min() < b.Min(); });

    // Merge overlapping cursors.
    std::vector<Cursor> merged;
    Cursor current = front();
    for (size_t c = 1; c < size(); ++c) {
        const auto &next = (*this)[c];
        if (current.Max() >= next.Min()) {
            // Overlap. Extend the current cursor to to include the next.
            const auto start = std::min(current.Min(), next.Min());
            const auto end = std::max(current.Max(), next.Max());
            current.Set(start, end);
        } else {
            // No overlap. Finalize the current cursor and start a new merge.
            merged.push_back(current);
            current = next;
        }
    }

    merged.push_back(current);
    Cursors = std::move(merged);

    // Update last added cursor index to be valid after sort/merge.
    const auto it = std::ranges::find_if(*this, [&last_added_cursor_lc](const auto &c) { return c.LC() == last_added_cursor_lc; });
    LastAddedIndex = it != end() ? std::distance(begin(), it) : 0;
}

std::optional<TextEditor::Cursor> TextEditor::Cursors::GetEditedCursor() {
    if (!AnyEdited()) return {};

    Cursor edited_range;
    for (auto &c : Cursors) {
        if (c.IsEdited()) {
            edited_range = c;
            break; // todo create a sensible cursor representing the combined range when multiple cursors are edited.
        }
    }
    return edited_range;
}

void TextEditor::InsertTextAtCursor(Lines text, Cursor &c) {
    if (!text.empty()) c.Set(InsertText(text, c.Min()));
}

void TextEditor::LinesIter::MoveRight() {
    if (LC == End) return;

    if (LC.C == Text[LC.L].size()) {
        ++LC.L;
        LC.C = 0;
    } else {
        LC.C = std::min(LC.C + UTF8CharLength(Text[LC.L][LC.C]), uint(Text[LC.L].size()));
    }
}

void TextEditor::LinesIter::MoveLeft() {
    if (LC == Begin) return;

    if (LC.C == 0) {
        --LC.L;
        LC.C = Text[LC.L].size();
    } else {
        do { --LC.C; } while (LC.C > 0 && IsUTFSequence(Text[LC.L][LC.C]));
    }
}

static uint NextTabstop(uint column, uint tab_size) { return ((column / tab_size) + 1) * tab_size; }

void TextEditor::MoveCharIndexAndColumn(const Line &line, uint &ci, uint &column) const {
    const char ch = line[ci];
    ci += UTF8CharLength(ch);
    column = ch == '\t' ? NextTabstop(column, NumTabSpaces) : column + 1;
}

uint TextEditor::NumStartingSpaceColumns(uint li) const {
    const auto &line = Text[li];
    uint column = 0;
    for (uint ci = 0; ci < line.size() && isblank(line[ci]);) MoveCharIndexAndColumn(line, ci, column);
    return column;
}

void TextEditor::Cursor::MoveLines(const TextEditor &editor, int amount, bool select, bool move_start, bool move_end) {
    if (!move_start && !move_end) return;

    // Track the cursor's column to return back to it after moving to a line long enough.
    // (This is the only place we worry about this.)
    const uint new_end_column = EndColumn.value_or(editor.ToCoords(End).C);
    const uint new_end_li = std::clamp(int(End.L) + amount, 0, int(editor.LineCount() - 1));
    const LineChar new_end{
        new_end_li,
        std::min(editor.GetCharIndex({new_end_li, new_end_column}), editor.GetLineMaxCharIndex(new_end_li)),
    };
    if (!select) return Set(new_end, true, new_end_column);
    if (!move_start) return Set(new_end, false, new_end_column);
    const uint new_start_column = StartColumn.value_or(editor.ToCoords(Start).C);
    const uint new_start_li = std::clamp(int(Start.L) + amount, 0, int(editor.LineCount() - 1));
    const LineChar new_start{
        new_start_li,
        std::min(editor.GetCharIndex({new_start_li, new_start_column}), editor.GetLineMaxCharIndex(new_start_li)),
    };
    Set(new_start, new_end, new_start_column, new_end_column);
}

void TextEditor::Cursor::MoveChar(const TextEditor &editor, bool right, bool select, bool is_word_mode) {
    auto lci = editor.Iter(LC());
    if ((!right && !lci.IsBegin()) || (right && !lci.IsEnd())) {
        if (right) ++lci;
        else --lci;
        Set(is_word_mode ? editor.FindWordBoundary(*lci, !right) : *lci, !select);
    }
}

void TextEditor::Cursors::MoveLines(const TextEditor &editor, int amount, bool select, bool move_start, bool move_end) {
    for (auto &c : Cursors) c.MoveLines(editor, amount, select, move_start, move_end);
}
void TextEditor::Cursors::MoveChar(const TextEditor &editor, bool right, bool select, bool is_word_mode) {
    const bool any_selections = AnyRanged();
    for (auto &c : Cursors) {
        if (any_selections && !select && !is_word_mode) c.Set(right ? c.Max() : c.Min(), true);
        else c.MoveChar(editor, right, select, is_word_mode);
    }
}
void TextEditor::Cursors::MoveTop(bool select) { Cursors.back().Set({0, 0}, !select); }
void TextEditor::Cursors::MoveBottom(const TextEditor &editor, bool select) { Cursors.back().Set(editor.LineMaxLC(editor.LineCount() - 1), !select); }
void TextEditor::Cursors::MoveStart(bool select) {
    for (auto &c : Cursors) c.Set({c.Line(), 0}, !select);
}
void TextEditor::Cursors::MoveEnd(const TextEditor &editor, bool select) {
    for (auto &c : Cursors) c.Set(editor.LineMaxLC(c.Line()), !select);
}

void TextEditor::EnterChar(ImWchar ch, bool is_shift) {
    if (ch == '\t' && Cursors.AnyMultiline()) return ChangeCurrentLinesIndentation(!is_shift);

    BeforeCursors = Cursors;
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);

    // Order is important here for typing '\n' in the same line with multiple cursors.
    for (auto &c : reverse_view(Cursors)) {
        TransientLine insert_line_trans{};
        if (ch == '\n') {
            if (AutoIndent && c.CharIndex() != 0) {
                // Match the indentation of the current or next line, whichever has more indentation.
                // todo use tree-sitter fold queries
                const uint li = c.Line();
                const uint indent_li = li < Text.size() - 1 && NumStartingSpaceColumns(li + 1) > NumStartingSpaceColumns(li) ? li + 1 : li;
                const auto &indent_line = Text[indent_li];
                for (uint i = 0; i < insert_line_trans.size() && isblank(indent_line[i]); ++i) insert_line_trans.push_back(indent_line[i]);
            }
        } else {
            char buf[5];
            ImTextCharToUtf8(buf, ch);
            for (uint i = 0; i < 5 && buf[i] != '\0'; ++i) insert_line_trans.push_back(buf[i]);
        }
        auto insert_line = insert_line_trans.persistent();
        InsertTextAtCursor(ch == '\n' ? Lines{{}, insert_line} : Lines{insert_line}, c);
    }

    Commit();
}

void TextEditor::Backspace(bool is_word_mode) {
    BeforeCursors = Cursors;
    if (!Cursors.AnyRanged()) {
        Cursors.MoveChar(*this, false, true, is_word_mode);
        // Can't do backspace if any cursor is at {0,0}.
        if (!Cursors.AllRanged()) {
            if (Cursors.AnyRanged()) Cursors.MoveChar(*this, true); // Restore cursors.
            return;
        }
        Cursors.SortAndMerge();
    }
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
    Commit();
}

void TextEditor::Delete(bool is_word_mode) {
    BeforeCursors = Cursors;
    if (!Cursors.AnyRanged()) {
        Cursors.MoveChar(*this, true, true, is_word_mode);
        // Can't do delete if any cursor is at the end of the last line.
        if (!Cursors.AllRanged()) {
            if (Cursors.AnyRanged()) Cursors.MoveChar(*this, false); // Restore cursors.
            return;
        }
        Cursors.SortAndMerge();
    }
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
    Commit();
}

void TextEditor::SetSelection(LineChar start, LineChar end, Cursor &c) {
    const LineChar min_lc{0, 0}, max_lc{LineMaxLC(Text.size() - 1)};
    c.Set(std::clamp(start, min_lc, max_lc), std::clamp(end, min_lc, max_lc));
}

void TextEditor::AddCursorForNextOccurrence(bool case_sensitive) {
    const auto &c = Cursors.GetLastAdded();
    if (const auto match_range = FindNextOccurrence(GetSelectedText(c), c.Max(), case_sensitive)) {
        Cursors.Add();
        SetSelection(match_range->GetStart(), match_range->GetEnd(), Cursors.back());
        Cursors.SortAndMerge();
    }
}

static char ToLower(char ch, bool case_sensitive) { return (!case_sensitive && ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch; }

std::optional<TextEditor::Cursor> TextEditor::FindNextOccurrence(const string &text, LineChar start, bool case_sensitive) {
    if (text.empty()) return {};

    auto find_lci = Iter(start);
    do {
        auto match_lci = find_lci;
        for (uint i = 0; i < text.size(); ++i) {
            const auto &match_lc = *match_lci;
            if (match_lc.C == Text[match_lc.L].size()) {
                if (text[i] != '\n' || match_lc.L + 1 >= Text.size()) break;
            } else if (ToLower(match_lci, case_sensitive) != ToLower(text[i], case_sensitive)) {
                break;
            }

            ++match_lci;
            if (i == text.size() - 1) return Cursor{*find_lci, *match_lci};
        }

        ++find_lci;
        if (find_lci.IsEnd()) find_lci.Reset();
    } while (*find_lci != start);

    return {};
}

std::optional<TextEditor::Cursor> TextEditor::FindMatchingBrackets(const Cursor &c) {
    static const std::unordered_map<char, char>
        OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
        CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

    const uint li = c.Line();
    const auto &line = Text[li];
    if (c.IsRange() || line.empty()) return {};

    uint ci = c.CharIndex();
    // Considered on bracket if cursor is to the left or right of it.
    if (ci > 0 && (CloseToOpenChar.contains(line[ci - 1]) || OpenToCloseChar.contains(line[ci - 1]))) --ci;

    const char ch = line[ci];
    const bool is_close_char = CloseToOpenChar.contains(ch), is_open_char = OpenToCloseChar.contains(ch);
    if (!is_close_char && !is_open_char) return {};

    const LineChar lc{li, ci};
    const char other_ch = is_close_char ? CloseToOpenChar.at(ch) : OpenToCloseChar.at(ch);
    uint match_count = 0;
    for (auto lci = Iter(lc); is_close_char ? !lci.IsBegin() : !lci.IsEnd(); is_close_char ? --lci : ++lci) {
        const char ch_inner = lci;
        if (ch_inner == ch) ++match_count;
        else if (ch_inner == other_ch && (match_count == 0 || --match_count == 0)) return Cursor{lc, *lci};
    }

    return {};
}

void TextEditor::ChangeCurrentLinesIndentation(bool increase) {
    BeforeCursors = Cursors;
    for (const auto &c : reverse_view(Cursors)) {
        for (uint li = c.Min().L; li <= c.Max().L; ++li) {
            // Check if selection ends at line start.
            if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

            const auto &line = Text[li];
            if (increase) {
                if (!line.empty()) InsertText({{'\t'}}, {li, 0});
            } else {
                int ci = int(GetCharIndex(line, NumTabSpaces)) - 1;
                while (ci > -1 && isblank(line[ci])) --ci;
                const bool only_space_chars_found = ci == -1;
                if (only_space_chars_found) DeleteRange({li, 0}, {li, GetCharIndex(line, NumTabSpaces)});
            }
        }
    }
    Commit();
}

void TextEditor::SwapLines(uint li1, uint li2) {
    if (li1 == li2 || li1 >= Text.size() || li2 >= Text.size()) return;

    InsertText({Text[li2], {}}, {li1, 0}, false);
    if (li2 + 1 < Text.size() - 1) DeleteRange({li2 + 1, 0}, {li2 + 2, 0}, false);
    // If the second line is the last line, we also need to delete the newline we just inserted.
    else DeleteRange({li2, uint(Text[li2].size())}, EndLC(), false);
}

void TextEditor::MoveCurrentLines(bool up) {
    BeforeCursors = Cursors;
    std::set<uint> affected_lines;
    uint min_li = std::numeric_limits<uint>::max(), max_li = std::numeric_limits<uint>::min();
    for (const auto &c : Cursors) {
        for (uint li = c.Min().L; li <= c.Max().L; ++li) {
            // Check if selection ends at line start.
            if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

            affected_lines.insert(li);
            min_li = std::min(li, min_li);
            max_li = std::max(li, max_li);
        }
    }
    if ((up && min_li == 0) || (!up && max_li == Text.size() - 1)) return; // Can't move up/down anymore.

    if (up) {
        for (const uint li : affected_lines) SwapLines(li - 1, li);
    } else {
        for (auto it = affected_lines.rbegin(); it != affected_lines.rend(); ++it) SwapLines(*it, *it + 1);
    }
    Cursors.MoveLines(*this, up ? -1 : 1, true, true, true);

    Commit();
}

static bool Equals(const auto &c1, const auto &c2, std::size_t c2_offset = 0) {
    if (c2.size() + c2_offset < c1.size()) return false;

    const auto begin = c2.begin() + c2_offset;
    return std::ranges::equal(c1, subrange(begin, begin + c1.size()));
}

void TextEditor::ToggleLineComment() {
    const string &comment = GetLanguage().SingleLineComment;
    if (comment.empty()) return;

    static const auto FindFirstNonSpace = [](const Line &line) {
        return std::distance(line.begin(), std::ranges::find_if_not(line, isblank));
    };

    std::unordered_set<uint> affected_lines;
    for (const auto &c : Cursors) {
        for (uint li = c.Min().L; li <= c.Max().L; ++li) {
            if (!(c.IsRange() && c.Max() == LineChar{li, 0}) && !Text[li].empty()) affected_lines.insert(li);
        }
    }

    const bool should_add_comment = any_of(affected_lines, [&](uint li) {
        return !Equals(comment, Text[li], FindFirstNonSpace(Text[li]));
    });

    BeforeCursors = Cursors;
    for (uint li : affected_lines) {
        if (should_add_comment) {
            InsertText({Line{comment.begin(), comment.end()} + Line{' '}}, {li, 0});
        } else {
            const auto &line = Text[li];
            const uint ci = FindFirstNonSpace(line);
            uint comment_ci = ci + comment.length();
            if (comment_ci < line.size() && line[comment_ci] == ' ') ++comment_ci;

            DeleteRange({li, ci}, {li, comment_ci});
        }
    }
    Commit();
}

void TextEditor::RemoveCurrentLines() {
    BeforeCursors = Cursors;
    for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
    Cursors.MoveStart();
    Cursors.SortAndMerge();

    for (const auto &c : reverse_view(Cursors)) {
        const uint li = c.Line();
        DeleteRange(
            li == Text.size() - 1 && li > 0 ? LineMaxLC(li - 1) : LineChar{li, 0},
            CheckedNextLineBegin(li)
        );
    }
    Commit();
}

TextEditor::Coords TextEditor::ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li) const {
    static constexpr float PosToCoordsColumnOffset = 0.33;

    const auto local = ImVec2{screen_pos.x + 3.0f, screen_pos.y} - ImGui::GetCursorScreenPos();
    if (is_over_li != nullptr) *is_over_li = local.x < TextStart;

    Coords coords{
        std::min(uint(std::max(0.f, floor(local.y / CharAdvance.y))), uint(Text.size()) - 1),
        uint(std::max(0.f, floor((local.x - TextStart + PosToCoordsColumnOffset * CharAdvance.x) / CharAdvance.x)))
    };
    // Check if the coord is in the middle of a tab character.
    const uint li = std::min(coords.L, uint(Text.size()) - 1);
    const auto &line = Text[li];
    const uint ci = GetCharIndex(line, coords.C);
    if (ci < line.size() && line[ci] == '\t') coords.C = GetColumn(line, ci);

    return {coords.L, GetLineMaxColumn(line, coords.C)};
}

TextEditor::LineChar TextEditor::FindWordBoundary(LineChar from, bool is_start) const {
    if (from.L >= Text.size()) return from;

    const auto &line = Text[from.L];
    uint ci = from.C;
    if (ci >= line.size()) return from;

    const char init_char = line[ci];
    const bool init_is_word_char = IsWordChar(init_char), init_is_space = isspace(init_char);
    for (; is_start ? ci > 0 : ci < line.size(); is_start ? --ci : ++ci) {
        if (ci == line.size() ||
            (init_is_space && !isspace(line[ci])) ||
            (init_is_word_char && !IsWordChar(line[ci])) ||
            (!init_is_word_char && !init_is_space && init_char != line[ci])) {
            if (is_start) ++ci; // Undo one left step before returning line/char.
            break;
        }
    }
    return {from.L, ci};
}

uint TextEditor::GetCharIndex(const Line &line, uint column) const {
    uint ci = 0;
    for (uint column_i = 0; ci < line.size() && column_i < column;) MoveCharIndexAndColumn(line, ci, column_i);
    return ci;
}

uint TextEditor::GetColumn(const Line &line, uint ci) const {
    uint column = 0;
    for (uint ci_i = 0; ci_i < ci && ci_i < line.size();) MoveCharIndexAndColumn(line, ci_i, column);
    return column;
}

uint TextEditor::GetFirstVisibleCharIndex(const Line &line, uint first_visible_column) const {
    uint ci = 0, column = 0;
    while (column < first_visible_column && ci < line.size()) MoveCharIndexAndColumn(line, ci, column);
    return column > first_visible_column && ci > 0 ? ci - 1 : ci;
}

uint TextEditor::GetLineMaxColumn(const Line &line) const {
    uint column = 0;
    for (uint ci = 0; ci < line.size();) MoveCharIndexAndColumn(line, ci, column);
    return column;
}
uint TextEditor::GetLineMaxColumn(const Line &line, uint limit) const {
    uint column = 0;
    for (uint ci = 0; ci < line.size() && column < limit;) MoveCharIndexAndColumn(line, ci, column);
    return column;
}

TextEditor::LineChar TextEditor::InsertText(Lines text, LineChar at, bool update_cursors) {
    if (text.empty()) return at;

    if (at.L < Text.size()) {
        auto ln1 = Text[at.L];
        Text = Text.set(at.L, ln1.take(at.C) + text[0]);
        Text = Text.take(at.L + 1) + text.drop(1) + Text.drop(at.L + 1);
        auto ln2 = Text[at.L + text.size() - 1];
        Text = Text.set(at.L + text.size() - 1, ln2 + ln1.drop(at.C));
    } else {
        Text = Text + text;
    }

    const uint num_new_lines = text.size() - 1;
    if (update_cursors) {
        auto cursors_below = Cursors | filter([&](const auto &c) { return c.Line() > at.L; });
        for (auto &c : cursors_below) c.Set({c.Line() + num_new_lines, c.CharIndex()});
    }

    const uint start_byte = ToByteIndex(at);
    const uint text_byte_length = std::accumulate(text.begin(), text.end(), 0, [](uint sum, const auto &line) { return sum + line.size(); }) + text.size() - 1;
    Edits.emplace_back(start_byte, start_byte, start_byte + text_byte_length);

    return LineChar{at.L + num_new_lines, text.size() == 1 ? uint(at.C + text.front().size()) : uint(text.back().size())};
}

void TextEditor::DeleteRange(LineChar start, LineChar end, bool update_cursors, const Cursor *exclude_cursor) {
    if (end <= start) return;

    auto start_line = Text[start.L], end_line = Text[end.L];
    const uint start_byte = ToByteIndex(start), old_end_byte = ToByteIndex(end);
    if (start.L == end.L) {
        Text = Text.set(start.L, start_line.erase(start.C, end.C));

        if (update_cursors) {
            auto cursors_to_right = Cursors | filter([&start](const auto &c) { return !c.IsRange() && c.IsRightOf(start); });
            for (auto &c : cursors_to_right) c.Set({c.Line(), uint(c.CharIndex() - (end.C - start.C))});
        }
    } else {
        end_line = end_line.drop(end.C);
        Text = Text.set(end.L, end_line);
        Text = Text.set(start.L, start_line.take(start.C) + end_line);
        Text = Text.erase(start.L + 1, end.L + 1);

        if (update_cursors) {
            auto cursors_below = Cursors | filter([&](const auto &c) { return (!exclude_cursor || c != *exclude_cursor) && c.Line() >= end.L; });
            for (auto &c : cursors_below) c.Set({c.Line() - (end.L - start.L), c.CharIndex()});
        }
    }

    Edits.emplace_back(start_byte, old_end_byte, start_byte);
}

void TextEditor::DeleteSelection(Cursor &c) {
    if (!c.IsRange()) return;

    // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
    DeleteRange(c.Min(), c.Max(), true, &c);
    c.Set(c.Min());
}

uint TextEditor::ToByteIndex(LineChar lc) const {
    if (lc.L >= Text.size()) return EndByteIndex();
    return ranges::accumulate(subrange(Text.begin(), Text.begin() + lc.L), 0u, [](uint sum, const auto &line) { return sum + line.size() + 1; }) + lc.C;
}

/**
From the [tree-sitter docs](https://tree-sitter.github.io/tree-sitter/syntax-highlighting#theme):
A theme can contain multiple keys that share a common subsequence.
Examples:
- 'variable' and 'variable.parameter'
- 'function', 'function.builtin', and 'function.method'

For a given highlight, styling will be determined based on the longest matching theme key.
For example, the highlight 'function.builtin.static' would match the key 'function.builtin' rather than 'function'.
*/
TextEditorStyle::CharStyle TSConfig::FindStyleByCaptureName(const std::string &capture_name) const {
    size_t pos = capture_name.size();
    do {
        if (auto it = StyleByHighlightName.find(capture_name.substr(0, pos)); it != StyleByHighlightName.end()) {
            return it->second;
        }
        pos = capture_name.rfind('.', pos - 1); // Move to the last '.' before the current `pos`.
    } while (pos != std::string::npos);

    return DefaultCharStyle;
}

static bool IsPressed(ImGuiKey key) {
    const auto key_index = ImGui::GetKeyIndex(key);
    const auto window_id = ImGui::GetCurrentWindowRead()->ID;
    ImGui::SetKeyOwner(key_index, window_id); // Prevent app from handling this key press.
    return ImGui::IsKeyPressed(key_index, window_id);
}

void TextEditor::HandleKeyboardInputs() {
    if (ImGui::IsWindowHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

    auto &io = ImGui::GetIO();
    io.WantCaptureKeyboard = io.WantTextInput = true;

    const bool
        is_osx = io.ConfigMacOSXBehaviors,
        alt = io.KeyAlt, ctrl = io.KeyCtrl, shift = io.KeyShift, super = io.KeySuper,
        is_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift,
        is_shift_shortcut = (is_osx ? (super && !ctrl) : (ctrl && !super)) && shift && !alt,
        is_wordmove_key = is_osx ? alt : ctrl,
        is_ctrl_only = ctrl && !alt && !shift && !super,
        is_shift_only = shift && !alt && !ctrl && !super;

    if (is_shortcut && IsPressed(ImGuiKey_Z) && CanUndo())
        Undo();
    else if (is_shift_shortcut && IsPressed(ImGuiKey_Z) && CanRedo())
        Redo();
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_UpArrow))
        Cursors.MoveLines(*this, -1, shift);
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_DownArrow))
        Cursors.MoveLines(*this, 1, shift);
    else if ((is_osx ? !ctrl : !alt) && !super && IsPressed(ImGuiKey_LeftArrow))
        Cursors.MoveChar(*this, false, shift, is_wordmove_key);
    else if ((is_osx ? !ctrl : !alt) && !super && IsPressed(ImGuiKey_RightArrow))
        Cursors.MoveChar(*this, true, shift, is_wordmove_key);
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_PageUp))
        Cursors.MoveLines(*this, -(ContentCoordDims.L - 2), shift);
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_PageDown))
        Cursors.MoveLines(*this, ContentCoordDims.L - 2, shift);
    else if (ctrl && !alt && !super && IsPressed(ImGuiKey_Home))
        Cursors.MoveTop(shift);
    else if (ctrl && !alt && !super && IsPressed(ImGuiKey_End))
        Cursors.MoveBottom(*this, shift);
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_Home))
        Cursors.MoveStart(shift);
    else if (!alt && !ctrl && !super && IsPressed(ImGuiKey_End))
        Cursors.MoveEnd(*this, shift);
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
    else if (((is_ctrl_only && IsPressed(ImGuiKey_Insert)) || (is_shortcut && IsPressed(ImGuiKey_C))) && CanCopy())
        Copy();
    else if (((is_shift_only && IsPressed(ImGuiKey_Insert)) || (is_shortcut && IsPressed(ImGuiKey_V))) && CanPaste())
        Paste();
    else if ((is_shortcut && IsPressed(ImGuiKey_X)) || (is_shift_only && IsPressed(ImGuiKey_Delete)))
        if (ReadOnly) {
            if (CanCopy()) Copy();
        } else {
            if (CanCut()) Cut();
        }
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

static float Distance(const ImVec2 &a, const ImVec2 &b) {
    const float x = a.x - b.x, y = a.y - b.y;
    return sqrt(x * x + y * y);
}

void TextEditor::HandleMouseInputs() {
    constexpr static ImGuiMouseButton MouseLeft = ImGuiMouseButton_Left, MouseMiddle = ImGuiMouseButton_Middle;

    const auto &io = ImGui::GetIO();
    const bool shift = io.KeyShift,
               ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl,
               alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    const bool panning = ImGui::IsMouseDown(MouseMiddle);
    if (panning && ImGui::IsMouseDragging(MouseMiddle)) {
        const ImVec2 scroll{ImGui::GetScrollX(), ImGui::GetScrollY()};
        const ImVec2 mouse_pos = ImGui::GetMouseDragDelta(MouseMiddle);
        const ImVec2 mouse_delta = mouse_pos - LastPanMousePos;
        ImGui::SetScrollY(scroll.y - mouse_delta.y);
        ImGui::SetScrollX(scroll.x - mouse_delta.x);
        LastPanMousePos = mouse_pos;
    }

    if (ImGui::IsMouseDown(MouseLeft) && ImGui::IsMouseDragging(MouseLeft)) {
        Cursors.GetLastAdded().SetEnd(ScreenPosToLC(ImGui::GetMousePos()));
    }

    const auto is_click = ImGui::IsMouseClicked(MouseLeft);
    if (shift && is_click) return Cursors.GetLastAdded().SetEnd(ScreenPosToLC(ImGui::GetMousePos()));
    if (shift || alt) return;

    if (ImGui::IsMouseClicked(MouseMiddle)) LastPanMousePos = ImGui::GetMouseDragDelta(MouseMiddle);

    const bool is_double_click = ImGui::IsMouseDoubleClicked(MouseLeft);
    const bool is_triple_click = is_click && !is_double_click && LastClickTime != -1.0f &&
        ImGui::GetTime() - LastClickTime < io.MouseDoubleClickTime && Distance(io.MousePos, LastClickPos) < 0.01f;
    if (is_triple_click) {
        if (ctrl) Cursors.Add();
        else Cursors.Reset();

        const auto cursor_lc = ScreenPosToLC(ImGui::GetMousePos());
        SetSelection({cursor_lc.L, 0}, CheckedNextLineBegin(cursor_lc.L), Cursors.back());

        LastClickTime = -1.0f;
    } else if (is_double_click) {
        if (ctrl) Cursors.Add();
        else Cursors.Reset();

        const auto cursor_lc = ScreenPosToLC(ImGui::GetMousePos());
        SetSelection(FindWordBoundary(cursor_lc, true), FindWordBoundary(cursor_lc, false), Cursors.back());

        LastClickTime = float(ImGui::GetTime());
        LastClickPos = io.MousePos;
    } else if (is_click) {
        if (ctrl) Cursors.Add();
        else Cursors.Reset();

        bool is_over_li;
        const auto cursor_lc = ScreenPosToLC(ImGui::GetMousePos(), &is_over_li);
        if (is_over_li) {
            SetSelection({cursor_lc.L, 0}, CheckedNextLineBegin(cursor_lc.L), Cursors.back());
        } else {
            Cursors.GetLastAdded().Set(cursor_lc);
        }

        LastClickTime = float(ImGui::GetTime());
        LastClickPos = io.MousePos;
    } else if (ImGui::IsMouseReleased(MouseLeft)) {
        Cursors.SortAndMerge();
    }
}

bool TextEditor::Render(bool is_parent_focused) {
    static constexpr float ImGuiScrollbarWidth = 14;

    auto edited_cursor = Cursors.GetEditedCursor();
    if (edited_cursor) {
        Cursors.ClearEdited();
        Cursors.SortAndMerge();
        MatchingBrackets = Cursors.size() == 1 ? FindMatchingBrackets(Cursors.front()) : std::nullopt;
    }
    const bool is_focused = ImGui::IsWindowFocused() || is_parent_focused;
    if (is_focused) HandleKeyboardInputs();
    if (ImGui::IsWindowHovered()) HandleMouseInputs();

    const float font_size = ImGui::GetFontSize();
    const float font_width = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    const float font_height = ImGui::GetTextLineHeightWithSpacing();
    CharAdvance = {font_width, font_height * LineSpacing};

    // Deduce `TextStart` by evaluating `Text` size plus two spaces as text width.
    TextStart = LeftMargin;
    static char li_buffer[16];
    if (ShowLineNumbers) {
        snprintf(li_buffer, 16, " %lu ", Text.size());
        TextStart += ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, li_buffer, nullptr, nullptr).x;
    }
    const ImVec2 scroll{ImGui::GetScrollX(), ImGui::GetScrollY()};
    const ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();
    const std::pair<bool, bool> scrollbars_visible = {CurrentSpaceDims.x > ContentDims.x, CurrentSpaceDims.y > ContentDims.y};
    const ImVec2 ContentDims{
        ImGui::GetWindowWidth() - (scrollbars_visible.first ? ImGuiScrollbarWidth : 0.0f),
        ImGui::GetWindowHeight() - (scrollbars_visible.second ? ImGuiScrollbarWidth : 0.0f)
    };
    const Coords first_visible_coords = {uint(scroll.y / CharAdvance.y), uint(std::max(scroll.x - TextStart, 0.0f) / CharAdvance.x)};
    const Coords last_visible_coords = {uint((ContentDims.y + scroll.y) / CharAdvance.y), uint((ContentDims.x + scroll.x - TextStart) / CharAdvance.x)};
    ContentCoordDims = last_visible_coords - first_visible_coords + Coords{1, 1};

    uint max_column = 0;
    auto dl = ImGui::GetWindowDrawList();
    const float space_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, " ").x;
    auto transition_it = CaptureIdTransitions.begin();
    for (uint li = first_visible_coords.L, byte_index = ToByteIndex({first_visible_coords.L, 0});
         li <= last_visible_coords.L && li < Text.size(); ++li) {
        const auto &line = Text[li];
        const uint line_max_column = GetLineMaxColumn(line, last_visible_coords.C);
        max_column = std::max(line_max_column, max_column);

        const ImVec2 line_start_screen_pos{cursor_screen_pos.x, cursor_screen_pos.y + li * CharAdvance.y};
        const float text_screen_pos_x = line_start_screen_pos.x + TextStart;
        const Coords line_start_coord{li, 0}, line_end_coord{li, line_max_column};
        // Draw current line selection
        for (const auto &c : Cursors) {
            const auto selection_start = ToCoords(c.Min()), selection_end = ToCoords(c.Max());
            if (selection_start <= line_end_coord && selection_end > line_start_coord) {
                const uint rect_start = selection_start > line_start_coord ? selection_start.C : 0;
                const uint rect_end = selection_end < line_end_coord ?
                    selection_end.C :
                    line_end_coord.C + (selection_end.L > li || (selection_end.L == li && selection_end > line_end_coord) ? 1 : 0);
                if (rect_start < rect_end) {
                    dl->AddRectFilled(
                        {text_screen_pos_x + rect_start * CharAdvance.x, line_start_screen_pos.y},
                        {text_screen_pos_x + rect_end * CharAdvance.x, line_start_screen_pos.y + CharAdvance.y},
                        GetColor(PaletteIndex::Selection)
                    );
                }
            }
        }

        // Draw line number (right aligned)
        if (ShowLineNumbers) {
            snprintf(li_buffer, 16, "%d  ", li + 1);
            const float line_num_width = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, li_buffer).x;
            dl->AddText({text_screen_pos_x - line_num_width, line_start_screen_pos.y}, GetColor(PaletteIndex::LineNumber), li_buffer);
        }

        // Render cursors
        if (is_focused) {
            for (const auto &c : filter(Cursors, [li](const auto &c) { return c.Line() == li; })) {
                const uint ci = c.CharIndex();
                const float cx = GetColumn(line, ci) * CharAdvance.x;
                float width = 1.0f;
                if (Overwrite && ci < line.size()) {
                    width = line[ci] == '\t' ? (1.f + std::floor((1.f + cx) / (NumTabSpaces * space_size))) * (NumTabSpaces * space_size) - cx : CharAdvance.x;
                }
                dl->AddRectFilled(
                    {text_screen_pos_x + cx, line_start_screen_pos.y},
                    {text_screen_pos_x + cx + width, line_start_screen_pos.y + CharAdvance.y},
                    GetColor(PaletteIndex::Cursor)
                );
            }
        }

        // Render colorized text
        const uint line_start_byte_index = byte_index;
        const uint start_ci = GetFirstVisibleCharIndex(line, first_visible_coords.C);
        byte_index += start_ci;
        transition_it.MoveForwardTo(byte_index);
        for (uint ci = start_ci, column = first_visible_coords.C; ci < line.size() && column <= last_visible_coords.C;) {
            const auto lc = LineChar{li, ci};
            const ImVec2 glyph_pos = line_start_screen_pos + ImVec2{TextStart + column * CharAdvance.x, 0};
            const char ch = line[lc.C];
            const uint seq_length = UTF8CharLength(ch);
            if (ch == '\t') {
                if (ShowWhitespaces) {
                    const ImVec2 p1{glyph_pos + ImVec2{CharAdvance.x * 0.3f, font_height * 0.5f}};
                    const ImVec2 p2{
                        glyph_pos.x + (ShortTabs ? (NumTabSpacesAtColumn(column) * CharAdvance.x - CharAdvance.x * 0.3f) : CharAdvance.x),
                        p1.y
                    };
                    const float gap = ImGui::GetFontSize() * (ShortTabs ? 0.16f : 0.2f);
                    const ImVec2 p3{p2.x - gap, p1.y - gap}, p4{p2.x - gap, p1.y + gap};
                    const ImU32 color = GetColor(PaletteIndex::ControlCharacter);
                    dl->AddLine(p1, p2, color);
                    dl->AddLine(p2, p3, color);
                    dl->AddLine(p2, p4, color);
                }
            } else if (ch == ' ') {
                if (ShowWhitespaces) {
                    dl->AddCircleFilled(
                        glyph_pos + ImVec2{space_size, ImGui::GetFontSize()} * 0.5f,
                        1.5f, GetColor(PaletteIndex::ControlCharacter), 4
                    );
                }
            } else {
                if (seq_length == 1 && MatchingBrackets && (MatchingBrackets->GetStart() == lc || MatchingBrackets->GetEnd() == lc)) {
                    const ImVec2 top_left{glyph_pos + ImVec2{0, font_height + 1.0f}};
                    dl->AddRectFilled(top_left, top_left + ImVec2{CharAdvance.x, 1.0f}, GetColor(PaletteIndex::Cursor));
                }
                // Find color for the current character.
                const string text = subrange(line.begin() + ci, line.begin() + ci + seq_length) | ranges::to<string>();
                dl->AddText(glyph_pos, StyleByCaptureId.at(*transition_it).Color, text.c_str());
            }
            if (ShowStyleTransitionPoints && !transition_it.IsEnd() && transition_it.ByteIndex == byte_index) {
                const auto &style = StyleByCaptureId.at(*transition_it);
                const float x = text_screen_pos_x + lc.C * CharAdvance.x;
                auto c = ImColor(style.Color);
                c.Value.w = 0.2f;
                dl->AddRectFilled({x, line_start_screen_pos.y}, ImVec2{x, line_start_screen_pos.y} + CharAdvance, c);
            }
            if (ShowChangedCaptureRanges) {
                for (const auto &range : ChangedCaptureRanges) {
                    if (byte_index >= range.Start && byte_index < range.End) {
                        const float x = text_screen_pos_x + lc.C * CharAdvance.x;
                        dl->AddRectFilled({x, line_start_screen_pos.y}, ImVec2{x, line_start_screen_pos.y} + CharAdvance, IM_COL32(255, 255, 255, 20));
                    }
                }
            }
            MoveCharIndexAndColumn(line, ci, column);
            byte_index += seq_length;
            transition_it.MoveForwardTo(byte_index);
        }
        byte_index = line_start_byte_index + line.size() + 1; // + 1 for the newline character.
    }

    CurrentSpaceDims = {
        std::max((max_column + std::min(ContentCoordDims.C - 1, max_column)) * CharAdvance.x, CurrentSpaceDims.x),
        (Text.size() + std::min(ContentCoordDims.L - 1, uint(Text.size()))) * CharAdvance.y
    };

    ImGui::SetCursorPos({0, 0});
    ImGui::Dummy(CurrentSpaceDims);
    if (edited_cursor) {
        // Move scroll to keep the edited cursor visible.
        // Goal: Keep the _entirity_ of the edited cursor(s) visible at all times.
        // So, vars like `end_in_view` mean, "is the end of the edited cursor in _fully_ in view?"
        // We assume at least the end has been edited, since it's the _interactive_ end.
        const auto end = edited_cursor->GetEndCoords(*this);
        const bool end_in_view = end.L > first_visible_coords.L && end.L < (std::min(last_visible_coords.L, 1u) - 1) &&
            end.C >= first_visible_coords.C && end.C < last_visible_coords.C;
        const bool target_start = edited_cursor->IsStartEdited() && end_in_view;
        const auto target = target_start ? edited_cursor->GetStartCoords(*this) : end;
        if (target.L <= first_visible_coords.L) {
            ImGui::SetScrollY(std::max((target.L - 0.5f) * CharAdvance.y, 0.f));
        } else if (target.L >= last_visible_coords.L) {
            ImGui::SetScrollY(std::max((target.L + 1.5f) * CharAdvance.y - ContentDims.y, 0.f));
        }
        if (target.C <= first_visible_coords.C) {
            ImGui::SetScrollX(std::clamp(TextStart + (target.C - 0.5f) * CharAdvance.x, 0.f, scroll.x));
        } else if (target.C >= last_visible_coords.C) {
            ImGui::SetScrollX(std::max(TextStart + (target.C + 1.5f) * CharAdvance.x - ContentDims.x, 0.f));
        }
    }
    if (ScrollToTop) {
        ScrollToTop = false;
        ImGui::SetScrollY(0);
    }

    return is_focused;
}

using namespace ImGui;

static void DrawEdits(const std::vector<TextEditor::InputEdit> &edits) {
    Text("Edits: %lu", edits.size());
    for (const auto &edit : edits) {
        BulletText("Start: %d, Old end: %d, New end: %d", edit.StartByte, edit.OldEndByte, edit.NewEndByte);
    }
}

void TextEditor::DebugPanel() {
    if (CollapsingHeader("Editor state")) {
        ImGui::Text("Cursor count: %lu", Cursors.size());
        for (const auto &c : Cursors) {
            const auto &start = c.GetStart(), &end = c.GetEnd();
            ImGui::Text("Start: {%d, %d}(%u), End: {%d, %d}(%u)", start.L, start.C, ToByteIndex(start), end.L, end.C, ToByteIndex(end));
        }
        if (CollapsingHeader("Line lengths")) {
            for (uint i = 0; i < Text.size(); i++) ImGui::Text("%u: %lu", i, Text[i].size());
        }
    }
    if (CollapsingHeader("History")) {
        ImGui::Text("Index: %u of %lu", HistoryIndex, History.size());
        for (size_t i = 0; i < History.size(); i++) {
            if (CollapsingHeader(std::to_string(i).c_str())) DrawEdits(History[i].Edits);
        }
    }
    if (CollapsingHeader("Tree-Sitter")) {
        ImGui::Text("S-expression:\n%s", GetSyntaxTreeSExp().c_str());
    }
}

const TextEditor::PaletteT TextEditor::DarkPalette = {{
    0xffe4dfdc, // Default
    0xff342c28, // Background
    0xffe0e0e0, // Cursor
    0x80a06020, // Selection
    0x800020ff, // Error
    0x15ffffff, // ControlCharacter
    0x40f08000, // Breakpoint
    0xff94837a, // Line number
    0x40000000, // Current line fill
    0x40808080, // Current line fill (inactive)
    0x40a0a0a0, // Current line edge
}};

const TextEditor::PaletteT TextEditor::MarianaPalette = {{
    0xffffffff, // Default
    0xff413830, // Background
    0xffe0e0e0, // Cursor
    0x80655a4e, // Selection
    0x80665fec, // Error
    0x30ffffff, // ControlCharacter
    0x40f08000, // Breakpoint
    0xb0ffffff, // Line number
    0x80655a4e, // Current line fill
    0x30655a4e, // Current line fill (inactive)
    0xb0655a4e, // Current line edge
}};

const TextEditor::PaletteT TextEditor::LightPalette = {{
    0xff404040, // Default
    0xffffffff, // Background
    0xff000000, // Cursor
    0x40600000, // Selection
    0xa00010ff, // Error
    0x90909090, // ControlCharacter
    0x80f08000, // Breakpoint
    0xff505000, // Line number
    0x40000000, // Current line fill
    0x40808080, // Current line fill (inactive)
    0x40000000, // Current line edge
}};

const TextEditor::PaletteT TextEditor::RetroBluePalette = {{
    0xff00ffff, // Default
    0xff800000, // Background
    0xff0080ff, // Cursor
    0x80ffff00, // Selection
    0xa00000ff, // Error
    0x80ff8000, // Breakpoint
    0xff808000, // Line number
    0x40000000, // Current line fill
    0x40808080, // Current line fill (inactive)
    0x40000000, // Current line edge
}};
