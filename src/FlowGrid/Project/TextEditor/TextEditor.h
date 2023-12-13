#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgui.h"

using std::string;

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

    inline int GetLineCount() const { return Lines.size(); }
    void SetPalette(PaletteIdT);
    void SetLanguageDefinition(LanguageDefinitionIdT);
    const char *GetLanguageDefinitionName() const;

    void SetTabSize(int);
    void SetLineSpacing(float);

    void SelectAll();
    bool AnyCursorHasSelection() const;
    bool AllCursorsHaveSelection() const;
    void ClearExtraCursors();
    void ClearSelections();

    void SetCursorPosition(int line_number, int char_index);
    inline std::pair<int, int> GetCursorLineColumn() const {
        const auto coords = GetCursorPosition();
        return {coords.Line, coords.Column};
    }

    void Copy();
    void Cut();
    void Paste();
    void Undo(int steps = 1);
    void Redo(int steps = 1);
    bool CanUndo() const { return !ReadOnly && UndoIndex > 0; }
    bool CanRedo() const { return !ReadOnly && UndoIndex < (int)UndoBuffer.size(); }

    void SetText(const string &text);
    string GetText() const;

    bool Render(const char *title, bool is_parent_focused = false, const ImVec2 &size = ImVec2(), bool border = false);
    void DebugPanel();

    bool ReadOnly{false};
    bool Overwrite{false};
    bool AutoIndent{true};
    bool ShowWhitespaces{true};
    bool ShowLineNumbers{true};
    bool ShortTabs{true};
    float LineSpacing{1};

private:
    inline static ImVec4 U32ColorToVec4(ImU32 in) {
        static const float s = 1.0f / 255.0f;
        return ImVec4(
            ((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_R_SHIFT) & 0xFF) * s
        );
    }
    inline int TabSizeAtColumn(int column) const { return TabSize - (column % TabSize); }

    inline static bool IsUTFSequence(char c) { return (c & 0xC0) == 0x80; }

    enum class PaletteIndex {
        Default,
        Keyword,
        Number,
        String,
        CharLiteral,
        Punctuation,
        Preprocessor,
        Identifier,
        KnownIdentifier,
        PreprocIdentifier,
        Comment,
        MultiLineComment,
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

    // Represents a character coordinate from the user's point of view,
    // i. e. consider an uniform grid (assuming fixed-width font) on the
    // screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..TabSize] u32 empty spaces, depending on
    // how many space is necessary to reach the next tab stop.
    // For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when TabSize = 4,
    // because it is rendered as "    ABC" on the screen.
    struct Coordinates {
        int Line, Column;
        Coordinates() : Line(0), Column(0) {}
        Coordinates(int line_number, int column) : Line(line_number), Column(column) {
            assert(line_number >= 0);
            assert(column >= 0);
        }
        inline static Coordinates Invalid() { return {-1, -1}; }

        bool operator==(const Coordinates &o) const { return Line == o.Line && Column == o.Column; }
        bool operator!=(const Coordinates &o) const { return Line != o.Line || Column != o.Column; }
        bool operator<(const Coordinates &o) const { return Line != o.Line ? Line < o.Line : Column < o.Column; }
        bool operator>(const Coordinates &o) const { return Line != o.Line ? Line > o.Line : Column > o.Column; }
        bool operator<=(const Coordinates &o) const { return Line != o.Line ? Line < o.Line : Column <= o.Column; }
        bool operator>=(const Coordinates &o) const { return Line != o.Line ? Line > o.Line : Column >= o.Column; }

        Coordinates operator-(const Coordinates &o) { return {Line - o.Line, Column - o.Column}; }
        Coordinates operator+(const Coordinates &o) { return {Line + o.Line, Column + o.Column}; }
    };

    struct Cursor {
        Coordinates InteractiveStart = {0, 0};
        Coordinates InteractiveEnd = {0, 0};
        inline Coordinates GetSelectionStart() const { return InteractiveStart < InteractiveEnd ? InteractiveStart : InteractiveEnd; }
        inline Coordinates GetSelectionEnd() const { return InteractiveStart > InteractiveEnd ? InteractiveStart : InteractiveEnd; }
        inline bool HasSelection() const { return InteractiveStart != InteractiveEnd; }
    };

    // State to be restored with undo/redo.
    struct EditorState {
        int CurrentCursor = 0;
        int LastAddedCursor = 0;
        std::vector<Cursor> Cursors = {{{0, 0}}};

        void AddCursor();
        int GetLastAddedCursorIndex();
        void SortCursorsFromTopToBottom();
    };

    struct Glyph {
        char Char;
        PaletteIndex ColorIndex = PaletteIndex::Default;
        bool IsComment : 1;
        bool IsMultiLineComment : 1;
        bool IsPreprocessor : 1;

        Glyph(char character, PaletteIndex color_index)
            : Char(character), ColorIndex(color_index), IsComment(false), IsMultiLineComment(false), IsPreprocessor(false) {}
    };

    using PaletteT = std::array<ImU32, (unsigned)PaletteIndex::Max>;

    using LineT = std::vector<Glyph>;

    struct LanguageDefinition {
        struct Identifier {
            Coordinates Location;
            string Declaration;
        };

        using TokenRegexStringT = std::pair<string, PaletteIndex>;
        using TokenizeCallbackT = bool (*)(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndex &palette_index);

        string Name;
        std::unordered_set<string> Keywords;
        std::unordered_map<string, Identifier> Identifiers;
        std::unordered_map<string, Identifier> PreprocIdentifiers;
        string CommentStart, CommentEnd, SingleLineComment;
        char PreprocChar{'#'};

        TokenizeCallbackT Tokenize{nullptr};
        std::vector<TokenRegexStringT> TokenRegexStrings;
        bool IsCaseSensitive{true};

        static const LanguageDefinition &Cpp();
        static const LanguageDefinition &Hlsl();
        static const LanguageDefinition &Glsl();
        static const LanguageDefinition &Python();
        static const LanguageDefinition &C();
        static const LanguageDefinition &Sql();
        static const LanguageDefinition &AngelScript();
        static const LanguageDefinition &Lua();
        static const LanguageDefinition &Cs();
        static const LanguageDefinition &Jsn();
    };

    enum class UndoOperationType {
        Add,
        Delete,
    };
    struct UndoOperation {
        string Text;
        TextEditor::Coordinates Start;
        TextEditor::Coordinates End;
        UndoOperationType Type;
    };

    struct UndoRecord {
        UndoRecord() {}
        ~UndoRecord() {}

        UndoRecord(
            const std::vector<UndoOperation> &operations,
            TextEditor::EditorState &before,
            TextEditor::EditorState &after
        );

        void Undo(TextEditor *);
        void Redo(TextEditor *);

        std::vector<UndoOperation> Operations;

        EditorState Before, After;
    };

    inline static const std::unordered_map<char, char> OpenToCloseChar = {
        {'{', '}'},
        {'(', ')'},
        {'[', ']'}
    };
    inline static const std::unordered_map<char, char> CloseToOpenChar = {
        {'}', '{'},
        {')', '('},
        {']', '['}
    };

    inline static const PaletteIdT DefaultPaletteId = PaletteIdT::Dark;

    string GetText(const Coordinates &start, const Coordinates &end) const;
    string GetClipboardText() const;
    string GetSelectedText(int cursor = -1) const;

    void SetCursorPosition(const Coordinates &position, int cursor = -1, bool clear_selection = true);

    int InsertTextAt(Coordinates &at, const char *);
    void InsertTextAtCursor(const string &, int cursor = -1);
    void InsertTextAtCursor(const char *, int cursor = -1);

    enum class MoveDirection {
        Right = 0,
        Left = 1,
        Up = 2,
        Down = 3
    };
    bool Move(int &line, int &char_index, bool left = false, bool lock_line = false) const;
    void MoveCharIndexAndColumn(int line, int &char_index, int &column) const;
    void MoveCoords(Coordinates &, MoveDirection, bool is_word_mode = false, int line_count = 1) const;
    void MoveUp(int amount = 1, bool select = false);
    void MoveDown(int amount = 1, bool select = false);
    void MoveLeft(bool select = false, bool is_word_mode = false);
    void MoveRight(bool select = false, bool is_word_mode = false);
    void MoveTop(bool select = false);
    void MoveBottom(bool select = false);
    void MoveHome(bool select = false);
    void MoveEnd(bool select = false);
    void EnterCharacter(ImWchar character, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void Delete(bool is_word_mode = false, const EditorState *editor_state = nullptr);

    void SetSelection(int start_line_number, int start_char_index, int end_line_number, int end_char_index, int cursor = -1);
    void SetSelection(Coordinates start, Coordinates end, int cursor = -1);

    void AddCursorForNextOccurrence(bool case_sensitive = true);
    bool FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out, bool case_sensitive = true);
    bool FindMatchingBracket(int line, int char_index, Coordinates &out);
    void ChangeCurrentLinesIndentation(bool increase);
    void MoveUpCurrentLines();
    void MoveDownCurrentLines();
    void ToggleLineComment();
    void RemoveCurrentLines();

    float TextDistanceToLineStart(const Coordinates &from, bool sanitize_coords = true) const;
    void EnsureCursorVisible(int cursor = -1);

    Coordinates SanitizeCoordinates(const Coordinates &) const;
    Coordinates GetCursorPosition(int cursor = -1) const;
    Coordinates ScreenPosToCoordinates(const ImVec2 &position, bool is_insertion_mode = false, bool *is_over_line_number = nullptr) const;
    Coordinates FindWordStart(const Coordinates &from) const;
    Coordinates FindWordEnd(const Coordinates &from) const;
    int GetCharacterIndexL(const Coordinates &coords) const;
    int GetCharacterIndexR(const Coordinates &coords) const;
    int GetCharacterColumn(int line_number, int char_index) const;
    int GetFirstVisibleCharacterIndex(int line) const;
    int GetLineMaxColumn(int line, int limit = -1) const;

    LineT &InsertLine(int line_number);
    void RemoveLine(int line_number, const std::unordered_set<int> *handled_cursors = nullptr);
    void RemoveLines(int start, int end);
    void DeleteRange(const Coordinates &start, const Coordinates &end);
    void DeleteSelection(int cursor = -1);

    void RemoveGlyphsFromLine(int line_number, int start_char_index, int end_char_index = -1);
    void AddGlyphsToLine(int line_number, int target_index, LineT::iterator source_start, LineT::iterator source_end);
    void AddGlyphToLine(int line_number, int target_index, Glyph glyph);
    ImU32 GetGlyphColor(const Glyph &glyph) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void Render(bool is_parent_focused = false);

    void OnCursorPositionChanged();
    void OnLineChanged(bool before_change, int line_number, int column, int char_count, bool is_deleted);
    void MergeCursorsIfPossible();

    void AddUndo(UndoRecord &);

    void Colorize(int from_line_number = 0, int line_count = -1);
    void ColorizeRange(int from_line_number = 0, int to_line_number = 0);
    void ColorizeInternal();

    static const PaletteT &GetDarkPalette();
    static const PaletteT &GetMarianaPalette();
    static const PaletteT &GetLightPalette();
    static const PaletteT &GetRetroBluePalette();

    std::vector<LineT> Lines;
    EditorState State;

    std::vector<UndoRecord> UndoBuffer;
    int UndoIndex{0};

    int TabSize{4};
    int LastEnsureVisibleCursor{-1};
    bool ScrollToTop{false};
    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    int LeftMargin{10};
    ImVec2 CharAdvance;
    float LastClickTime{-1}; // In ImGui time.
    float CurrentSpaceWidth{20}; // Longest line length in the most recent render.
    int FirstVisibleLine{0}, LastVisibleLine{0}, VisibleLineCount{0};
    int FirstVisibleColumn{0}, LastVisibleColumn{0}, VisibleColumnCount{0};
    float ContentWidth{0}, ContentHeight{0};
    float ScrollX{0}, ScrollY{0};
    bool Panning{false};
    bool IsDraggingSelection{false};
    ImVec2 LastMousePos;
    bool CursorPositionChanged{false};
    bool CursorOnBracket{false};
    Coordinates MatchingBracketCoords;

    int ColorRangeMin{0}, ColorRangeMax{0};
    bool ShouldCheckComments{true};
    PaletteIdT PaletteId;
    PaletteT Palette;
    LanguageDefinitionIdT LanguageDefinitionId;
    const LanguageDefinition *LanguageDef = nullptr;
    std::vector<std::pair<std::regex, PaletteIndex>> RegexList;
    string LineBuffer;
};
