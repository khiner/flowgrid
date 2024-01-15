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

    inline static const LanguageDefinitions Languages{};
    const LanguageDefinition &GetLanguage() const { return Languages.Get(LanguageId); }

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

    LineChar GetCursorPosition() const { return GetCursor().End; }
    bool AnyCursorHasSelection() const;
    bool AnyCursorHasMultilineSelection() const;
    bool AllCursorsHaveSelection() const;

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

    bool Render(const char *title, bool is_parent_focused = false, const ImVec2 &size = ImVec2(), bool border = false);
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
    inline static ImVec4 U32ColorToVec4(ImU32 in) {
        static constexpr float s = 1.0f / 255.0f;
        return ImVec4(
            ((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_R_SHIFT) & 0xFF) * s
        );
    }

    inline static bool IsUTFSequence(char c) { return (c & 0xC0) == 0x80; }

    struct Cursor {
        // todo next up: We store `Coords` in addition to `LineChar` to keep the visual column at max(column, line_end) when navigating cursors up and down.
        // `Start` and `End` are the the first and second coordinate _set in an interaction_.
        // For position-ordered coordinates, use `SelectionStart()` and `SelectionEnd()`.
        LineChar Start{}, End{};

        bool operator==(const Cursor &) const = default;
        bool operator!=(const Cursor &) const = default;

        LineChar SelectionStart() const { return Start < End ? Start : End; }
        LineChar SelectionEnd() const { return Start > End ? Start : End; }
        bool HasSelection() const { return Start != End; }
        bool HasMultilineSelection() const { return SelectionStart().L != SelectionEnd().L; }
    };

    // State to be restored with undo/redo (in addition to the text).
    struct EditorState {
        std::vector<Cursor> Cursors{{{0, 0}}};
        uint LastAddedCursorIndex{0};
    };

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
        UndoRecord(const std::vector<UndoOperation> &ops, const EditorState &before, const EditorState &after)
            : Operations(ops), Before(before), After(after) {
            // for (const UndoOperation &o : Operations) assert(o.Start <= o.End);
        }
        UndoRecord(const EditorState &before) : Before(before) {}
        UndoRecord(std::vector<UndoOperation> &&ops, EditorState &&before, EditorState &&after)
            : Operations(std::move(ops)), Before(std::move(before)), After(std::move(after)) {}
        UndoRecord(EditorState &&before) : Before(std::move(before)) {}
        ~UndoRecord() = default;

        void Undo(TextEditor *);
        void Redo(TextEditor *);

        std::vector<UndoOperation> Operations{};
        EditorState Before{}, After{};
    };

    inline static const PaletteIdT DefaultPaletteId{PaletteIdT::Dark};

    void AddCursor();
    void ResetCursors();
    uint GetLastAddedCursorIndex() { return State.LastAddedCursorIndex >= State.Cursors.size() ? 0 : State.LastAddedCursorIndex; }
    Cursor &GetLastAddedCursor() { return State.Cursors[GetLastAddedCursorIndex()]; }
    Cursor &GetCursor() { return State.Cursors.back(); }
    const Cursor &GetCursor() const { return State.Cursors.back(); }
    void SetCursorPosition(LineChar position, Cursor &cursor, bool clear_selection = true);
    void EnsureCursorVisible(bool start_too = false);
    void OnCursorPositionChanged();
    void SortAndMergeCursors();

    void AddUndoOp(UndoRecord &, UndoOperationType, LineChar start, LineChar end);

    std::string GetSelectedText(const Cursor &c) const { return GetText(c.SelectionStart(), c.SelectionEnd()); }
    ImU32 GetGlyphColor(LineChar) const;

    enum class MoveDirection {
        Right = 0,
        Left = 1,
        Up = 2,
        Down = 3
    };

    static LineChar BeginLC() { return {0, 0}; }
    LineChar EndLC() const { return {uint(Lines.size() - 1), uint(Lines.back().size())}; }
    LinesIter Iter(LineChar lc, LineChar begin, LineChar end) const { return {Lines, std::move(lc), std::move(begin), std::move(end)}; }
    LinesIter Iter(LineChar lc = BeginLC()) const { return Iter(std::move(lc), BeginLC(), EndLC()); }

    LineChar LineMaxLC(uint li) const { return {li, uint(Lines[li].size())}; }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetCharColumn(Lines[lc.L], lc.C)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(Lines[coords.L], coords.C)}; }
    uint ToByteIndex(LineChar) const;

    Coords ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const;
    LineChar ScreenPosToLC(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const { return ToLineChar(ScreenPosToCoords(screen_pos, is_over_li)); }
    uint GetCharIndex(const LineT &, uint column) const;
    uint GetCharColumn(const LineT &line, uint ci) const;
    uint GetFirstVisibleCharIndex(uint li) const;
    uint GetLineMaxColumn(uint li) const;
    uint GetLineMaxColumn(uint li, uint limit) const;

    LineChar MoveLC(LineChar, MoveDirection, bool is_word_mode = false, uint line_count = 1) const;

    void MoveCharIndexAndColumn(const LineT &, uint &ci, uint &column) const;
    void MoveUp(uint amount = 1, bool select = false);
    void MoveDown(uint amount = 1, bool select = false);
    void MoveLeft(bool select = false, bool is_word_mode = false);
    void MoveRight(bool select = false, bool is_word_mode = false);
    void MoveTop(bool select = false);
    void MoveBottom(bool select = false);
    void MoveStart(bool select = false);
    void MoveEnd(bool select = false);
    void EnterChar(ImWchar, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void Delete(bool is_word_mode = false, const EditorState *editor_state = nullptr);

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

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdateViewVariables(float scroll_x, float scroll_y);
    void Render(bool is_parent_focused = false);

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

    using PaletteT = std::array<ImU32, (unsigned)PaletteIndex::Max>;
    static const PaletteT *GetPalette(PaletteIdT);
    static const PaletteT DarkPalette, MarianaPalette, LightPalette, RetroBluePalette;

    LinesT Lines{LineT{}};
    std::vector<std::vector<PaletteIndex>> PaletteIndices{std::vector<PaletteIndex>{}};
    EditorState State;

    std::vector<UndoRecord> UndoBuffer;
    uint UndoIndex{0};

    uint NumTabSpaces{4};
    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    uint LeftMargin{10};
    int LastEnsureCursorVisible{-1};
    bool LastEnsureCursorVisibleStartToo{false};
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
    PaletteT Palette;
    LanguageID LanguageId{LanguageID::None};

    std::unique_ptr<CodeParser> Parser;
    TSTree *Tree{nullptr};
    bool TextChanged{false};
};
