#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <regex>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgui.h"

#include "PaletteIndex.h"

struct LanguageDefinition;

struct TextEditor {
    TextEditor();
    ~TextEditor();

    enum class PaletteIdT {
        Dark,
        Light,
        Mariana,
        RetroBlue
    };
    enum class LanguageDefinitionIdT {
        None,
        Cpp,
        C,
        Cs,
        Python,
        Lua,
        Json,
        Sql,
        AngelScript,
        Glsl,
        Hlsl
    };

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
        bool operator==(const Coords &o) const = default;
        bool operator!=(const Coords &o) const = default;

        Coords operator-(const Coords &o) const { return {L - o.L, C - o.C}; }
        Coords operator+(const Coords &o) const { return {L + o.L, C + o.C}; }
    };

    uint GetLineCount() const { return Lines.size(); }
    Coords GetCursorPosition() const { return SanitizeCoords(State.GetCursor().End); }

    void SetPalette(PaletteIdT);
    void SetLanguageDefinition(LanguageDefinitionIdT);
    const char *GetLanguageDefinitionName() const;

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

        bool operator==(const Cursor &o) const { return Start == o.Start && End == o.End; }

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
        PaletteIndex ColorIndex{PaletteIndex::Default};
        bool IsComment : 1;
        bool IsMultiLineComment : 1;
        bool IsPreprocessor : 1;

        Glyph(char ch, PaletteIndex color_index)
            : Char(ch), ColorIndex(color_index), IsComment(false), IsMultiLineComment(false), IsPreprocessor(false) {}
    };

    using PaletteT = std::array<ImU32, (unsigned)PaletteIndex::Max>;
    using LineT = std::vector<Glyph>;

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

    static bool Equals(const std::string &, const std::span<const Glyph> &, std::size_t span_offset = 0);

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
    bool Move(uint &li, uint &ci, bool left = false, bool lock_line = false) const;
    void MoveCharIndexAndColumn(uint li, uint &ci, uint &column) const;
    Coords MoveCoords(const Coords &, MoveDirection, bool is_word_mode = false, uint line_count = 1) const;
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
    // Returns a cursor containing the start/end coords of the next occurrence of `text` after `from`, or `std::nullopt` if not found.
    std::optional<Cursor> FindNextOccurrence(const std::string &text, const Coords &from, bool case_sensitive = true);
    std::optional<Cursor> FindMatchingBrackets(const Cursor &);
    static uint FindFirstNonSpace(const LineT &);
    static bool LineStartsWith(const LineT &, const std::string &comment);
    void ChangeCurrentLinesIndentation(bool increase);
    void MoveCurrentLines(bool up);
    void ToggleLineComment();
    void RemoveCurrentLines();

    float TextDistanceToLineStart(const Coords &from, bool sanitize_coords = true) const;
    void EnsureCursorVisible(bool start_too = false);

    Coords LineMaxCoords(uint li) const { return {li, GetLineMaxColumn(li)}; }
    Coords LineCharCoords(uint li, uint ci) const { return {li, GetCharColumn(li, ci)}; }
    Coords SanitizeCoords(const Coords &) const;
    Coords ScreenPosToCoords(const ImVec2 &screen_pos, bool *is_over_li = nullptr) const;
    Coords FindWordStart(const Coords &from) const;
    Coords FindWordEnd(const Coords &from) const;
    uint GetCharIndex(const Coords &) const;
    uint GetCharColumn(uint li, uint ci) const;
    uint GetFirstVisibleCharIndex(uint line) const;
    uint GetLineMaxColumn(uint li) const;
    uint GetLineMaxColumn(uint li, uint limit) const;

    LineT &InsertLine(uint li);
    void DeleteRange(const Coords &start, const Coords &end, const Cursor *exclude_cursor = nullptr);
    void DeleteSelection(Cursor &, UndoRecord &);

    void AddOrRemoveGlyphs(uint li, uint ci, std::span<const Glyph>, bool is_add);
    void AddGlyphs(uint li, uint ci, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(li, ci, glyphs, true); }
    void RemoveGlyphs(uint li, uint ci, std::span<const Glyph> glyphs) { AddOrRemoveGlyphs(li, ci, glyphs, false); }
    void RemoveGlyphs(uint li, uint ci, uint end_ci) { RemoveGlyphs(li, ci, {Lines[li].cbegin() + ci, Lines[li].cbegin() + end_ci}); }
    void RemoveGlyphs(uint li, uint ci) { RemoveGlyphs(li, ci, {Lines[li].cbegin() + ci, Lines[li].cend()}); }
    ImU32 GetGlyphColor(const Glyph &) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdateViewVariables(float scroll_x, float scroll_y);
    void Render(bool is_parent_focused = false);

    void OnCursorPositionChanged();
    void SortAndMergeCursors();

    void AddUndo(UndoRecord &);

    void Colorize(uint from_li, uint line_count);
    void ColorizeRange(uint from_li, uint to_li);
    void ColorizeInternal();

    bool IsHorizontalScrollbarVisible() const { return CurrentSpaceWidth > ContentWidth; }
    bool IsVerticalScrollbarVisible() const { return CurrentSpaceHeight > ContentHeight; }
    uint TabSizeAtColumn(uint column) const { return TabSize - (column % TabSize); }

    static const PaletteT *GetPalette(PaletteIdT);

    static const PaletteT DarkPalette, MarianaPalette, LightPalette, RetroBluePalette;

    std::vector<LineT> Lines;
    EditorState State;

    std::vector<UndoRecord> UndoBuffer;
    uint UndoIndex{0};

    uint TabSize{4};
    int LastEnsureCursorVisible{-1};
    bool LastEnsureCursorVisibleStartToo{false};
    bool ScrollToTop{false};
    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    uint LeftMargin{10};
    ImVec2 CharAdvance;
    float LastClickTime{-1}; // In ImGui time.
    ImVec2 LastClickPos{-1, -1};
    float CurrentSpaceWidth{20}, CurrentSpaceHeight{20.0f};
    uint FirstVisibleLineI{0}, LastVisibleLineI{0}, VisibleLineCount{0};
    uint FirstVisibleColumn{0}, LastVisibleColumn{0}, VisibleColumnCount{0};
    float ContentWidth{0}, ContentHeight{0};
    float ScrollX{0}, ScrollY{0};
    bool Panning{false};
    bool IsDraggingSelection{false};
    ImVec2 LastMousePos;
    bool CursorPositionChanged{false};
    std::optional<Cursor> MatchingBrackets{};

    uint ColorRangeMin{0}, ColorRangeMax{0};
    bool ShouldCheckComments{true};
    PaletteT Palette;
    const LanguageDefinition *LanguageDef{nullptr};
    std::vector<std::pair<std::regex, PaletteIndex>> RegexList;
    std::string LineBuffer;
};
