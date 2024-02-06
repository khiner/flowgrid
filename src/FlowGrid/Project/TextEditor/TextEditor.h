#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <immer/algorithm.hpp>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <immer/vector.hpp>

#include "imgui.h"

#include "LanguageID.h"

namespace fs = std::filesystem;

struct TSLanguage;
struct TSTree;
struct TSQuery;
struct TSQueryCursor;
struct TSParser;

// These classes corresponds to tree-sitter's `config.json`.
// https://tree-sitter.github.io/tree-sitter/syntax-highlighting#per-user-configuration
struct TextEditorStyle {
    struct CharStyle {
        ImU32 Color{IM_COL32_WHITE};
        bool Bold{false}, Italic{false}, Underline{false};
    };
};
struct TSConfig {
    std::vector<std::string> ParserDirectories{};
    std::unordered_map<std::string, TextEditorStyle::CharStyle> StyleByHighlightName{};

    inline static const TextEditorStyle::CharStyle DefaultCharStyle{};

    TextEditorStyle::CharStyle FindStyleByCaptureName(const std::string &) const;
};

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

struct LanguageDefinition {
    // todo recursively copy `queries` dir to build dir in CMake.
    inline static fs::path QueriesDir = fs::path("..") / "src" / "FlowGrid" / "Project" / "TextEditor" / "queries";

    TSQuery *GetQuery() const;

    LanguageID Id;
    std::string Name;
    std::string ShortName{""}; // e.g. "cpp" in "tree-sitter-cpp"
    TSLanguage *TsLanguage{nullptr};
    std::unordered_set<std::string> FileExtensions{};
    std::string SingleLineComment{""};
};

struct LanguageDefinitions {
    using ID = LanguageID;

    LanguageDefinitions();

    const LanguageDefinition &Get(ID id) const { return ById.at(id); }

    std::unordered_map<ID, LanguageDefinition> ById;
    std::unordered_map<std::string, LanguageID> ByFileExtension;
    std::string AllFileExtensionsFilter;
};

struct TextEditor {
    TextEditor(std::string_view text = "", LanguageID language_id = LanguageID::None);
    TextEditor(const fs::path &);
    ~TextEditor();

    enum class PaletteIdT {
        Dark,
        Light,
        Mariana,
        RetroBlue
    };

    inline static const PaletteIdT DefaultPaletteId{PaletteIdT::Dark};
    inline static const LanguageDefinitions Languages{};

    // Represents a character coordinate from the user's point of view,
    // i. e. consider a uniform grid (assuming fixed-width font) on the screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..NumTabSpaces] u32 empty spaces, depending on how many space is necessary to reach the next tab stop.
    // For example, `Coords{1, 5}` represents the character 'B' in the line "\tABC", when NumTabSpaces = 4, since it is rendered as "    ABC".
    struct Coords {
        uint L{0}, C{0}; // Line, Column

        auto operator<=>(const Coords &o) const {
            if (auto cmp = L <=> o.L; cmp != 0) return cmp;
            return C <=> o.C;
        }
        bool operator==(const Coords &) const = default;
        bool operator!=(const Coords &) const = default;

        Coords operator-(const Coords &o) const { return {L - o.L, C - o.C}; }
        Coords operator+(const Coords &o) const { return {L + o.L, C + o.C}; }
    };

    struct LineChar {
        uint L{0}, C{0};

        auto operator<=>(const LineChar &o) const {
            if (auto cmp = L <=> o.L; cmp != 0) return cmp;
            return C <=> o.C;
        }
        bool operator==(const LineChar &) const = default;
        bool operator!=(const LineChar &) const = default;
    };

    using Line = immer::flex_vector<char>;
    using Lines = immer::flex_vector<Line>;
    using TransientLine = immer::flex_vector_transient<char>;
    using TransientLines = immer::flex_vector_transient<Line>;
    using PaletteLine = immer::flex_vector<PaletteIndex>;
    using PaletteLines = immer::flex_vector<PaletteLine>;

    uint LineCount() const { return Text.size(); }
    const Line &GetLine(uint li) const { return Text[li]; }
    LineChar GetCursorPosition() const { return Cursors.back().LC(); }
    std::string GetText(LineChar start, LineChar end) const;
    std::string GetText() const { return GetText(BeginLC(), EndLC()); }
    std::string GetSyntaxTreeSExp() const;
    const LanguageDefinition &GetLanguage() const { return Languages.Get(LanguageId); }
    ImU32 GetColor(PaletteIndex index) const { return GetPalette()[uint(index)]; }

    void SetText(const std::string &);
    void SetFilePath(const fs::path &);
    void SetPalette(PaletteIdT);
    void SetLanguage(LanguageID);
    void SetNumTabSpaces(uint);
    void SetLineSpacing(float);

    void SelectAll();

    void Undo();
    void Redo();
    void Copy();
    void Cut();
    void Paste();
    bool CanUndo() const { return !ReadOnly && HistoryIndex > 0; }
    bool CanRedo() const { return !ReadOnly && History.size() > 1 && HistoryIndex < uint(History.size() - 1); }
    bool CanCopy() const { return Cursors.AnyRanged(); }
    bool CanCut() const { return !ReadOnly && CanCopy(); }
    bool CanPaste() const;

    bool Render(bool is_parent_focused = false);
    void DebugPanel();

    bool ReadOnly{false};
    bool Overwrite{false};
    bool AutoIndent{true};
    bool ShowWhitespaces{true};
    bool ShowLineNumbers{true};
    bool ShowStyleTransitionPoints{false};
    bool ShortTabs{true};
    float LineSpacing{1};

private:
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

        void MoveRight();
        void MoveLeft();
    };

    struct Cursor {
        Cursor() = default;

        Cursor(LineChar lc) : Start(lc), End(lc) {}
        Cursor(LineChar start, LineChar end) : Start(start), End(end) {}

        bool operator==(const Cursor &) const = default;
        bool operator!=(const Cursor &) const = default;

        LineChar GetStart() const { return Start; }
        LineChar GetEnd() const { return End; }
        uint GetStartColumn(const TextEditor &editor) {
            if (!StartColumn) StartColumn = editor.GetColumn(Start);
            return *StartColumn;
        }
        uint GetEndColumn(const TextEditor &editor) {
            if (!EndColumn) EndColumn = editor.GetColumn(End);
            return *EndColumn;
        }
        Coords GetStartCoords(const TextEditor &editor) { return {Start.L, GetStartColumn(editor)}; }
        Coords GetEndCoords(const TextEditor &editor) { return {End.L, GetEndColumn(editor)}; }
        uint Line() const { return End.L; }
        uint CharIndex() const { return End.C; }
        LineChar LC() const { return End; } // Be careful if this is a multiline cursor!

        LineChar Min() const { return std::min(Start, End); }
        LineChar Max() const { return std::max(Start, End); }

        bool IsRange() const { return Start != End; }
        bool IsMultiline() const { return Start.L != End.L; }
        bool IsRightOf(LineChar lc) const { return End.L == lc.L && End.C > lc.C; }

        bool IsEdited() const { return StartEdited || EndEdited; }
        bool IsStartEdited() const { return StartEdited; }
        bool IsEndEdited() const { return EndEdited; }
        void MarkEdited() { StartEdited = EndEdited = true; }
        void ClearEdited() { StartEdited = EndEdited = false; }

        void SetStart(LineChar start, std::optional<uint> start_column = std::nullopt) {
            Start = std::move(start);
            StartColumn = start_column;
            EndEdited = true; // todo maybe only if changed?
        }
        void SetEnd(LineChar end, std::optional<uint> end_column = std::nullopt) {
            End = std::move(end);
            EndColumn = end_column;
            EndEdited = true; // todo maybe only if changed?
        }
        void Set(LineChar end, bool set_both, std::optional<uint> end_column = std::nullopt) {
            if (set_both) SetStart(end, end_column);
            SetEnd(end, end_column);
        }
        void Set(LineChar lc, std::optional<uint> column = std::nullopt) { Set(lc, true, column); }
        void Set(LineChar start, LineChar end, std::optional<uint> start_column = std::nullopt, std::optional<uint> end_column = std::nullopt) {
            SetStart(start, start_column);
            SetEnd(end, end_column);
        }

        void MoveChar(const TextEditor &, bool right = true, bool select = false, bool is_word_mode = false);
        void MoveLines(const TextEditor &, int amount = 1, bool select = false, bool move_start = false, bool move_end = true);

    private:
        // `Start` and `End` are the the first and second coordinate _set in an interaction_.
        // Use `Min()` and `Max()` for position ordering.
        LineChar Start{}, End{Start};

        // A column is emptied when its respective `LineChar` is changed without the caller providing an explicit column.
        // A column is computed and set on demand when its `LineChar` is read.
        // Operationally, this means that if a column is never read, it is never computed,
        // and a non-empty column is always up-to-date with its latest `LineChar` value.
        std::optional<uint> StartColumn{}, EndColumn{};
        // Cleared every frame. Used to keep recently edited cursors visible.
        // todo These should not be stored in history, since all cursors are marked as edited during an undo/redo.
        bool StartEdited{false}, EndEdited{false};
    };

    struct Cursors {
        auto begin() const { return Cursors.begin(); }
        auto end() const { return Cursors.end(); }
        auto begin() { return Cursors.begin(); }
        auto end() { return Cursors.end(); }
        auto cbegin() const { return Cursors.cbegin(); }
        auto cend() const { return Cursors.cend(); }

        Cursor &operator[](uint i) { return Cursors[i]; }
        const Cursor &operator[](uint i) const { return Cursors[i]; }
        const Cursor &front() const { return Cursors.front(); }
        const Cursor &back() const { return Cursors.back(); }
        Cursor &front() { return Cursors.front(); }
        Cursor &back() { return Cursors.back(); }
        auto size() const { return Cursors.size(); }

        bool AnyRanged() const;
        bool AllRanged() const;
        bool AnyMultiline() const;
        bool AnyEdited() const;

        void Add();
        void Reset();
        void MarkEdited() {
            for (Cursor &c : Cursors) c.MarkEdited();
        }
        void ClearEdited() {
            for (Cursor &c : Cursors) c.ClearEdited();
        }
        uint GetLastAddedIndex() { return LastAddedIndex >= size() ? 0 : LastAddedIndex; }
        Cursor &GetLastAdded() { return Cursors[GetLastAddedIndex()]; }
        void SortAndMerge();

        void MoveLines(const TextEditor &, int amount = 1, bool select = false, bool move_start = false, bool move_end = true);
        void MoveChar(const TextEditor &, bool right = true, bool select = false, bool is_word_mode = false);
        void MoveTop(bool select = false);
        void MoveBottom(const TextEditor &, bool select = false);
        void MoveStart(bool select = false);
        void MoveEnd(const TextEditor &, bool select = false);

        // Returns the range of all edited cursor starts/ends since the last call to `ClearEdited()`.
        // Used for updating the scroll range.
        // todo need to update the approach here after switching to persistent undo.
        std::optional<Cursor> GetEditedCursor();

    private:
        std::vector<Cursor> Cursors{{{0, 0}}};
        uint LastAddedIndex{0};
    };

    void Record(); // Every `Record` should be paired with a `BeforeCursors = Cursors`.

    std::string GetSelectedText(const Cursor &c) const { return GetText(c.Min(), c.Max()); }

    static LineChar BeginLC() { return {0, 0}; }
    LineChar EndLC() const { return {uint(Text.size() - 1), uint(Text.back().size())}; }
    LinesIter Iter(LineChar lc, LineChar begin, LineChar end) const { return {Text, std::move(lc), std::move(begin), std::move(end)}; }
    LinesIter Iter(LineChar lc = BeginLC()) const { return Iter(std::move(lc), BeginLC(), EndLC()); }

    LineChar LineMaxLC(uint li) const { return {li, GetLineMaxCharIndex(li)}; }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetColumn(Text[lc.L], lc.C)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(std::move(coords))}; }
    uint ToByteIndex(LineChar) const;
    void MoveCharIndexAndColumn(const Line &, uint &ci, uint &column) const;

    Coords ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const;
    LineChar ScreenPosToLC(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const { return ToLineChar(ScreenPosToCoords(screen_pos, is_over_li)); }
    uint GetCharIndex(const Line &, uint column) const;
    uint GetCharIndex(Coords coords) const { return GetCharIndex(Text[coords.L], coords.C); }
    uint GetColumn(const Line &, uint ci) const;
    uint GetColumn(LineChar lc) const { return GetColumn(Text[lc.L], lc.C); }
    uint GetFirstVisibleCharIndex(const Line &, uint first_visible_column) const;
    uint GetLineMaxColumn(const Line &) const;
    uint GetLineMaxColumn(const Line &, uint limit) const;
    uint GetLineMaxCharIndex(uint li) const { return Text[li].size(); }

    void EnterChar(ImWchar, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void Delete(bool is_word_mode = false);

    void SetSelection(LineChar start, LineChar end, Cursor &);
    void AddCursorForNextOccurrence(bool case_sensitive = true);

    LineChar FindWordBoundary(LineChar from, bool is_start = false) const;
    // Returns a cursor containing the start/end positions of the next occurrence of `text` at or after `start`, or `std::nullopt` if not found.
    std::optional<Cursor> FindNextOccurrence(const std::string &text, LineChar start, bool case_sensitive = true);
    std::optional<Cursor> FindMatchingBrackets(const Cursor &);
    uint NumStartingSpaceColumns(uint li) const;

    void ChangeCurrentLinesIndentation(bool increase);
    void MoveCurrentLines(bool up);
    void ToggleLineComment();
    void RemoveCurrentLines();
    void SwapLines(uint li1, uint li2);

    LineChar InsertText(Lines, LineChar); // Returns insertion end.
    void InsertTextAtCursor(Lines, Cursor &);
    void DeleteRange(LineChar start, LineChar end, const Cursor *exclude_cursor = nullptr);
    void DeleteSelection(Cursor &);

    void HandleKeyboardInputs();
    void HandleMouseInputs();

    /**
    `start_byte`: Start position of the text change.
    `old_end_byte`: End position of the original text before the change.
      - For insertion, same as `start`.
      - For replacement, where the replaced text ended.
      - For deletion, where the deleted text ended.
    `new_end_byte`: End position of the new text after the change.
      - For insertion or replacement, where the new text ends.
      - For deletion, same as `start`.
    **/
    void OnTextChanged(uint start_byte, uint old_end_byte, uint new_end_byte);

    uint NumTabSpacesAtColumn(uint column) const { return NumTabSpaces - (column % NumTabSpaces); }

    void Parse();

    inline static uint NoneCaptureId{uint(-1)}; // Maps to the default style.

    using PaletteT = std::array<ImU32, uint(PaletteIndex::Max)>;
    static const PaletteT DarkPalette, MarianaPalette, LightPalette, RetroBluePalette;

    const PaletteT &GetPalette() const;

    Lines Text{Line{}};
    Cursors Cursors, BeforeCursors;
    PaletteIdT PaletteId{DefaultPaletteId};
    LanguageID LanguageId{LanguageID::None};

    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    uint NumTabSpaces{4};
    uint LeftMargin{10};
    ImVec2 CharAdvance;

    ImVec2 ContentDims{0, 0}; // Pixel width/height of current content area.
    Coords ContentCoordDims{0, 0}; // Coords width/height of current content area.
    ImVec2 CurrentSpaceDims{20, 20}; // Pixel width/height given to `ImGui::Dummy`.
    ImVec2 LastClickPos{-1, -1}, LastPanMousePos{-1, -1};
    float LastClickTime{-1}; // ImGui time.
    std::optional<Cursor> MatchingBrackets{};
    bool ScrollToTop{false};

    TSConfig HighlightConfig;
    TSTree *Tree{nullptr};
    TSParser *Parser;
    TSQuery *Query{nullptr};
    TSQueryCursor *QueryCursor{nullptr};
    std::unordered_map<uint, TextEditorStyle::CharStyle> StyleByCaptureId{};
    std::map<uint, uint> CaptureIdByTransitionByte{};

    struct Snapshot {
        Lines Text;
        struct Cursors Cursors, BeforeCursors;
        TSTree *Tree;
    };
    // The first history record is the initial state (after construction), and it's never removed from the history.
    immer::vector<Snapshot> History;
    uint HistoryIndex{0};
};
