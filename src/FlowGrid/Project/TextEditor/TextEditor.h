#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgui.h"

#include "LanguageID.h"

namespace fs = std::filesystem;

struct TSLanguage;
struct TSTree;

enum class PaletteIndex {
    // Language
    Default,
    Keyword,
    NumberLiteral,
    StringLiteral,
    CharLiteral,
    Punctuation,
    Preprocessor,
    Operator,
    Identifier,
    Type,
    Comment,
    // Application
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
    using PaletteT = std::unordered_map<std::string, PaletteIndex>; // Key is TS node type name.

    static PaletteT CreatePalette(LanguageID);

    LanguageID Id;
    std::string Name;
    TSLanguage *TsLanguage{nullptr};
    std::unordered_set<std::string> FileExtensions{};
    std::string SingleLineComment{""};
    PaletteT Palette{CreatePalette(Id)};
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
    // Forward-declare wrapper structs for Tree-Sitter types.
    struct CodeParser;

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
    const LanguageDefinition &GetLanguage() const { return Languages.Get(LanguageId); }
    ImU32 GetColor(PaletteIndex index) const { return GetPalette()[uint(index)]; }

    // Represents a character coordinate from the user's point of view,
    // i. e. consider a uniform grid (assuming fixed-width font) on the screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..NumTabSpaces] u32 empty spaces, depending on how many space is necessary to reach the next tab stop.
    // For example, `Coords{1, 5}` represents the character 'B' in the line "\tABC", when NumTabSpaces = 4, since it is rendered as "    ABC" on the screen.
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

    using LineT = std::vector<char>;
    using LinesT = std::vector<LineT>;

    uint LineCount() const { return Lines.size(); }
    const LineT &GetLine(uint li) const { return Lines[li]; }

    void SetPalette(PaletteIdT);
    void SetLanguage(LanguageID);

    void SetNumTabSpaces(uint);
    void SetLineSpacing(float);

    void SelectAll();

    LineChar GetCursorPosition() const { return Cursors.back().LC(); }
    bool CanCopy() const { return Cursors.AnyRanged(); }

    void Copy();
    void Cut();
    void Paste();
    void Undo(uint steps = 1);
    void Redo(uint steps = 1);
    bool CanUndo() const { return !ReadOnly && UndoIndex > 0; }
    bool CanRedo() const { return !ReadOnly && UndoIndex < uint(UndoBuffer.size()); }

    void SetText(const std::string &);
    void SetFilePath(const fs::path &);

    std::string GetText(LineChar start, LineChar end) const;
    std::string GetText() const { return GetText(BeginLC(), EndLC()); }

    std::string GetSyntaxTreeSExp() const;

    bool Render(bool is_parent_focused = false);
    void DebugPanel();

    enum class SetViewAtLineMode {
        FirstVisibleLine,
        Centered,
        LastVisibleLine
    };

    bool ReadOnly{false};
    bool Overwrite{false};
    bool AutoIndent{true};
    bool ShowWhitespaces{true};
    bool ShowLineNumbers{true};
    bool ShortTabs{true};
    float LineSpacing{1};
    int SetViewAtLineI{-1};
    SetViewAtLineMode SetViewAtLineMode{SetViewAtLineMode::FirstVisibleLine};

private:
    struct LinesIter {
        LinesIter(const LinesT &lines, LineChar lc, LineChar begin, LineChar end)
            : Lines(lines), LC(std::move(lc)), Begin(std::move(begin)), End(std::move(end)) {}
        LinesIter(const LinesIter &) = default; // Needed since we have an assignment operator.

        LinesIter &operator=(const LinesIter &o) {
            LC = o.LC;
            Begin = o.Begin;
            End = o.End;
            return *this;
        }

        operator char() const {
            const auto &line = Lines[LC.L];
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
        const LinesT &Lines;
        LineChar LC, Begin, End;

        void MoveRight();
        void MoveLeft();
    };

    struct Cursor {
        Cursor() = default;
        Cursor(const Cursor &cursor) {
            *this = cursor;
        }
        // Copy everything but `Modified` fields.
        Cursor &operator=(const Cursor &other) {
            Start = other.Start;
            End = other.End;
            StartColumn = other.StartColumn;
            EndColumn = other.EndColumn;
            return *this;
        }

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

        bool IsEdited() const { return StartModified || EndModified; }
        bool IsStartEdited() const { return StartModified; }
        bool IsEndEdited() const { return EndModified; }
        void ClearEdited() { StartModified = EndModified = false; }

        void SetStart(LineChar start, std::optional<uint> column = std::nullopt) {
            Start = std::move(start);
            StartColumn = column;
            EndModified = true; // todo maybe only if changed?
        }
        void SetEnd(LineChar end, std::optional<uint> column = std::nullopt) {
            End = std::move(end);
            EndColumn = column;
            EndModified = true; // todo maybe only if changed?
        }
        void Set(LineChar lc, std::optional<uint> column = std::nullopt) {
            SetStart(lc, column);
            SetEnd(lc, column);
        }
        void Set(LineChar lc, bool both, std::optional<uint> column = std::nullopt) {
            if (both) SetStart(lc, column);
            SetEnd(lc, column);
        }
        void Set(LineChar start, LineChar end, std::optional<uint> start_column = std::nullopt, std::optional<uint> end_column = std::nullopt) {
            SetStart(start, start_column);
            SetEnd(end, end_column);
        }

        void MoveChar(const TextEditor &, bool right = true, bool select = false, bool is_word_mode = false);
        void MoveLines(const TextEditor &, int amount = 1, bool select = false);

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
        bool StartModified{false}, EndModified{false};
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

        void Add();
        void Reset();
        void ClearEdited() {
            for (Cursor &c : Cursors) c.ClearEdited();
        }
        uint GetLastAddedIndex() { return LastAddedIndex >= size() ? 0 : LastAddedIndex; }
        Cursor &GetLastAdded() { return Cursors[GetLastAddedIndex()]; }
        void SortAndMerge();

        void MoveLines(const TextEditor &, int amount = 1, bool select = false);
        void MoveChar(const TextEditor &, bool right = true, bool select = false, bool is_word_mode = false);
        void MoveTop(bool select = false);
        void MoveBottom(const TextEditor &, bool select = false);
        void MoveStart(bool select = false);
        void MoveEnd(const TextEditor &, bool select = false);

        // Returns the range of all edited cursor starts/ends since the last call to `ClearEdited()`.
        // Used for updating the scroll range.
        std::optional<std::pair<Coords, Coords>> GetEditedCoordRange(const TextEditor &);

    private:
        std::vector<Cursor> Cursors{{{0, 0}}};
        uint LastAddedIndex{0};
    };

    enum class UndoOperationType {
        Add,
        Delete,
    };
    struct UndoOperation {
        std::string Text;
        LineChar Start, End;
        UndoOperationType Type;
    };

    struct UndoRecord {
        UndoRecord() {}
        UndoRecord(const std::vector<UndoOperation> &ops, const Cursors &before, const Cursors &after)
            : Operations(ops), Before(before), After(after) {
            // for (const UndoOperation &o : Operations) assert(o.Start <= o.End);
        }
        UndoRecord(const Cursors &before) : Before(before) {}
        UndoRecord(std::vector<UndoOperation> &&ops, Cursors &&before, Cursors &&after)
            : Operations(std::move(ops)), Before(std::move(before)), After(std::move(after)) {}
        UndoRecord(Cursors &&before) : Before(std::move(before)) {}
        ~UndoRecord() = default;

        void Undo(TextEditor *);
        void Redo(TextEditor *);

        std::vector<UndoOperation> Operations{};
        Cursors Before{}, After{};
    };

    void OnCursorPositionChanged();

    void AddUndoOp(UndoRecord &, UndoOperationType, LineChar start, LineChar end);

    std::string GetSelectedText(const Cursor &c) const { return GetText(c.Min(), c.Max()); }
    ImU32 GetColor(LineChar lc) const { return GetColor(PaletteIndices[lc.L][lc.C]); }

    static LineChar BeginLC() { return {0, 0}; }
    LineChar EndLC() const { return {uint(Lines.size() - 1), uint(Lines.back().size())}; }
    LinesIter Iter(LineChar lc, LineChar begin, LineChar end) const { return {Lines, std::move(lc), std::move(begin), std::move(end)}; }
    LinesIter Iter(LineChar lc = BeginLC()) const { return Iter(std::move(lc), BeginLC(), EndLC()); }

    LineChar LineMaxLC(uint li) const { return {li, GetLineMaxCharIndex(li)}; }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetColumn(Lines[lc.L], lc.C)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(std::move(coords))}; }
    uint ToByteIndex(LineChar) const;
    void MoveCharIndexAndColumn(const LineT &, uint &ci, uint &column) const;

    Coords ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const;
    LineChar ScreenPosToLC(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const { return ToLineChar(ScreenPosToCoords(screen_pos, is_over_li)); }
    uint GetCharIndex(const LineT &, uint column) const;
    uint GetCharIndex(Coords coords) const { return GetCharIndex(Lines[coords.L], coords.C); }
    uint GetColumn(const LineT &, uint ci) const;
    uint GetColumn(LineChar lc) const { return GetColumn(Lines[lc.L], lc.C); }
    uint GetFirstVisibleCharIndex(uint li) const;
    uint GetLineMaxColumn(const LineT &) const;
    uint GetLineMaxColumn(const LineT &, uint limit) const;
    uint GetLineMaxCharIndex(uint li) const { return Lines[li].size(); }

    void EnterChar(ImWchar, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void Delete(bool is_word_mode = false, const TextEditor::Cursors *editor_state = nullptr);

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

    LineChar InsertTextAt(LineChar, const std::string &); // Returns insertion end.
    void InsertTextAtCursor(const std::string &, Cursor &, UndoRecord &);
    void DeleteRange(LineChar start, LineChar end, const Cursor *exclude_cursor = nullptr);
    void DeleteSelection(Cursor &, UndoRecord &);

    void AddOrRemoveGlyphs(LineChar lc, std::span<const char>, bool is_add);
    void AddGlyphs(LineChar lc, std::span<const char> glyphs) { AddOrRemoveGlyphs(std::move(lc), glyphs, true); }
    void RemoveGlyphs(LineChar lc, std::span<const char> glyphs) { AddOrRemoveGlyphs(std::move(lc), glyphs, false); }

    void HandleKeyboardInputs();
    void HandleMouseInputs();
    void UpdateViewVariables(float scroll_x, float scroll_y);

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

    void AddUndo(UndoRecord &);

    bool IsHorizontalScrollbarVisible() const { return CurrentSpaceWidth > ContentWidth; }
    bool IsVerticalScrollbarVisible() const { return CurrentSpaceHeight > ContentHeight; }
    uint NumTabSpacesAtColumn(uint column) const { return NumTabSpaces - (column % NumTabSpaces); }

    void Parse();
    void Highlight();

    using PaletteT = std::array<ImU32, uint(PaletteIndex::Max)>;
    static const PaletteT DarkPalette, MarianaPalette, LightPalette, RetroBluePalette;

    const PaletteT &GetPalette() const;

    LinesT Lines{LineT{}};
    std::vector<std::vector<PaletteIndex>> PaletteIndices{std::vector<PaletteIndex>{}};
    Cursors Cursors;

    std::vector<UndoRecord> UndoBuffer;
    uint UndoIndex{0};

    uint NumTabSpaces{4};
    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    uint LeftMargin{10};
    ImVec2 CharAdvance;
    float LastClickTime{-1}; // In ImGui time.
    ImVec2 LastClickPos{-1, -1};
    float CurrentSpaceWidth{20}, CurrentSpaceHeight{20.0f};
    Coords FirstVisibleCoords{0, 0}, LastVisibleCoords{0, 0};
    uint VisibleLineCount{0}, VisibleColumnCount{0};
    float ContentWidth{0}, ContentHeight{0};
    float ScrollX{0}, ScrollY{0};
    bool Panning{false}, IsDraggingSelection{false}, ScrollToTop{false};
    ImVec2 LastMousePos;
    bool CursorPositionChanged{false};
    std::optional<Cursor> MatchingBrackets{};
    PaletteIdT PaletteId;
    LanguageID LanguageId{LanguageID::None};

    std::unique_ptr<CodeParser> Parser;
    TSTree *Tree{nullptr};
    bool TextChanged{false};
};
