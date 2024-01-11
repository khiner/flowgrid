#pragma once

#include <array>
#include <cassert>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgui.h"

namespace fs = std::filesystem;

struct TSLanguage;

struct TextEditor {
    // Forward-declare wrapper structs for Tree-Sitter types.
    struct CodeParser;
    struct SyntaxTree;

    TextEditor();
    ~TextEditor();

    // todo group all language data/functionality into a struct.

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
        ErrorMarker,
        ControlCharacter,
        Breakpoint,
        LineNumber,
        CurrentLineFill,
        CurrentLineFillInactive,
        CurrentLineEdge,
        Max
    };

    enum class PaletteIdT {
        Dark,
        Light,
        Mariana,
        RetroBlue
    };

    using LanguagePalette = std::unordered_map<std::string, PaletteIndex>;

    enum class LanguageDefinitionIdT {
        None,
        Cpp,
        Json,
    };

    inline static std::string GetLanguageDefinitionName(LanguageDefinitionIdT id) {
        static const std::unordered_map<LanguageDefinitionIdT, std::string> LanguageDefinitionNames{
            {LanguageDefinitionIdT::Cpp, "C++"},
            {LanguageDefinitionIdT::Json, "JSON"},
            {LanguageDefinitionIdT::None, "None"}
        };
        return LanguageDefinitionNames.at(id);
    }

    inline static LanguageDefinitionIdT GetLanguageDefinitionForFile(const fs::path &file_path) {
        static const std::unordered_map<std::string, LanguageDefinitionIdT> LanguageDefinitions{
            {".h", LanguageDefinitionIdT::Cpp},
            {".hpp", LanguageDefinitionIdT::Cpp},
            {".cpp", LanguageDefinitionIdT::Cpp},
            {".json", LanguageDefinitionIdT::Json},
        };

        const std::string extension = file_path.extension();
        if (extension.empty()) return LanguageDefinitionIdT::None;
        return LanguageDefinitions.contains(extension) ? LanguageDefinitions.at(extension) : LanguageDefinitionIdT::None;
    }

    inline static std::string GetSingleLineComment(LanguageDefinitionIdT id) {
        static const std::unordered_map<LanguageDefinitionIdT, std::string> SingleLineComments{
            {LanguageDefinitionIdT::Cpp, "//"},
            {LanguageDefinitionIdT::Json, "//"},
        };
        return SingleLineComments.at(id);
    }
    LanguageDefinitionIdT GetLanguageDefinitionId() const { return LanguageDefId; }
    static TSLanguage *GetLanguage(LanguageDefinitionIdT);
    static const LanguagePalette &GetLanguagePalette(LanguageDefinitionIdT);

    static LanguagePalette CreateCppPalette();
    static LanguagePalette CreateJsonPalette();

    // Represents a character coordinate from the user's point of view,
    // i. e. consider a uniform grid (assuming fixed-width font) on the screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..TabSize] u32 empty spaces, depending on how many space is necessary to reach the next tab stop.
    // For example, `Coords{1, 5}` represents the character 'B' in the line "\tABC", when TabSize = 4, since it is rendered as "    ABC" on the screen.
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

        bool operator==(const LineChar &) const = default;
        bool operator!=(const LineChar &) const = default;
    };

    uint GetLineCount() const { return Lines.size(); }
    Coords GetCursorPosition() const { return SanitizeCoords(State.GetCursor().End); }

    void SetPalette(PaletteIdT);
    void SetLanguageDefinition(LanguageDefinitionIdT);
    std::string GetLanguageDefinitionName() const { return GetLanguageDefinitionName(LanguageDefId); }

    void SetTabSize(uint);
    void SetLineSpacing(float);

    void SelectAll();
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

    void SetText(const std::string &text);
    std::string GetText(const Coords &start, const Coords &end) const;
    std::string GetText() const { return Lines.empty() ? "" : GetText({}, LineMaxCoords(Lines.size() - 1)); }

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
        // These coordinates reflect the order of interaction.
        // For ordered coordinates, use `SelectionStart()` and `SelectionEnd()`.
        Coords Start{}, End{};

        bool operator==(const Cursor &) const = default;
        bool operator!=(const Cursor &) const = default;

        Coords SelectionStart() const { return Start < End ? Start : End; }
        Coords SelectionEnd() const { return Start > End ? Start : End; }
        bool HasSelection() const { return Start != End; }
        bool HasMultilineSelection() const { return SelectionStart().L != SelectionEnd().L; }
    };

    // State to be restored with undo/redo.
    struct EditorState {
        uint LastAddedCursorIndex{0};
        std::vector<Cursor> Cursors{{{0, 0}}};

        void AddCursor();
        void ResetCursors();
        uint GetLastAddedCursorIndex() { return LastAddedCursorIndex >= Cursors.size() ? 0 : LastAddedCursorIndex; }
        Cursor &GetLastAddedCursor() { return Cursors[GetLastAddedCursorIndex()]; }
        Cursor &GetCursor(uint c) { return Cursors[c]; }
        const Cursor &GetCursor(uint c) const { return Cursors[c]; }
        Cursor &GetCursor() { return Cursors.back(); }
        const Cursor &GetCursor() const { return Cursors.back(); }
    };

    struct Glyph {
        char Char;
        PaletteIndex PaletteIndex;

        Glyph(char ch, TextEditor::PaletteIndex palette_index = PaletteIndex::Default) : Char(ch), PaletteIndex(palette_index) {}

        bool operator==(char ch) const { return Char == ch; }
        operator char() const { return Char; }
    };

    using PaletteT = std::array<ImU32, (unsigned)PaletteIndex::Max>;
    using LineT = std::vector<Glyph>;

    struct LineCharIter {
        LineCharIter(const std::vector<LineT> &lines, LineChar lc = {0, 0})
            : Lines(lines), LC(std::move(lc)), Begin({0, 0}), End({uint(Lines.size() - 1), uint(Lines.back().size())}) {}
        LineCharIter(const LineCharIter &) = default; // Needed since we have an assignment operator.

        LineCharIter &operator=(const LineCharIter &o) {
            LC = o.LC;
            Begin = o.Begin;
            End = o.End;
            return *this;
        }

        operator char() const { return Lines[LC.L][LC.C]; }

        LineChar operator*() const { return LC; }

        LineCharIter &operator++() {
            MoveRight();
            return *this;
        }
        LineCharIter &operator--() {
            MoveLeft();
            return *this;
        }

        bool operator==(const LineCharIter &o) const { return LC == o.LC; }
        bool operator!=(const LineCharIter &o) const { return LC != o.LC; }

        LineCharIter begin() const { return {Lines, Begin}; }
        LineCharIter end() const { return {Lines, End}; }

    private:
        const std::vector<LineT> &Lines;
        LineChar LC;
        LineChar Begin, End;

        void MoveRight();
        void MoveLeft();
    };

    enum class UndoOperationType {
        Add,
        Delete,
    };
    struct UndoOperation {
        std::string Text;
        Coords Start, End;
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

    void AddUndoOp(UndoRecord &, UndoOperationType, const Coords &start, const Coords &end);

    void SetCursorPosition(const Coords &position, Cursor &cursor, bool clear_selection = true);

    std::string GetSelectedText(const Cursor &c) const { return GetText(c.SelectionStart(), c.SelectionEnd()); }
    Coords InsertTextAt(const Coords &, const std::string &); // Returns insertion end.
    void InsertTextAtCursor(const std::string &, Cursor &);

    enum class MoveDirection {
        Right = 0,
        Left = 1,
        Up = 2,
        Down = 3
    };

    Coords MoveCoords(const Coords &, MoveDirection, bool is_word_mode = false, uint line_count = 1) const;

    void MoveCharIndexAndColumn(uint li, uint &ci, uint &column) const;
    void MoveUp(uint amount = 1, bool select = false);
    void MoveDown(uint amount = 1, bool select = false);
    void MoveLeft(bool select = false, bool is_word_mode = false);
    void MoveRight(bool select = false, bool is_word_mode = false);
    void MoveTop(bool select = false);
    void MoveBottom(bool select = false);
    void MoveHome(bool select = false);
    void MoveEnd(bool select = false);
    void EnterChar(ImWchar, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void Delete(bool is_word_mode = false, const EditorState *editor_state = nullptr);

    void SetSelection(Coords start, Coords end, Cursor &);

    void AddCursorForNextOccurrence(bool case_sensitive = true);
    // Returns a cursor containing the start/end coords of the next occurrence of `text` at or after `start`, or `std::nullopt` if not found.
    std::optional<Cursor> FindNextOccurrence(const std::string &text, const Coords &start, bool case_sensitive = true);
    std::optional<Cursor> FindMatchingBrackets(const Cursor &);
    uint NumStartingSpaceColumns(uint li) const;

    void ChangeCurrentLinesIndentation(bool increase);
    void MoveCurrentLines(bool up);
    void ToggleLineComment();
    void RemoveCurrentLines();

    float TextDistanceToLineStart(const Coords &from, bool sanitize_coords = true) const;
    void EnsureCursorVisible(bool start_too = false);

    Coords LineMaxCoords(uint li) const { return {li, GetLineMaxColumn(li)}; }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetCharColumn(lc)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(coords)}; }
    Coords SanitizeCoords(const Coords &) const;
    Coords ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const;
    Coords FindWordBoundary(const Coords &from, bool is_start = false) const;
    uint GetCharIndex(const Coords &) const;
    uint GetCharColumn(LineChar) const;
    uint GetFirstVisibleCharIndex(uint li) const;
    uint GetLineMaxColumn(uint li) const;
    uint GetLineMaxColumn(uint li, uint limit) const;

    LineT &InsertLine(uint li);
    void DeleteRange(const Coords &start, const Coords &end, const Cursor *exclude_cursor = nullptr);
    void DeleteSelection(Cursor &, UndoRecord &);

    void AddOrRemoveGlyphs(LineChar lc, std::span<const Glyph>, bool is_add);
    void AddGlyphs(LineChar lc, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(std::move(lc), glyphs, true); }
    void RemoveGlyphs(LineChar lc, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(std::move(lc), glyphs, false); }
    void RemoveGlyphs(LineChar lc, uint end_ci) { RemoveGlyphs(lc, {Lines[lc.L].cbegin() + lc.C, Lines[lc.L].cbegin() + end_ci}); }
    void RemoveGlyphs(LineChar lc) { RemoveGlyphs(lc, {Lines[lc.L].cbegin() + lc.C, Lines[lc.L].cend()}); }
    ImU32 GetGlyphColor(const Glyph &) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdateViewVariables(float scroll_x, float scroll_y);
    void Render(bool is_parent_focused = false);

    void OnTextChanged(Cursor &&range);
    void OnTextChanged(); // All text changed.
    void OnTextChanged(uint li); // Line changed.
    void OnCursorPositionChanged();
    void SortAndMergeCursors();

    void AddUndo(UndoRecord &);

    bool IsHorizontalScrollbarVisible() const { return CurrentSpaceWidth > ContentWidth; }
    bool IsVerticalScrollbarVisible() const { return CurrentSpaceHeight > ContentHeight; }
    uint TabSizeAtColumn(uint column) const { return TabSize - (column % TabSize); }

    static const PaletteT *GetPalette(PaletteIdT);

    void Highlight();

    static const PaletteT DarkPalette, MarianaPalette, LightPalette, RetroBluePalette;

    std::vector<LineT> Lines;
    std::vector<uint> LineStarts; // Stores the starting byte of each line.
    EditorState State;

    std::vector<UndoRecord> UndoBuffer;
    uint UndoIndex{0};

    uint TabSize{4};
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
    std::optional<Cursor> ChangedRange{};
    PaletteT Palette;
    LanguageDefinitionIdT LanguageDefId{LanguageDefinitionIdT::None};

    std::unique_ptr<CodeParser> Parser;
    std::unique_ptr<SyntaxTree> Tree;
};
