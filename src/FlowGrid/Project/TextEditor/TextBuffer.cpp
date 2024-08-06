#include "TextBuffer.h"

#include <array>
#include <ranges>
#include <set>
#include <vector>

#include "imgui_internal.h"
#include "immer/flex_vector_transient.hpp"
#include "immer/vector.hpp"

#include "Application/ApplicationPreferences.h"
#include "Core/Windows.h"
#include "Helper/File.h"
#include "Helper/String.h"
#include "Helper/Time.h"
#include "Project/FileDialog/FileDialog.h"
#include "UI/Fonts.h"

#include "LanguageID.h"
#include "SyntaxTree.h"
#include "TextBufferPaletteId.h"
#include "TextBufferSnapshot.h"

using Snapshot = TextBufferSnapshot;
using Cursor = LineCharRange;

using std::string, std::string_view;
using std::views::filter, std::views::transform, std::views::join,
    std::ranges::reverse_view, std::ranges::any_of, std::ranges::all_of, std::ranges::find_if, std::ranges::subrange, std::ranges::fold_left, std::ranges::to;

std::string join_with(auto &&v, string_view delimiter = "") {
    std::ostringstream os;
    for (auto it = std::ranges::begin(v); it != std::ranges::end(v); ++it) {
        if (it != v.begin()) os << delimiter;
        os << *it;
    }
    return os.str();
}
namespace fs = std::filesystem;

struct SyntaxTree;
struct SyntaxNodeInfo;

enum class PaletteIndex {
    TextDefault,
    Background,
    Cursor,
    Selection,
    Error,
    ControlCharacter,
    Breakpoint,
    LineNumber,
    CurrentLineFill,
    CurrentLineFillInactive,
    CurrentLineEdge,
    Max
};

const char *TSReadText(void *payload, u32 byte_index, TSPoint position, u32 *bytes_read);

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static u32 UTF8CharLength(char ch) {
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

static char ToLower(char ch, bool case_sensitive) { return (!case_sensitive && ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch; }

static bool Equals(const auto &c1, const auto &c2, std::size_t c2_offset = 0) {
    if (c2.size() + c2_offset < c1.size()) return false;

    const auto begin = c2.begin() + c2_offset;
    return std::ranges::equal(c1, subrange(begin, begin + c1.size()));
}

static float Distance(const ImVec2 &a, const ImVec2 &b) {
    const float x = a.x - b.x, y = a.y - b.y;
    return sqrt(x * x + y * y);
}

struct TextBufferImpl {
    TextBufferImpl(ID id, std::string_view text, LanguageID language_id)
        : Id(id), Syntax(std::make_unique<SyntaxTree>(TSInput{this, TSReadText, TSInputEncodingUTF8})) {
        SetLanguage(language_id);
        SetText(string(text));
        Commit({});
    }
    TextBufferImpl(ID id, const fs::path &file_path)
        : Id(id), Syntax(std::make_unique<SyntaxTree>(TSInput{this, TSReadText, TSInputEncodingUTF8})) {
        OpenFile(file_path);
        Commit({});
    }

    ~TextBufferImpl() = default;

    using PaletteT = std::array<u32, u32(PaletteIndex::Max)>;
    inline static const TextBufferPaletteId DefaultPaletteId{TextBufferPaletteId::Dark};

    inline static const PaletteT DarkPalette = {{
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

    inline static const PaletteT MarianaPalette = {{
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

    inline static const PaletteT LightPalette = {{
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

    inline static const PaletteT RetroBluePalette = {{
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

    // Represents a character coordinate from the user's point of view,
    // i. e. consider a uniform grid (assuming fixed-width font) on the screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..NumTabSpaces] u32 empty spaces, depending on how many space is necessary to reach the next tab stop.
    // For example, `Coords{1, 5}` represents the character 'B' in the line "\tABC", when NumTabSpaces = 4, since it is rendered as "    ABC".
    struct Coords {
        u32 L{0}, C{0}; // Line, Column

        auto operator<=>(const Coords &o) const {
            if (auto cmp = L <=> o.L; cmp != 0) return cmp;
            return C <=> o.C;
        }
        bool operator==(const Coords &) const = default;
        bool operator!=(const Coords &) const = default;

        Coords operator-(const Coords &o) const { return {L - o.L, C - o.C}; }
        Coords operator+(const Coords &o) const { return {L + o.L, C + o.C}; }
    };

    using Line = TextBufferLine;
    using Lines = TextBufferLines;
    using TransientLine = immer::flex_vector_transient<char>;
    using TransientLines = immer::flex_vector_transient<Line>;

    struct LinesIter {
        LinesIter(const Lines &lines, LineChar lc, LineChar begin, LineChar end)
            : Text(lines), LC(std::move(lc)), Begin(std::move(begin)), End(std::move(end)) {}
        LinesIter(const LinesIter &) = default; // Needed since we have an assignment operator.

        LinesIter &operator=(const LinesIter &o) {
            LC = o.LC;
            Begin = o.Begin;
            End = o.End;
            return *this;
        }

        operator char() const {
            const auto &line = Text[LC.L];
            return LC.C < line.size() ? line[LC.C] : '\0';
        }

        LineChar operator*() const { return LC; }

        LinesIter &operator++() {
            MoveRight();
            return *this;
        }
        LinesIter &operator--() {
            MoveLeft();
            return *this;
        }

        bool operator==(const LinesIter &o) const { return LC == o.LC; }
        bool operator!=(const LinesIter &o) const { return LC != o.LC; }

        bool IsBegin() const { return LC == Begin; }
        bool IsEnd() const { return LC == End; }
        void Reset() { LC = Begin; }

    private:
        const Lines &Text;
        LineChar LC, Begin, End;

        void MoveRight() {
            if (LC == End) return;

            if (LC.C == Text[LC.L].size()) {
                ++LC.L;
                LC.C = 0;
            } else {
                LC.C = std::min(LC.C + UTF8CharLength(Text[LC.L][LC.C]), u32(Text[LC.L].size()));
            }
        }

        void MoveLeft() {
            if (LC == Begin) return;

            if (LC.C == 0) {
                --LC.L;
                LC.C = Text[LC.L].size();
            } else {
                do { --LC.C; } while (LC.C > 0 && IsUTFSequence(Text[LC.L][LC.C]));
            }
        }
    };

    bool AnyCursorsRanged() const {
        return any_of(Cursors, [](const auto &c) { return c.IsRange(); });
    }
    bool AllCursorsRanged() const {
        return all_of(Cursors, [](const auto &c) { return c.IsRange(); });
    }
    bool AnyCursorsMultiline() const {
        return any_of(Cursors, [](const auto &c) { return c.IsMultiline(); });
    }

    void MarkCursorsEdited() {
        for (u32 i = 0; i < Cursors.size(); ++i) {
            StartEdited.insert(i);
            EndEdited.insert(i);
        }
    }

    const Cursor &LastAddedCursor() const { return Cursors[LastAddedIndex]; }

    void SortAndMerge() {
        ColumnsForCursorIndex.clear();
        if (Cursors.size() <= 1) return;

        // Sort cursors.
        const auto last_added_cursor_lc = LastAddedCursor().LC();
        std::ranges::sort(Cursors, [](const auto &a, const auto &b) { return a.Min() < b.Min(); });

        // Merge overlapping cursors.
        std::vector<Cursor> merged;
        Cursor current = Cursors.front();
        for (size_t c = 1; c < Cursors.size(); ++c) {
            const auto &next = Cursors[c];
            if (current.Max() >= next.Min()) {
                // Overlap. Extend the current cursor to to include the next.
                const auto start = std::min(current.Min(), next.Min());
                const auto end = std::max(current.Max(), next.Max());
                current.Start = start;
                current.End = end;
            } else {
                // No overlap. Finalize the current cursor and start a new merge.
                merged.push_back(current);
                current = next;
            }
        }

        merged.push_back(current);
        Cursors = std::move(merged);

        // Update last added cursor index to be valid after sort/merge.
        const auto it = find_if(Cursors, [&last_added_cursor_lc](const auto &c) { return c.LC() == last_added_cursor_lc; });
        LastAddedIndex = it != Cursors.end() ? std::distance(Cursors.begin(), it) : 0;
    }

    // Returns the range of all edited cursor starts/ends since cursor edits were last cleared.
    // Used for updating the scroll range.
    // todo need to update the approach here after switching to persistent undo.
    std::optional<Cursor> GetEditedCursor() const {
        if (StartEdited.empty() && EndEdited.empty()) return {};

        Cursor edited_range;
        for (u32 i = 0; i < Cursors.size(); ++i) {
            if (StartEdited.contains(i) || EndEdited.contains(i)) {
                edited_range = Cursors[i];
                break; // todo create a sensible cursor representing the combined range when multiple cursors are edited.
            }
        }
        return edited_range;
    }

    std::unordered_map<u32, std::pair<u32, u32>> ColumnsForCursorIndex{};
    // Cleared every frame. Used to keep recently edited cursors visible.
    std::unordered_set<u32> StartEdited{}, EndEdited{};

    bool Empty() const { return Text.empty() || (Text.size() == 1 && Text[0].empty()); }

    u32 LineCount() const { return Text.size(); }
    const Line &GetLine(u32 li) const { return Text[li]; }
    LineChar GetCursorPosition() const { return Cursors.back().LC(); }
    LineChar CheckedNextLineBegin(u32 li) const { return li < Text.size() - 1 ? LineChar{li + 1, 0} : EndLC(); }

    string GetText(LineChar start, LineChar end) const {
        if (end <= start) return "";

        const u32 start_li = start.L, end_li = std::min(u32(Text.size()) - 1, end.L);
        const u32 start_ci = start.C, end_ci = end.C;
        string result;
        for (u32 ci = start_ci, li = start_li; li < end_li || ci < end_ci;) {
            if (const auto &line = Text[li]; ci < line.size()) {
                result += line[ci++];
            } else {
                ++li;
                ci = 0;
                result += '\n';
            }
        }

        return result;
    }

    std::string GetText() const { return GetText(BeginLC(), EndLC()); }

    string GetSyntaxTreeSExp() const { return Syntax->GetSExp(); }

    string_view GetLanguageName() const { return Languages.Get(LanguageId).Name; }

    u32 GetColor(PaletteIndex index) const { return GetPalette()[u32(index)]; }

    const PaletteT &GetPalette() const {
        switch (PaletteId) {
            case TextBufferPaletteId::Dark: return DarkPalette;
            case TextBufferPaletteId::Light: return LightPalette;
            case TextBufferPaletteId::Mariana: return MarianaPalette;
            case TextBufferPaletteId::RetroBlue: return RetroBluePalette;
        }
    }

    void SetText(const string &text) {
        const u32 old_end_byte = EndByteIndex();
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
        History = {};
        HistoryIndex = -1;

        Edits = Edits.push_back({0, old_end_byte, EndByteIndex()});
    }

    void OpenFile(const fs::path &file_path) {
        SetFilePath(file_path);
        SetText(FileIO::read(file_path));
    }

    void SetFilePath(const fs::path &file_path) {
        const string extension = file_path.extension();
        SetLanguage(!extension.empty() && Languages.ByFileExtension.contains(extension) ? Languages.ByFileExtension.at(extension) : LanguageID::None);
    }

    void SetPalette(TextBufferPaletteId palette_id) { PaletteId = palette_id; }

    void SetLanguage(LanguageID language_id) {
        if (LanguageId == language_id) return;

        LanguageId = language_id;
        Syntax->SetLanguage(language_id);
        Syntax->ApplyEdits(Edits);
        Edits = {};
    }

    void SetNumTabSpaces(u32 tab_size) { NumTabSpaces = std::clamp(tab_size, 1u, 8u); }
    void SetLineSpacing(float line_spacing) { LineSpacing = std::clamp(line_spacing, 1.f, 2.f); }

    // If `add == true`, a new cursor is added and set.
    // Otherwise, the cursors are _cleared_ and a new cursor is added and set.
    void SetCursor(Cursor c, bool add = false) {
        if (!add) {
            Cursors.clear();
            LastAddedIndex = 0;
        }
        Cursors.emplace_back(std::move(c));
        LastAddedIndex = Cursors.size() - 1;
        SortAndMerge();
    }
    void EditCursor(Cursor &c, LineChar end, bool set_both = true) const {
        if (set_both) c.Start = end;
        c.End = end;
    }
    void MoveCursorsBottom(bool select = false) {
        for (auto &c : Cursors) EditCursor(c, LineMaxLC(LineCount() - 1), !select);
        MarkCursorsEdited();
        SortAndMerge();
    }
    void MoveCursorsTop(bool select = false) {
        for (auto &c : Cursors) EditCursor(c, {0, 0}, !select);
        MarkCursorsEdited();
        SortAndMerge();
    }
    void MoveCursorsStartLine(bool select = false) {
        for (auto &c : Cursors) EditCursor(c, {c.Line(), 0}, !select);
        MarkCursorsEdited();
        SortAndMerge();
    }
    void MoveCursorsEndLine(bool select = false) {
        for (auto &c : Cursors) EditCursor(c, LineMaxLC(c.Line()), !select);
        MarkCursorsEdited();
        SortAndMerge();
    }

    std::pair<u32, u32> GetColumns(const Cursor &c, u32 i) {
        if (!ColumnsForCursorIndex.contains(i)) ColumnsForCursorIndex[i] = {GetColumn(c.Start), GetColumn(c.End)};
        return ColumnsForCursorIndex.at(i);
    }

    void MoveCursorsLines(int amount = 1, bool select = false, bool move_start = false, bool move_end = true) {
        if (!move_start && !move_end) return;

        for (u32 i = 0; i < Cursors.size(); ++i) {
            auto &c = Cursors[i];
            // Track the cursor's column to return back to it after moving to a line long enough.
            // (This is the only place we worry about this.)
            const auto [new_start_column, new_end_column] = GetColumns(c, i);
            const u32 new_end_li = std::clamp(int(c.End.L) + amount, 0, int(LineCount() - 1));
            const LineChar new_end{
                new_end_li,
                std::min(GetCharIndex({new_end_li, new_end_column}), GetLineMaxCharIndex(new_end_li)),
            };
            if (!select || !move_start) {
                EditCursor(c, new_end, !select);
                continue;
            }

            const u32 new_start_li = std::clamp(int(c.Start.L) + amount, 0, int(LineCount() - 1));
            const LineChar new_start{
                new_start_li,
                std::min(GetCharIndex({new_start_li, new_start_column}), GetLineMaxCharIndex(new_start_li)),
            };
            c.Start = new_start;
            c.End = new_end;
        }
    }

    void PageCursorsLines(bool up, bool select = false) {
        MoveCursorsLines((ContentCoordDims.L - 2) * (up ? -1 : 1), select);
    }

    void MoveCursorsChar(bool right = true, bool select = false, bool is_word_mode = false) {
        const bool any_selections = AnyCursorsRanged();
        for (auto &c : Cursors) {
            if (any_selections && !select && !is_word_mode) {
                EditCursor(c, right ? c.Max() : c.Min(), true);
            } else if (auto lci = Iter(c.LC()); (!right && !lci.IsBegin()) || (right && !lci.IsEnd())) {
                if (right) ++lci;
                else --lci;
                EditCursor(c, is_word_mode ? FindWordBoundary(*lci, !right) : *lci, !select);
            }
        }
        SortAndMerge();
    }

    void SelectAll() { SetCursor({{0, 0}, EndLC()}, false); }

    void ToggleOverwrite() { Overwrite ^= true; } // todo use Bool prop

    bool CanUndo() const { return !ReadOnly && HistoryIndex > 0; }
    bool CanRedo() const { return !ReadOnly && History.size() > 1 && HistoryIndex < u32(History.size() - 1); }
    bool CanCopy() const { return AnyCursorsRanged(); }
    bool CanCut() const { return !ReadOnly && CanCopy(); }
    bool CanPaste() const { return !ReadOnly && ImGui::GetClipboardText() != nullptr; }

    void Undo() {
        if (!CanUndo()) return;

        const auto &current = History[HistoryIndex], &restore = History[--HistoryIndex];
        Text = restore.Text;
        Cursors = current.BeforeCursors.Cursors;
        LastAddedIndex = current.BeforeCursors.LastAddedIndex;
        MarkCursorsEdited();
        Syntax->ApplyEdits(reverse_view(current.Edits) | transform([](const auto &edit) { return edit.Invert(); }));
        Edits = {};
    }
    void Redo() {
        if (!CanRedo()) return;

        const auto restore = History[++HistoryIndex];
        Text = restore.Text;
        Cursors = restore.Cursors.Cursors;
        LastAddedIndex = restore.Cursors.LastAddedIndex;
        MarkCursorsEdited();
        Edits = restore.Edits;
        Syntax->ApplyEdits(Edits);
        Edits = {};
    }

    void Copy() {
        const string str = AnyCursorsRanged() ?
            join_with(Cursors | filter([](const auto &c) { return c.IsRange(); }) | transform([this](const auto &c) { return GetSelectedText(c); }), "\n") :
            Text[GetCursorPosition().L] | to<string>();
        ImGui::SetClipboardText(str.c_str());
    }

    void Cut() {
        if (!AnyCursorsRanged()) return;

        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        Copy();
        for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
        Commit(std::move(before_cursors));
    }

    void Paste() {
        // todo store clipboard text manually in a `Lines`?
        const char *clip_text = ImGui::GetClipboardText();
        if (*clip_text == '\0') return;

        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};

        TransientLines insert_text_lines_trans{};
        const char *ptr = clip_text;
        while (*ptr != '\0') {
            u32 str_length = 0;
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
        Commit(std::move(before_cursors));
    }

    void EnterChar(ImWchar ch) {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        for (auto &c : reverse_view(Cursors)) DeleteSelection(c);

        // Order is important here for typing '\n' in the same line with multiple cursors.
        for (auto &c : reverse_view(Cursors)) {
            TransientLine insert_line_trans{};
            if (ch == '\n') {
                if (AutoIndent && c.CharIndex() != 0) {
                    // Match the indentation of the current or next line, whichever has more indentation.
                    // todo use tree-sitter fold queries
                    const u32 li = c.Line();
                    const u32 indent_li = li < Text.size() - 1 && NumStartingSpaceColumns(li + 1) > NumStartingSpaceColumns(li) ? li + 1 : li;
                    const auto &indent_line = Text[indent_li];
                    for (u32 i = 0; i < insert_line_trans.size() && isblank(indent_line[i]); ++i) insert_line_trans.push_back(indent_line[i]);
                }
            } else {
                char buf[5];
                ImTextCharToUtf8(buf, ch);
                for (u32 i = 0; i < 5 && buf[i] != '\0'; ++i) insert_line_trans.push_back(buf[i]);
            }
            auto insert_line = insert_line_trans.persistent();
            InsertTextAtCursor(ch == '\n' ? Lines{{}, insert_line} : Lines{insert_line}, c);
        }

        Commit(std::move(before_cursors));
    }

    void Backspace(bool is_word_mode = false) {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        if (!AnyCursorsRanged()) {
            MoveCursorsChar(false, true, is_word_mode);
            // Can't do backspace if any cursor is at {0,0}.
            if (!AllCursorsRanged()) {
                if (AnyCursorsRanged()) MoveCursorsChar(true); // Restore cursors.
                return;
            }
            SortAndMerge();
        }
        for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
        Commit(std::move(before_cursors));
    }

    void Delete(bool is_word_mode = false) {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        if (!AnyCursorsRanged()) {
            MoveCursorsChar(true, true, is_word_mode);
            // Can't do delete if any cursor is at the end of the last line.
            if (!AllCursorsRanged()) {
                if (AnyCursorsRanged()) MoveCursorsChar(false); // Restore cursors.
                return;
            }
            SortAndMerge();
        }
        for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
        Commit(std::move(before_cursors));
    }

    void MoveCurrentLines(bool up) {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        std::set<u32> affected_lines;
        u32 min_li = std::numeric_limits<u32>::max(), max_li = std::numeric_limits<u32>::min();
        for (const auto &c : Cursors) {
            for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
                // Check if selection ends at line start.
                if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

                affected_lines.insert(li);
                min_li = std::min(li, min_li);
                max_li = std::max(li, max_li);
            }
        }
        if ((up && min_li == 0) || (!up && max_li == Text.size() - 1)) return; // Can't move up/down anymore.

        if (up) {
            for (const u32 li : affected_lines) SwapLines(li - 1, li);
        } else {
            for (auto it = affected_lines.rbegin(); it != affected_lines.rend(); ++it) SwapLines(*it, *it + 1);
        }
        MoveCursorsLines(up ? -1 : 1, true, true, true);

        Commit(std::move(before_cursors));
    }

    void ToggleLineComment() {
        const string &comment = Languages.Get(LanguageId).SingleLineComment;
        if (comment.empty()) return;

        static const auto FindFirstNonSpace = [](const Line &line) { return std::distance(line.begin(), std::ranges::find_if_not(line, isblank)); };

        std::unordered_set<u32> affected_lines;
        for (const auto &c : Cursors) {
            for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
                if (!(c.IsRange() && c.Max() == LineChar{li, 0}) && !Text[li].empty()) affected_lines.insert(li);
            }
        }

        const bool should_add_comment = any_of(affected_lines, [&](u32 li) {
            return !Equals(comment, Text[li], FindFirstNonSpace(Text[li]));
        });

        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        for (u32 li : affected_lines) {
            if (should_add_comment) {
                InsertText({Line{comment.begin(), comment.end()} + Line{' '}}, {li, 0});
            } else {
                const auto &line = Text[li];
                const u32 ci = FindFirstNonSpace(line);
                u32 comment_ci = ci + comment.length();
                if (comment_ci < line.size() && line[comment_ci] == ' ') ++comment_ci;

                DeleteRange({li, ci}, {li, comment_ci});
            }
        }
        Commit(std::move(before_cursors));
    }

    void DeleteCurrentLines() {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        for (auto &c : reverse_view(Cursors)) DeleteSelection(c);
        MoveCursorsStartLine();
        SortAndMerge();

        for (const auto &c : reverse_view(Cursors)) {
            const u32 li = c.Line();
            DeleteRange(
                li == Text.size() - 1 && li > 0 ? LineMaxLC(li - 1) : LineChar{li, 0},
                CheckedNextLineBegin(li)
            );
        }
        Commit(std::move(before_cursors));
    }

    void ChangeCurrentLinesIndentation(bool increase) {
        CursorsSnapshot before_cursors{Cursors, LastAddedIndex};
        for (const auto &c : reverse_view(Cursors)) {
            for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
                // Check if selection ends at line start.
                if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

                const auto &line = Text[li];
                if (increase) {
                    if (!line.empty()) InsertText({{'\t'}}, {li, 0});
                } else {
                    int ci = int(GetCharIndex(line, NumTabSpaces)) - 1;
                    while (ci > -1 && isblank(line[ci])) --ci;
                    if (const bool only_space_chars_found = ci == -1; only_space_chars_found) {
                        DeleteRange({li, 0}, {li, GetCharIndex(line, NumTabSpaces)});
                    }
                }
            }
        }
        Commit(std::move(before_cursors));
    }

    void SelectNextOccurrence(bool case_sensitive = true) {
        const auto &c = LastAddedCursor();
        if (const auto match_range = FindNextOccurrence(GetSelectedText(c), c.Max(), case_sensitive)) {
            SetCursor({match_range->Start, match_range->End}, true);
        }
    }

    std::optional<TextBuffer::ActionType> Render(bool is_focused);
    void DebugPanel();

    bool ReadOnly{false};
    bool Overwrite{false};
    bool AutoIndent{true};
    bool ShowWhitespaces{true};
    bool ShowLineNumbers{true};
    bool ShowStyleTransitionPoints{false}, ShowChangedCaptureRanges{false};
    bool ShortTabs{true};
    float LineSpacing{1};

private:
    // Commit a snapshot to the undo history and edit the tree (see `EditTree`).
    void Commit(CursorsSnapshot &&before_cursors) {
        if (Edits.empty()) return;

        History = History.take(++HistoryIndex);
        Snapshot snapshot{Text, {Cursors, LastAddedIndex}, std::move(before_cursors), Edits};
        History = History.push_back(std::move(snapshot));
        Syntax->ApplyEdits(Edits);
        Edits = {};
    }

    std::string GetSelectedText(const Cursor &c) const { return GetText(c.Min(), c.Max()); }

    static LineChar BeginLC() { return {0, 0}; }

    LineChar EndLC() const { return LineMaxLC(Text.size() - 1); }
    u32 EndByteIndex() const { return ToByteIndex(EndLC()); }

    LinesIter Iter(LineChar lc, LineChar begin, LineChar end) const { return {Text, std::move(lc), std::move(begin), std::move(end)}; }
    LinesIter Iter(LineChar lc = BeginLC()) const { return Iter(std::move(lc), BeginLC(), EndLC()); }

    LineChar LineMaxLC(u32 li) const { return {li, GetLineMaxCharIndex(li)}; }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetColumn(Text[lc.L], lc.C)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(std::move(coords))}; }
    u32 ToByteIndex(LineChar lc) const {
        if (lc.L >= Text.size()) return EndByteIndex();
        return fold_left(subrange(Text.begin(), Text.begin() + lc.L), 0u, [](u32 sum, const auto &line) { return sum + line.size() + 1; }) + lc.C;
    }

    void MoveCharIndexAndColumn(const Line &line, u32 &ci, u32 &column) const {
        const char ch = line[ci];
        ci += UTF8CharLength(ch);
        column = ch == '\t' ? NextTabstop(column) : column + 1;
    }

    Coords ScreenPosToCoords(const ImVec2 &screen_pos, ImVec2 char_advance, float text_start_x, bool *is_over_li = nullptr) const {
        static constexpr float PosToCoordsColumnOffset = 0.33;

        const auto local = screen_pos + ImVec2{3, 0} - ImGui::GetCursorScreenPos();
        if (is_over_li != nullptr) *is_over_li = local.x < text_start_x;

        Coords coords{
            std::min(u32(std::max(0.f, floor(local.y / char_advance.y))), u32(Text.size()) - 1),
            u32(std::max(0.f, floor((local.x - text_start_x + PosToCoordsColumnOffset * char_advance.x) / char_advance.x)))
        };
        // Check if the coord is in the middle of a tab character.
        const auto &line = Text[std::min(coords.L, u32(Text.size()) - 1)];
        const u32 ci = GetCharIndex(line, coords.C);
        if (ci < line.size() && line[ci] == '\t') coords.C = GetColumn(line, ci);

        return {coords.L, GetLineMaxColumn(line, coords.C)};
    }

    u32 GetCharIndex(const Line &line, u32 column) const {
        u32 ci = 0;
        for (u32 column_i = 0; ci < line.size() && column_i < column;) MoveCharIndexAndColumn(line, ci, column_i);
        return ci;
    }
    u32 GetCharIndex(Coords coords) const { return GetCharIndex(Text[coords.L], coords.C); }
    u32 GetLineMaxCharIndex(u32 li) const { return Text[li].size(); }

    u32 GetColumn(const Line &line, u32 ci) const {
        u32 column = 0;
        for (u32 ci_i = 0; ci_i < ci && ci_i < line.size();) MoveCharIndexAndColumn(line, ci_i, column);
        return column;
    }
    u32 GetColumn(LineChar lc) const { return GetColumn(Text[lc.L], lc.C); }

    u32 GetFirstVisibleCharIndex(const Line &line, u32 first_visible_column) const {
        u32 ci = 0, column = 0;
        while (column < first_visible_column && ci < line.size()) MoveCharIndexAndColumn(line, ci, column);
        return column > first_visible_column && ci > 0 ? ci - 1 : ci;
    }

    u32 GetLineMaxColumn(const Line &line) const {
        u32 column = 0;
        for (u32 ci = 0; ci < line.size();) MoveCharIndexAndColumn(line, ci, column);
        return column;
    }
    u32 GetLineMaxColumn(const Line &line, u32 limit) const {
        u32 column = 0;
        for (u32 ci = 0; ci < line.size() && column < limit;) MoveCharIndexAndColumn(line, ci, column);
        return column;
    }

    Cursor Clamped(LineChar start, LineChar end) const {
        const auto begin_lc = BeginLC(), end_lc = EndLC();
        return {std::clamp(start, begin_lc, end_lc), std::clamp(end, begin_lc, end_lc)};
    }

    LineChar FindWordBoundary(LineChar from, bool is_start = false) const {
        if (from.L >= Text.size()) return from;

        const auto &line = Text[from.L];
        u32 ci = from.C;
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

    // Returns a cursor containing the start/end positions of the next occurrence of `text` at or after `start`, or `std::nullopt` if not found.
    std::optional<Cursor> FindNextOccurrence(string_view text, LineChar start, bool case_sensitive) const {
        if (text.empty()) return {};

        auto find_lci = Iter(start);
        do {
            auto match_lci = find_lci;
            for (u32 i = 0; i < text.size(); ++i) {
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

    std::optional<Cursor> FindMatchingBrackets(const Cursor &c) const {
        static const std::unordered_map<char, char>
            OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
            CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

        const u32 li = c.Line();
        const auto &line = Text[li];
        if (c.IsRange() || line.empty()) return {};

        u32 ci = c.CharIndex();
        // Considered on bracket if cursor is to the left or right of it.
        if (ci > 0 && (CloseToOpenChar.contains(line[ci - 1]) || OpenToCloseChar.contains(line[ci - 1]))) --ci;

        const char ch = line[ci];
        const bool is_close_char = CloseToOpenChar.contains(ch), is_open_char = OpenToCloseChar.contains(ch);
        if (!is_close_char && !is_open_char) return {};

        const LineChar lc{li, ci};
        const char other_ch = is_close_char ? CloseToOpenChar.at(ch) : OpenToCloseChar.at(ch);
        u32 match_count = 0;
        for (auto lci = Iter(lc); is_close_char ? !lci.IsBegin() : !lci.IsEnd(); is_close_char ? --lci : ++lci) {
            const char ch_inner = lci;
            if (ch_inner == ch) ++match_count;
            else if (ch_inner == other_ch && (match_count == 0 || --match_count == 0)) return Cursor{lc, *lci};
        }

        return {};
    }

    u32 NumStartingSpaceColumns(u32 li) const {
        const auto &line = Text[li];
        u32 column = 0;
        for (u32 ci = 0; ci < line.size() && isblank(line[ci]);) MoveCharIndexAndColumn(line, ci, column);
        return column;
    }

    void SwapLines(u32 li1, u32 li2) {
        if (li1 == li2 || li1 >= Text.size() || li2 >= Text.size()) return;

        InsertText({Text[li2], {}}, {li1, 0}, false);
        if (li2 + 1 < Text.size() - 1) DeleteRange({li2 + 1, 0}, {li2 + 2, 0}, false);
        // If the second line is the last line, we also need to delete the newline we just inserted.
        else DeleteRange({li2, u32(Text[li2].size())}, EndLC(), false);
    }

    // Returns insertion end.
    LineChar InsertText(Lines text, LineChar at, bool update_cursors = true) {
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

        const u32 num_new_lines = text.size() - 1;
        if (update_cursors) {
            auto cursors_below = Cursors | filter([&](const auto &c) { return c.Line() > at.L; });
            for (auto &c : cursors_below) EditCursor(c, {c.Line() + num_new_lines, c.CharIndex()});
        }

        const u32 start_byte = ToByteIndex(at);
        const u32 text_byte_length = std::accumulate(text.begin(), text.end(), 0, [](u32 sum, const auto &line) { return sum + line.size(); }) + text.size() - 1;
        Edits = Edits.push_back({start_byte, start_byte, start_byte + text_byte_length});

        return LineChar{at.L + num_new_lines, text.size() == 1 ? u32(at.C + text.front().size()) : u32(text.back().size())};
    }

    void InsertTextAtCursor(Lines text, Cursor &c) {
        if (!text.empty()) EditCursor(c, InsertText(text, c.Min()));
    }

    void DeleteRange(LineChar start, LineChar end, bool update_cursors = true, const Cursor *exclude_cursor = nullptr) {
        if (end <= start) return;

        auto start_line = Text[start.L], end_line = Text[end.L];
        const u32 start_byte = ToByteIndex(start), old_end_byte = ToByteIndex(end);
        if (start.L == end.L) {
            Text = Text.set(start.L, start_line.erase(start.C, end.C));

            if (update_cursors) {
                auto cursors_to_right = Cursors | filter([&start](const auto &c) { return !c.IsRange() && c.IsRightOf(start); });
                for (auto &c : cursors_to_right) EditCursor(c, {c.Line(), u32(c.CharIndex() - (end.C - start.C))});
            }
        } else {
            end_line = end_line.drop(end.C);
            Text = Text.set(end.L, end_line);
            Text = Text.set(start.L, start_line.take(start.C) + end_line);
            Text = Text.erase(start.L + 1, end.L + 1);

            if (update_cursors) {
                auto cursors_below = Cursors | filter([&](const auto &c) { return (!exclude_cursor || c != *exclude_cursor) && c.Line() >= end.L; });
                for (auto &c : cursors_below) EditCursor(c, {c.Line() - (end.L - start.L), c.CharIndex()});
            }
        }

        Edits = Edits.push_back({start_byte, old_end_byte, start_byte});
    }

    void DeleteSelection(Cursor &c) {
        if (!c.IsRange()) return;

        // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
        DeleteRange(c.Min(), c.Max(), true, &c);
        EditCursor(c, c.Min());
    }

    std::optional<TextBuffer::ActionType> HandleMouseInputs(ImVec2 char_advance, float text_start_x);

    u32 NumTabSpacesAtColumn(u32 column) const { return NumTabSpaces - (column % NumTabSpaces); }
    u32 NextTabstop(u32 column) const { return ((column / NumTabSpaces) + 1) * NumTabSpaces; }

    void CreateHoveredNode(u32 byte_index) {
        DestroyHoveredNode();
        HoveredNode = std::make_unique<SyntaxNodeAncestry>(Syntax->GetNodeAncestryAtByte(byte_index));
        for (const auto &node : HoveredNode->Ancestry) {
            std::string name = !node.FieldName.empty() ? std::format("{}: {}", node.FieldName, node.Type) : node.Type;
            HelpInfo::ById.emplace(node.Id, HelpInfo{.Name = std::move(name), .Help = ""});
        }
    }

    void DestroyHoveredNode() {
        if (HoveredNode) {
            for (const auto &node : HoveredNode->Ancestry) HelpInfo::ById.erase(node.Id);
            HoveredNode.reset();
        }
    }

    ID Id;
    Lines Text{Line{}};
    std::vector<Cursor> Cursors{{}};
    u32 LastAddedIndex{0};

    immer::vector<TextInputEdit> Edits{};

    TextBufferPaletteId PaletteId{DefaultPaletteId};
    LanguageID LanguageId{LanguageID::None};

    u32 NumTabSpaces{4};

    ImVec2 ContentDims{0, 0}; // Pixel width/height of current content area.
    Coords ContentCoordDims{0, 0}; // Coords width/height of current content area.
    ImVec2 CurrentSpaceDims{20, 20}; // Pixel width/height given to `ImGui::Dummy`.
    ImVec2 LastClickPos{-1, -1};
    float LastClickTime{-1}; // ImGui time.
    std::unique_ptr<SyntaxNodeAncestry> HoveredNode{};
    std::unique_ptr<SyntaxTree> Syntax;

    // The first history record is the initial state (after construction), and it's never removed from the history.
    immer::vector<Snapshot> History;
    u32 HistoryIndex{0};
};

const char *TSReadText(void *payload, u32 byte_index, TSPoint position, u32 *bytes_read) {
    (void)byte_index; // Unused.
    static const char newline = '\n';

    const auto *buffer = static_cast<TextBufferImpl *>(payload);
    if (position.row >= buffer->LineCount()) {
        *bytes_read = 0;
        return nullptr;
    }
    const auto &line = buffer->GetLine(position.row);
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

TextBuffer::TextBuffer(ArgsT &&args, const ::FileDialog &file_dialog, const fs::path &file_path)
    : ActionableComponent(std::move(args)), FileDialog(file_dialog), _LastOpenedFilePath(file_path),
      Impl(std::make_unique<TextBufferImpl>(Id, file_path)) {}

TextBuffer::~TextBuffer() {}

bool TextBuffer::CanApply(const ActionType &action) const {
    using namespace Action::TextBuffer;

    return std::visit(
        Match{
            [](const ShowOpenDialog &) { return true; },
            [](const ShowSaveDialog &) { return true; },
            [](const Open &) { return true; },
            [](const Save &) { return true; },

            [this](const Undo &) { return Impl->CanUndo(); },
            [this](const Redo &) { return Impl->CanRedo(); },

            [](const SetCursor &) { return true; },
            [](const SetCursorRange &) { return true; },
            [](const MoveCursorsLines &) { return true; },
            [](const PageCursorsLines &) { return true; },
            [](const MoveCursorsChar &) { return true; },
            [](const MoveCursorsTop &) { return true; },
            [](const MoveCursorsBottom &) { return true; },
            [](const MoveCursorsStartLine &) { return true; },
            [](const MoveCursorsEndLine &) { return true; },
            [](const SelectAll &) { return true; },
            [](const SelectNextOccurrence &) { return true; },

            [](const Set &) { return true; },
            [](const ToggleOverwrite &) { return true; },

            [this](const Copy &) { return Impl->CanCopy(); },
            [this](const Cut &) { return Impl->CanCut(); },
            [this](const Paste &) { return Impl->CanPaste(); },
            [this](const Delete &) { return !Impl->ReadOnly; },
            [this](const Backspace &) { return !Impl->ReadOnly; },
            [this](const DeleteCurrentLines &) { return !Impl->ReadOnly; },
            [this](const ChangeCurrentLinesIndentation &) { return !Impl->ReadOnly; },
            [this](const MoveCurrentLines &) { return !Impl->ReadOnly; },
            [this](const ToggleLineComment &) { return !Impl->ReadOnly; },
            [this](const EnterChar &) { return !Impl->ReadOnly; },
        },
        action
    );
}

void TextBuffer::Apply(const ActionType &action) const {
    using namespace Action::TextBuffer;

    std::visit(
        Match{
            [this](const ShowOpenDialog &) {
                FileDialog.Set({
                    .owner_id = Id,
                    .title = "Open file",
                    .filters = ".*", // No filter for opens. Go nuts :)
                    .save_mode = false,
                    .max_num_selections = 1, // todo open multiple files
                });
            },
            [this](const ShowSaveDialog &) {
                const string current_file_ext = fs::path(LastOpenedFilePath).extension();
                FileDialog.Set({
                    .owner_id = Id,
                    .title = std::format("Save {} file", Impl->GetLanguageName()),
                    .filters = current_file_ext,
                    .default_file_name = std::format("my_{}_program{}", Impl->GetLanguageName() | transform(::tolower) | to<string>(), current_file_ext),
                    .save_mode = true,
                });
            },
            [this](const Open &a) {
                LastOpenedFilePath.Set(a.file_path);
                Impl->OpenFile(a.file_path);
            },
            [this](const Save &a) { FileIO::write(a.file_path, Impl->GetText()); },

            [this](const Undo &) { Impl->Undo(); },
            [this](const Redo &) { Impl->Redo(); },

            [this](const SetCursor &a) { Impl->SetCursor(a.lc, a.add); },
            [this](const SetCursorRange &a) { Impl->SetCursor(a.lcr, a.add); },
            [this](const MoveCursorsLines &a) { Impl->MoveCursorsLines(a.amount, a.select); },
            [this](const PageCursorsLines &a) { Impl->PageCursorsLines(a.up, a.select); },
            [this](const MoveCursorsChar &a) { Impl->MoveCursorsChar(a.right, a.select, a.word); },
            [this](const MoveCursorsTop &a) { Impl->MoveCursorsTop(a.select); },
            [this](const MoveCursorsBottom &a) { Impl->MoveCursorsBottom(a.select); },
            [this](const MoveCursorsStartLine &a) { Impl->MoveCursorsStartLine(a.select); },
            [this](const MoveCursorsEndLine &a) { Impl->MoveCursorsEndLine(a.select); },
            [this](const SelectAll &) { Impl->SelectAll(); },
            [this](const SelectNextOccurrence &) { Impl->SelectNextOccurrence(); },

            [this](const Set &a) { Impl->SetText(a.value); },
            [this](const ToggleOverwrite &) { Impl->ToggleOverwrite(); },

            [this](const Copy &) { Impl->Copy(); },
            [this](const Cut &) { Impl->Cut(); },
            [this](const Paste &) { Impl->Paste(); },
            [this](const Delete &a) { Impl->Delete(a.word); },
            [this](const Backspace &a) { Impl->Backspace(a.word); },
            [this](const DeleteCurrentLines &) { Impl->DeleteCurrentLines(); },
            [this](const ChangeCurrentLinesIndentation &a) { Impl->ChangeCurrentLinesIndentation(a.increase); },
            [this](const MoveCurrentLines &a) { Impl->MoveCurrentLines(a.up); },
            [this](const ToggleLineComment &) { Impl->ToggleLineComment(); },
            [this](const EnterChar &a) { Impl->EnterChar(a.value); },
        },
        action
    );
}

string TextBuffer::GetText() const { return Impl->GetText(); }
bool TextBuffer::Empty() const { return Impl->Empty(); }

static bool IsPressed(ImGuiKeyChord chord) {
    const auto window_id = ImGui::GetCurrentWindowRead()->ID;
    ImGui::SetKeyOwnersForKeyChord(chord, window_id); // Prevent app from handling this key press.
    return ImGui::IsKeyChordPressed(chord, ImGuiInputFlags_Repeat, window_id);
}

std::optional<TextBuffer::ActionType> TextBuffer::ProduceKeyboardAction() const {
    using namespace Action::TextBuffer;

    // history
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) return Undo{Id};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_Z)) return Redo{Id};
    // no-select moves
    if (IsPressed(ImGuiKey_UpArrow)) return MoveCursorsLines{.component_id = Id, .amount = -1, .select = false};
    if (IsPressed(ImGuiKey_DownArrow)) return MoveCursorsLines{.component_id = Id, .amount = 1, .select = false};
    if (IsPressed(ImGuiKey_LeftArrow)) return MoveCursorsChar{.component_id = Id, .right = false, .select = false, .word = false};
    if (IsPressed(ImGuiKey_RightArrow)) return MoveCursorsChar{.component_id = Id, .right = true, .select = false, .word = false};
    if (IsPressed(ImGuiMod_Alt | ImGuiKey_LeftArrow)) return MoveCursorsChar{.component_id = Id, .right = false, .select = false, .word = true};
    if (IsPressed(ImGuiMod_Alt | ImGuiKey_RightArrow)) return MoveCursorsChar{.component_id = Id, .right = true, .select = false, .word = true};
    if (IsPressed(ImGuiKey_PageUp)) return PageCursorsLines{.component_id = Id, .up = false, .select = false};
    if (IsPressed(ImGuiKey_PageDown)) return PageCursorsLines{.component_id = Id, .up = true, .select = false};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Home)) return MoveCursorsTop{.component_id = Id, .select = false};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_End)) return MoveCursorsBottom{.component_id = Id, .select = false};
    if (IsPressed(ImGuiKey_Home)) return MoveCursorsStartLine{.component_id = Id, .select = false};
    if (IsPressed(ImGuiKey_End)) return MoveCursorsEndLine{.component_id = Id, .select = false};
    // select moves
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_UpArrow)) return MoveCursorsLines{.component_id = Id, .amount = -1, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_DownArrow)) return MoveCursorsLines{.component_id = Id, .amount = 1, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_LeftArrow)) return MoveCursorsChar{.component_id = Id, .right = false, .select = true, .word = false};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_RightArrow)) return MoveCursorsChar{.component_id = Id, .right = true, .select = true, .word = false};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_LeftArrow)) return MoveCursorsChar{.component_id = Id, .right = false, .select = true, .word = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Alt | ImGuiKey_RightArrow)) return MoveCursorsChar{.component_id = Id, .right = true, .select = true, .word = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_PageUp)) return PageCursorsLines{.component_id = Id, .up = false, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_PageDown)) return PageCursorsLines{.component_id = Id, .up = true, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_Home)) return MoveCursorsTop{.component_id = Id, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_End)) return MoveCursorsBottom{.component_id = Id, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_Home)) return MoveCursorsStartLine{.component_id = Id, .select = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_End)) return MoveCursorsEndLine{.component_id = Id, .select = true};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_A)) return SelectAll{Id};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_D)) return SelectNextOccurrence{Id};
    // cut/copy/paste
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Insert) || IsPressed(ImGuiMod_Ctrl | ImGuiKey_C)) return Copy{Id};
    if (IsPressed(ImGuiMod_Shift | ImGuiKey_Insert) || IsPressed(ImGuiMod_Ctrl | ImGuiKey_V)) return Paste{Id};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_X) || IsPressed(ImGuiMod_Shift | ImGuiKey_Delete)) {
        if (Impl->ReadOnly) return Copy{Id};
        return Cut{Id};
    }
    // todo readonly toggle
    if (IsPressed(ImGuiKey_Insert)) return ToggleOverwrite{Id};
    // edits
    if (IsPressed(ImGuiKey_Delete)) return Delete{.component_id = Id, .word = false};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Delete)) return Delete{.component_id = Id, .word = true};
    if (IsPressed(ImGuiKey_Backspace)) return Backspace{.component_id = Id, .word = false};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Backspace)) return Backspace{.component_id = Id, .word = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_K)) return DeleteCurrentLines{Id};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_LeftBracket) || IsPressed(ImGuiMod_Shift | ImGuiKey_Tab)) {
        return ChangeCurrentLinesIndentation{.component_id = Id, .increase = false};
    }
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_RightBracket) || (IsPressed(ImGuiKey_Tab) && Impl->AnyCursorsMultiline())) {
        return ChangeCurrentLinesIndentation{.component_id = Id, .increase = true};
    }
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_UpArrow)) return MoveCurrentLines{.component_id = Id, .up = true};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_DownArrow)) return MoveCurrentLines{.component_id = Id, .up = false};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Slash)) return ToggleLineComment{.component_id = Id};
    if (IsPressed(ImGuiKey_Tab)) return EnterChar{.component_id = Id, .value = '\t'};
    if (IsPressed(ImGuiKey_Enter) || IsPressed(ImGuiKey_KeypadEnter)) return EnterChar{.component_id = Id, .value = '\n'};

    return {};
}

using namespace ImGui;

std::optional<TextBuffer::ActionType> TextBufferImpl::HandleMouseInputs(ImVec2 char_advance, float text_start_x) {
    using namespace Action::TextBuffer;
    constexpr static ImGuiMouseButton MouseLeft = ImGuiMouseButton_Left, MouseMiddle = ImGuiMouseButton_Middle;

    if (!IsWindowHovered()) {
        DestroyHoveredNode();
        return {};
    }

    SetMouseCursor(ImGuiMouseCursor_TextInput);

    if (IsMouseDown(MouseMiddle) && IsMouseDragging(MouseMiddle)) {
        const auto scroll = ImVec2{GetScrollX(), GetScrollY()} - GetMouseDragDelta(MouseMiddle);
        SetScrollX(scroll.x);
        SetScrollY(scroll.y);
    }

    bool is_over_line_number = false;
    const auto mouse_pos = GetMousePos();
    const auto mouse_lc = ToLineChar(ScreenPosToCoords(mouse_pos, char_advance, text_start_x, &is_over_line_number));
    const auto &io = GetIO();
    const auto is_click = IsMouseClicked(MouseLeft);
    if ((io.KeyShift && is_click) || IsMouseDragging(MouseLeft)) {
        return SetCursorRange{Id, Cursor{LastAddedCursor().Start, mouse_lc}, false};
    }
    if (io.KeyShift || io.KeyAlt) return {};

    if (is_over_line_number) DestroyHoveredNode();
    else if (Syntax) CreateHoveredNode(ToByteIndex(mouse_lc));

    const float time = GetTime();
    const bool is_double_click = IsMouseDoubleClicked(MouseLeft);
    const bool is_triple_click = is_click && !is_double_click && LastClickTime != -1.0f &&
        time - LastClickTime < io.MouseDoubleClickTime && Distance(io.MousePos, LastClickPos) < 0.01f;
    if (is_triple_click) {
        LastClickTime = -1.0f;
        return SetCursorRange{Id, Clamped({mouse_lc.L, 0}, CheckedNextLineBegin(mouse_lc.L)), io.KeyCtrl};
    } else if (is_double_click) {
        LastClickTime = time;
        LastClickPos = mouse_pos;
        return SetCursorRange{Id, Clamped(FindWordBoundary(mouse_lc, true), FindWordBoundary(mouse_lc, false)), io.KeyCtrl};
    } else if (is_click) {
        LastClickTime = time;
        LastClickPos = mouse_pos;
        auto lcr = is_over_line_number ? Clamped({mouse_lc.L, 0}, CheckedNextLineBegin(mouse_lc.L)) : Clamped(mouse_lc, mouse_lc);
        return SetCursorRange{Id, std::move(lcr), io.KeyCtrl};
    }

    return {};
}

std::optional<TextBuffer::ActionType> TextBufferImpl::Render(bool is_focused) {
    static constexpr float ScrollbarWidth = 14, LeftMargin = 10;

    const float font_size = GetFontSize();
    const float font_width = GetFont()->CalcTextSizeA(font_size, FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    const float font_height = GetTextLineHeightWithSpacing();
    const ImVec2 char_advance = {font_width, font_height * LineSpacing};
    // Line-number column has room for the max line-num digits plus two spaces.
    const float text_start_x = LeftMargin + (ShowLineNumbers ? std::format("{}  ", std::max(0, int(Text.size()) - 1)).size() * font_width : 0);

    const ImVec2 scroll{GetScrollX(), GetScrollY()};
    const ImVec2 cursor_screen_pos = GetCursorScreenPos();
    ContentDims = {
        GetWindowWidth() - (CurrentSpaceDims.x > ContentDims.x ? ScrollbarWidth : 0.0f),
        GetWindowHeight() - (CurrentSpaceDims.y > ContentDims.y ? ScrollbarWidth : 0.0f)
    };
    const Coords first_visible_coords{u32(scroll.y / char_advance.y), u32(std::max(scroll.x - text_start_x, 0.0f) / char_advance.x)};
    const Coords last_visible_coords{u32((ContentDims.y + scroll.y) / char_advance.y), u32((ContentDims.x + scroll.x - text_start_x) / char_advance.x)};
    ContentCoordDims = last_visible_coords - first_visible_coords + Coords{1, 1};

    if (auto edited_cursor = GetEditedCursor(); edited_cursor) {
        StartEdited.clear();
        EndEdited.clear();

        // Move scroll to keep the edited cursor visible.
        // Goal: Keep all edited cursor(s) visible at all times.
        // So, vars like `end_in_view` mean, "is the end of the edited cursor _fully_ in view?"
        // We assume at least the end has been edited, since it's the _interactive_ end.
        const Coords end{edited_cursor->End.L, GetColumn(edited_cursor->End)};

        const bool end_in_view = end.L > first_visible_coords.L && end.L < (std::min(last_visible_coords.L, 1u) - 1) &&
            end.C >= first_visible_coords.C && end.C < last_visible_coords.C;
        // const bool target_start = edited_cursor->StartEdited && end_in_view;
        const bool target_start = end_in_view;
        const auto target = target_start ? Coords{edited_cursor->Start.L, GetColumn(edited_cursor->Start)} : end;
        if (target.L <= first_visible_coords.L) {
            SetScrollY(std::max((target.L - 0.5f) * char_advance.y, 0.f));
        } else if (target.L >= last_visible_coords.L) {
            SetScrollY(std::max((target.L + 1.5f) * char_advance.y - ContentDims.y, 0.f));
        }
        if (target.C <= first_visible_coords.C) {
            SetScrollX(std::clamp(text_start_x + (target.C - 0.5f) * char_advance.x, 0.f, scroll.x));
        } else if (target.C >= last_visible_coords.C) {
            SetScrollX(std::max(text_start_x + (target.C + 1.5f) * char_advance.x - ContentDims.x, 0.f));
        }
    }

    const auto mouse_action = HandleMouseInputs(char_advance, text_start_x);

    u32 max_column = 0;
    auto dl = GetWindowDrawList();
    auto transition_it = Syntax->CaptureIdTransitions.begin();
    for (u32 li = first_visible_coords.L, byte_index = ToByteIndex({first_visible_coords.L, 0});
         li <= last_visible_coords.L && li < Text.size(); ++li) {
        const auto &line = Text[li];
        const u32 line_max_column = GetLineMaxColumn(line, last_visible_coords.C);
        max_column = std::max(line_max_column, max_column);

        const ImVec2 line_start_screen_pos{cursor_screen_pos.x, cursor_screen_pos.y + li * char_advance.y};
        const float text_screen_x = line_start_screen_pos.x + text_start_x;
        const Coords line_start_coord{li, 0}, line_end_coord{li, line_max_column};
        // Draw current line selection
        for (const auto &c : Cursors) {
            const auto selection_start = ToCoords(c.Min()), selection_end = ToCoords(c.Max());
            if (selection_start <= line_end_coord && selection_end > line_start_coord) {
                const u32 start_col = selection_start > line_start_coord ? selection_start.C : 0;
                const u32 end_col = selection_end < line_end_coord ?
                    selection_end.C :
                    line_end_coord.C + (selection_end.L > li || (selection_end.L == li && selection_end > line_end_coord) ? 1 : 0);
                if (start_col < end_col) {
                    const ImVec2 rect_start{text_screen_x + start_col * char_advance.x, line_start_screen_pos.y};
                    const ImVec2 rect_end = rect_start + ImVec2{(end_col - start_col) * char_advance.x, char_advance.y};
                    dl->AddRectFilled(rect_start, rect_end, GetColor(PaletteIndex::Selection));
                }
            }
        }

        if (ShowLineNumbers) {
            // Draw line number (right aligned).
            const string line_num_str = std::format("{}  ", li);
            dl->AddText({text_screen_x - line_num_str.size() * font_width, line_start_screen_pos.y}, GetColor(PaletteIndex::LineNumber), line_num_str.c_str());
        }

        // Render cursors
        if (is_focused) {
            {
                // (Copied from ImGui::InputTextEx)
                // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
                auto &g = *GImGui;
                g.PlatformImeData.WantVisible = true;
                g.PlatformImeData.InputPos = {cursor_screen_pos.x - 1.0f, cursor_screen_pos.y - g.FontSize};
                g.PlatformImeData.InputLineHeight = g.FontSize;
                g.PlatformImeViewport = ImGui::GetCurrentWindowRead()->Viewport->ID;
            }

            for (const auto &c : filter(Cursors, [li](const auto &c) { return c.Line() == li; })) {
                const u32 ci = c.CharIndex(), column = GetColumn(line, ci);
                const float width = !Overwrite || ci >= line.size() ? 1.f : (line[ci] == '\t' ? NumTabSpacesAtColumn(column) : 1) * char_advance.x;
                const ImVec2 pos{text_screen_x + column * char_advance.x, line_start_screen_pos.y};
                dl->AddRectFilled(pos, pos + ImVec2{width, char_advance.y}, GetColor(PaletteIndex::Cursor));
            }
        }

        // Render colorized text
        const u32 line_start_byte_index = byte_index;
        const u32 start_ci = GetFirstVisibleCharIndex(line, first_visible_coords.C);
        byte_index += start_ci;
        transition_it.MoveForwardTo(byte_index);
        for (u32 ci = start_ci, column = first_visible_coords.C; ci < line.size() && column <= last_visible_coords.C;) {
            const auto lc = LineChar{li, ci};
            const ImVec2 glyph_pos = line_start_screen_pos + ImVec2{text_start_x + column * char_advance.x, 0};
            const char ch = line[lc.C];
            const u32 seq_length = UTF8CharLength(ch);
            if (ch == '\t') {
                if (ShowWhitespaces) {
                    const float gap = font_size * (ShortTabs ? 0.16f : 0.2f);
                    const ImVec2 p1{glyph_pos + ImVec2{char_advance.x * 0.3f, font_height * 0.5f}};
                    const ImVec2 p2{glyph_pos.x + char_advance.x * (ShortTabs ? (NumTabSpacesAtColumn(column) - 0.3f) : 1.f), p1.y};
                    const u32 color = GetColor(PaletteIndex::ControlCharacter);
                    dl->AddLine(p1, p2, color);
                    dl->AddLine(p2, {p2.x - gap, p1.y - gap}, color);
                    dl->AddLine(p2, {p2.x - gap, p1.y + gap}, color);
                }
            } else if (ch == ' ') {
                if (ShowWhitespaces) {
                    dl->AddCircleFilled(glyph_pos + ImVec2{font_width, font_size} * 0.5f, 1.5f, GetColor(PaletteIndex::ControlCharacter), 4);
                }
            } else {
                if (seq_length == 1 && Cursors.size() == 1) {
                    if (const auto matching_brackets = FindMatchingBrackets(Cursors.front())) {
                        if (matching_brackets->Start == lc || matching_brackets->End == lc) {
                            const ImVec2 start{glyph_pos + ImVec2{0, font_height + 1.0f}};
                            dl->AddRectFilled(start, start + ImVec2{char_advance.x, 1.0f}, GetColor(PaletteIndex::Cursor));
                        }
                    }
                }
                // Render the current character.
                const auto &char_style = Syntax->StyleByCaptureId.at(*transition_it);
                const bool font_changed = Fonts::Push(FontFamily::Monospace, char_style.Font);
                const char *seq_begin = &line[ci];
                dl->AddText(glyph_pos, char_style.Color, seq_begin, seq_begin + seq_length);
                if (font_changed) Fonts::Pop();
            }
            if (ShowStyleTransitionPoints && !transition_it.IsEnd() && transition_it.ByteIndex == byte_index) {
                const auto color = SetAlpha(Syntax->StyleByCaptureId.at(*transition_it).Color, 40);
                dl->AddRectFilled(glyph_pos, glyph_pos + char_advance, color);
            }
            if (ShowChangedCaptureRanges) {
                for (const auto &range : Syntax->ChangedCaptureRanges) {
                    if (byte_index >= range.Start && byte_index < range.End) {
                        dl->AddRectFilled(glyph_pos, glyph_pos + char_advance, Col32(255, 255, 255, 20));
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
        std::max((max_column + std::min(ContentCoordDims.C - 1, max_column)) * char_advance.x, CurrentSpaceDims.x),
        (Text.size() + std::min(ContentCoordDims.L - 1, u32(Text.size()))) * char_advance.y
    };

    ImGui::SetCursorPos({0, 0});

    // Stack invisible items to push node hierarchy to ImGui stack.
    if (Syntax && HoveredNode) {
        const auto before_cursor = ImGui::GetCursorScreenPos();
        for (const auto &node : HoveredNode->Ancestry) {
            PushOverrideID(node.Id);
            InvisibleButton("", CurrentSpaceDims, ImGuiButtonFlags_AllowOverlap);
            ImGui::SetCursorScreenPos(before_cursor);
        }
        for (u32 i = 0; i < HoveredNode->Ancestry.size(); ++i) PopID();
    }

    Dummy(CurrentSpaceDims);

    return mouse_action;
}

static void DrawEdits(const std::ranges::input_range auto &edits) {
    Text("Edits: %lu", edits.size());
    for (const auto &edit : edits) {
        BulletText("Start: %d, Old end: %d, New end: %d", edit.StartByte, edit.OldEndByte, edit.NewEndByte);
    }
}

void TextBufferImpl::DebugPanel() {
    if (CollapsingHeader("Editor state")) {
        ImGui::Text("Cursor count: %lu", Cursors.size());
        for (const auto &c : Cursors) {
            const auto &start = c.Start, &end = c.End;
            ImGui::Text("Start: {%d, %d}(%u), End: {%d, %d}(%u)", start.L, start.C, ToByteIndex(start), end.L, end.C, ToByteIndex(end));
        }
        if (CollapsingHeader("Line lengths")) {
            for (u32 i = 0; i < Text.size(); i++) ImGui::Text("%u: %lu", i, Text[i].size());
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

void TextBuffer::Render() const {
    static string PrevSelectedPath = "";
    if (FileDialog.OwnerId == Id && PrevSelectedPath != FileDialog.SelectedFilePath) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        PrevSelectedPath = FileDialog.SelectedFilePath = "";
        if (FileDialog.SaveMode) Q(Action::TextBuffer::Save{Id, selected_path});
        else Q(Action::TextBuffer::Open{Id, selected_path});
    }

    const auto cursor_coords = Impl->GetCursorPosition();
    const string editing_file = LastOpenedFilePath ? string(fs::path(LastOpenedFilePath).filename()) : "No file";
    Text(
        "%6d/%-6d %6d lines  | %s | %s | %s | %s", cursor_coords.L + 1, cursor_coords.C + 1, Impl->LineCount(),
        Impl->Overwrite ? "Ovr" : "Ins",
        Impl->CanUndo() ? "*" : " ",
        Impl->GetLanguageName().data(),
        editing_file.c_str()
    );

    const bool is_parent_focused = IsWindowFocused();
    PushStyleColor(ImGuiCol_ChildBg, Impl->GetColor(PaletteIndex::Background));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    BeginChild("TextBuffer", {}, false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

    const bool font_changed = Fonts::Push(FontFamily::Monospace);
    const bool is_focused = IsWindowFocused() || is_parent_focused;
    if (is_focused) {
        auto &io = GetIO();
        io.WantCaptureKeyboard = io.WantTextInput = true;

        // (Copied from ImGui::InputTextEx)
        // Process regular text input (before we check for Return because using some IME will effectively send a Return?)
        // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
        const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (io.ConfigMacOSXBehaviors && io.KeyCtrl);
        if (auto action = ProduceKeyboardAction()) Q(*action);
        else if (!io.InputQueueCharacters.empty() && !ignore_char_inputs) {
            for (const auto ch : io.InputQueueCharacters) {
                if (ch != 0 && (ch == '\n' || ch >= 32)) Q(Action::TextBuffer::EnterChar{.component_id = Id, .value = ch});
            }
            io.InputQueueCharacters.resize(0);
        }
    }
    if (auto action = Impl->Render(is_focused)) Q(*action);
    if (font_changed) Fonts::Pop();

    EndChild();
    PopStyleVar();
    PopStyleColor();
}

void TextBuffer::RenderMenu() const {
    FileMenu.Draw();

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
            if (MenuItem("Mariana palette")) Impl->SetPalette(TextBufferPaletteId::Mariana);
            if (MenuItem("Dark palette")) Impl->SetPalette(TextBufferPaletteId::Dark);
            if (MenuItem("Light palette")) Impl->SetPalette(TextBufferPaletteId::Light);
            if (MenuItem("Retro blue palette")) Impl->SetPalette(TextBufferPaletteId::RetroBlue);
            EndMenu();
        }
        MenuItem("Show style transition points", nullptr, &Impl->ShowStyleTransitionPoints);
        MenuItem("Show changed capture ranges", nullptr, &Impl->ShowChangedCaptureRanges);
        gWindows.ToggleDebugMenuItem(Debug);
        EndMenu();
    }
}

void TextBuffer::RenderDebug() const { Impl->DebugPanel(); }
