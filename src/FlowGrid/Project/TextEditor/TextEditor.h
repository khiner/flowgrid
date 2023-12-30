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
    // For example, coordinate (1, 5) represents the character 'B' in the line "\tABC", when TabSize = 4, since it is rendered as "    ABC" on the screen.
    struct Coordinates {
        uint L{0}, C{0}; // Line, Column

        auto operator<=>(const Coordinates &o) const {
            if (auto cmp = L <=> o.L; cmp != 0) return cmp;
            return C <=> o.C;
        }
        bool operator==(const Coordinates &o) const = default;
        bool operator!=(const Coordinates &o) const = default;

        Coordinates operator-(const Coordinates &o) const { return {L - o.L, C - o.C}; }
        Coordinates operator+(const Coordinates &o) const { return {L + o.L, C + o.C}; }
    };

    uint GetLineCount() const { return Lines.size(); }
    void SetPalette(PaletteIdT);
    void SetLanguageDefinition(LanguageDefinitionIdT);
    const char *GetLanguageDefinitionName() const;

    void SetTabSize(uint);
    void SetLineSpacing(float);

    void SelectAll();
    bool AnyCursorHasSelection() const;
    bool AnyCursorHasMultilineSelection() const;
    bool AllCursorsHaveSelection() const;

    Coordinates GetCursorPosition() const;

    void Copy();
    void Cut();
    void Paste();
    void Undo(uint steps = 1);
    void Redo(uint steps = 1);
    bool CanUndo() const { return !ReadOnly && UndoIndex > 0; }
    bool CanRedo() const { return !ReadOnly && UndoIndex < uint(UndoBuffer.size()); }

    void SetText(const std::string &text);
    std::string GetText() const;

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
        Coordinates Start{}, End{};

        bool operator==(const Cursor &o) const { return Start == o.Start && End == o.End; }

        Coordinates SelectionStart() const { return Start < End ? Start : End; }
        Coordinates SelectionEnd() const { return Start > End ? Start : End; }
        bool HasSelection() const { return Start != End; }
        bool HasMultilineSelection() const { return SelectionStart().L != SelectionEnd().L; }
    };

    // State to be restored with undo/redo.
    struct EditorState {
        uint LastAddedCursorIndex{0};
        std::vector<Cursor> Cursors{{{0, 0}}};

        void AddCursor();
        void ResetCursors();
        uint GetLastAddedCursorIndex();
        Cursor &GetCursor(uint c);
        Cursor &GetCursor();
        const Cursor &GetCursor(uint c) const;
        const Cursor &GetCursor() const;
        Cursor &GetLastAddedCursor();
        void SortCursors();
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
        Coordinates Start, End;
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

    void AddUndoOp(UndoRecord &, UndoOperationType, const Coordinates &start, const Coordinates &end);

    std::string GetText(const Coordinates &start, const Coordinates &end) const;
    std::string GetSelectedText(const Cursor &) const;

    void SetCursorPosition(const Coordinates &position, Cursor &cursor, bool clear_selection = true);

    Coordinates InsertTextAt(const Coordinates &, const std::string &); // Returns insertion end.
    void InsertTextAtCursor(const std::string &, Cursor &);

    enum class MoveDirection {
        Right = 0,
        Left = 1,
        Up = 2,
        Down = 3
    };
    bool Move(uint &line, uint &ci, bool left = false, bool lock_line = false) const;
    void MoveCharIndexAndColumn(uint line, uint &ci, uint &column) const;
    void MoveCoords(Coordinates &, MoveDirection, bool is_word_mode = false, uint line_count = 1) const;
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

    void SetSelection(Coordinates start, Coordinates end, Cursor &);

    void AddCursorForNextOccurrence(bool case_sensitive = true);
    // Returns a cursor containing the start/end coords of the next occurrence of `text` after `from`, or `std::nullopt` if not found.
    std::optional<Cursor> FindNextOccurrence(const std::string &text, const Coordinates &from, bool case_sensitive = true);
    bool FindMatchingBracket(uint line, uint ci, Coordinates &out);
    void ChangeCurrentLinesIndentation(bool increase);
    void MoveCurrentLines(bool up);
    void ToggleLineComment();
    void RemoveCurrentLines();

    float TextDistanceToLineStart(const Coordinates &from, bool sanitize_coords = true) const;
    void EnsureCursorVisible(bool start_too = false);

    Coordinates SanitizeCoordinates(const Coordinates &) const;
    Coordinates ScreenPosToCoordinates(const ImVec2 &position, bool is_insertion_mode = false, bool *is_over_li = nullptr) const;
    Coordinates FindWordStart(const Coordinates &from) const;
    Coordinates FindWordEnd(const Coordinates &from) const;
    uint GetCharIndexL(const Coordinates &) const;
    uint GetCharIndexR(const Coordinates &) const;
    uint GetCharColumn(uint li, uint ci) const;
    uint GetFirstVisibleCharIndex(uint line) const;
    uint GetLineMaxColumn(uint line, int limit = -1) const;

    LineT &InsertLine(uint li);
    void DeleteRange(const Coordinates &start, const Coordinates &end, const Cursor *exclude_cursor = nullptr);
    void DeleteSelection(Cursor &, UndoRecord &);

    void RemoveGlyphsFromLine(uint li, uint start_ci, int end_ci = -1);
    void AddGlyphsToLine(uint li, uint ci, std::span<const Glyph>);
    ImU32 GetGlyphColor(const Glyph &glyph) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdateViewVariables(float scroll_x, float scroll_y);
    void Render(bool is_parent_focused = false);

    void OnCursorPositionChanged();
    std::unordered_map<uint, uint> BeforeLineChanged(uint li, uint column, uint char_count, bool is_deleted);
    void AfterLineChanged(uint li, std::unordered_map<uint, uint> &&cursor_indices);
    void SortAndMergeCursors();

    void AddUndo(UndoRecord &);

    void Colorize(uint from_li, uint line_count);
    void ColorizeRange(uint from_li, uint to_li);
    void ColorizeInternal();

    bool IsHorizontalScrollbarVisible() const { return CurrentSpaceWidth > ContentWidth; }
    bool IsVerticalScrollbarVisible() const { return CurrentSpaceHeight > ContentHeight; }
    uint TabSizeAtColumn(uint column) const { return TabSize - (column % TabSize); }

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
    bool CursorOnBracket{false};
    Coordinates MatchingBracketCoords;

    uint ColorRangeMin{0}, ColorRangeMax{0};
    bool ShouldCheckComments{true};
    PaletteIdT PaletteId;
    PaletteT Palette;
    LanguageDefinitionIdT LanguageDefinitionId;
    const LanguageDefinition *LanguageDef{nullptr};
    std::vector<std::pair<std::regex, PaletteIndex>> RegexList;
    std::string LineBuffer;
};
