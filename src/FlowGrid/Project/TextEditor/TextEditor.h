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
        static Coordinates Invalid() { return {-1, -1}; }

        bool operator==(const Coordinates &o) const { return Line == o.Line && Column == o.Column; }
        bool operator!=(const Coordinates &o) const { return Line != o.Line || Column != o.Column; }
        bool operator<(const Coordinates &o) const { return Line != o.Line ? Line < o.Line : Column < o.Column; }
        bool operator>(const Coordinates &o) const { return Line != o.Line ? Line > o.Line : Column > o.Column; }
        bool operator<=(const Coordinates &o) const { return Line != o.Line ? Line < o.Line : Column <= o.Column; }
        bool operator>=(const Coordinates &o) const { return Line != o.Line ? Line > o.Line : Column >= o.Column; }

        Coordinates operator-(const Coordinates &o) { return {Line - o.Line, Column - o.Column}; }
        Coordinates operator+(const Coordinates &o) { return {Line + o.Line, Column + o.Column}; }
    };

    void SetPalette(PaletteIdT);
    void SetLanguageDefinition(LanguageDefinitionIdT);
    const char *GetLanguageDefinitionName() const;
    void SetTabSize(int);
    void SetLineSpacing(float);

    void SelectAll();
    bool AnyCursorHasSelection() const;
    bool AllCursorsHaveSelection() const;
    Coordinates GetCursorPosition(int cursor = -1) const;
    void SetCursorPosition(int line_number, int char_index);

    void Copy();
    void Cut();
    void Paste();

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo(int aSteps = 1);
    void Redo(int aSteps = 1);
    int GetUndoIndex() const;

    string GetText() const;
    int GetTotalLines() const { return (int)Lines.size(); }
    string GetClipboardText() const;
    string GetSelectedText(int cursor = -1) const;
    string GetCurrentLineText() const;

    void SetText(const string &text);
    void SetTextLines(const std::vector<string> &);

    bool Render(const char *title, bool is_parent_focused = false, const ImVec2 &size = ImVec2(), bool border = false);
    void DebugPanel();

    struct Identifier {
        Coordinates Location;
        string Declaration;
    };

    enum class PaletteIndexT {
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

    using IdentifiersT = std::unordered_map<string, Identifier>;
    using PaletteT = std::array<ImU32, (unsigned)PaletteIndexT::Max>;

    struct Glyph {
        char Char;
        PaletteIndexT ColorIndex = PaletteIndexT::Default;
        bool IsComment : 1;
        bool IsMultiLineComment : 1;
        bool IsPreprocessor : 1;

        Glyph(char character, PaletteIndexT color_index)
            : Char(character), ColorIndex(color_index), IsComment(false), IsMultiLineComment(false), IsPreprocessor(false) {}
    };

    using LineT = std::vector<Glyph>;
    using RegexListT = std::vector<std::pair<std::regex, PaletteIndexT>>;

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


    void OnCursorPositionChanged();

    void SetCursorPosition(const Coordinates &position, int cursor = -1, bool clear_selection = true);

    void InsertText(const string &, int cursor = -1);
    void InsertText(const char *, int cursor = -1);

    enum class MoveDirection {
        Right = 0,
        Left = 1,
        Up = 2,
        Down = 3
    };
    bool Move(int &line, int &char_index, bool left = false, bool lock_line = false) const;
    void MoveCoords(Coordinates &, MoveDirection, bool is_word_mode = false, int line_count = 1) const;

    void MoveUp(int amount = 1, bool select = false);
    void MoveDown(int amount = 1, bool select = false);
    void MoveLeft(bool select = false, bool is_word_mode = false);
    void MoveRight(bool select = false, bool is_word_mode = false);
    void MoveTop(bool select = false);
    void MoveBottom(bool select = false);
    void MoveHome(bool select = false);
    void MoveEnd(bool select = false);

    void SetSelection(Coordinates start, Coordinates end, int cursor = -1);
    void SetSelection(int start_line_number, int start_char_index, int end_line_number, int end_char_index, int cursor = -1);

    struct EditorState;
    void Delete(bool is_word_mode = false, const EditorState *editor_state = nullptr);

    void AddCursorForNextOccurrence(bool case_sensitive = true);

    static const PaletteT &GetMarianaPalette();
    static const PaletteT &GetDarkPalette();
    static const PaletteT &GetLightPalette();
    static const PaletteT &GetRetroBluePalette();

    static bool IsGlyphWordChar(const Glyph &glyph);

    bool ReadOnly{false};
    bool Overwrite{false};
    bool AutoIndent{true};
    bool ShowWhitespaces{true};

    float LineSpacing{1};
    float LongestLineLength{20};

    struct Cursor {
        Coordinates InteractiveStart = {0, 0};
        Coordinates InteractiveEnd = {0, 0};
        inline Coordinates GetSelectionStart() const { return InteractiveStart < InteractiveEnd ? InteractiveStart : InteractiveEnd; }
        inline Coordinates GetSelectionEnd() const { return InteractiveStart > InteractiveEnd ? InteractiveStart : InteractiveEnd; }
        inline bool HasSelection() const { return InteractiveStart != InteractiveEnd; }
    };

    struct EditorState {
        int FirstVisibleLine = 0, LastVisibleLine = 0, VisibleLineCount = 0;
        int FirstVisibleColumn = 0, LastVisibleColumn = 0, VisibleColumnCount = 0;
        float ContentWidth = 0, ContentHeight = 0;
        float ScrollX = 0, ScrollY = 0;
        bool Panning = false;
        bool IsDraggingSelection = false;
        ImVec2 LastMousePos;
        int CurrentCursor = 0;
        int LastAddedCursor = 0;
        bool CursorPositionChanged = false;
        std::vector<Cursor> Cursors = {{{0, 0}}};

        void AddCursor() {
            // Vector is never resized to smaller size.
            // `CurrentCursor` points to last available cursor in vector.
            CurrentCursor++;
            Cursors.resize(CurrentCursor + 1);
            LastAddedCursor = CurrentCursor;
        }

        int GetLastAddedCursorIndex() { return LastAddedCursor > CurrentCursor ? 0 : LastAddedCursor; }

        void SortCursorsFromTopToBottom() {
            Coordinates last_added_cursor_pos = Cursors[GetLastAddedCursorIndex()].InteractiveEnd;
            std::sort(Cursors.begin(), Cursors.begin() + (CurrentCursor + 1), [](const Cursor &a, const Cursor &b) -> bool {
                return a.GetSelectionStart() < b.GetSelectionStart();
            });
            // Update last added cursor index to be valid after sort.
            for (int c = CurrentCursor; c > -1; c--) {
                if (Cursors[c].InteractiveEnd == last_added_cursor_pos) LastAddedCursor = c;
            }
        }
    };

    void MergeCursorsIfPossible();

    std::vector<LineT> Lines;
    EditorState State;

private:
    struct LanguageDefT {
        using TokenRegexStringT = std::pair<string, PaletteIndexT>;
        using TokenizeCallbackT = bool (*)(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndexT &palette_index);

        string Name;
        std::unordered_set<string> Keywords;
        IdentifiersT Identifiers;
        IdentifiersT PreprocIdentifiers;
        string CommentStart, CommentEnd, SingleLineComment;
        char PreprocChar{'#'};

        TokenizeCallbackT Tokenize{nullptr};
        std::vector<TokenRegexStringT> TokenRegexStrings;
        bool IsCaseSensitive{true};

        static const LanguageDefT &Cpp();
        static const LanguageDefT &Hlsl();
        static const LanguageDefT &Glsl();
        static const LanguageDefT &Python();
        static const LanguageDefT &C();
        static const LanguageDefT &Sql();
        static const LanguageDefT &AngelScript();
        static const LanguageDefT &Lua();
        static const LanguageDefT &Cs();
        static const LanguageDefT &Jsn();
    };

    struct UndoRecord {
        UndoRecord() {}
        ~UndoRecord() {}

        UndoRecord(
            const std::vector<UndoOperation> &operations,
            TextEditor::EditorState &before,
            TextEditor::EditorState &after
        );

        void Undo(TextEditor *editor);
        void Redo(TextEditor *editor);

        std::vector<UndoOperation> Operations;

        EditorState Before;
        EditorState After;
    };

    static const std::unordered_map<char, char> OpenToCloseChar;
    static const std::unordered_map<char, char> CloseToOpenChar;
    inline static const PaletteIdT DefaultPaletteId = PaletteIdT::Dark;

    inline ImVec4 U32ColorToVec4(ImU32 in) {
        static const float s = 1.0f / 255.0f;
        return ImVec4(
            ((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_R_SHIFT) & 0xFF) * s
        );
    }

    inline bool IsUTFSequence(char c) const { return (c & 0xC0) == 0x80; }

    void Colorize(int from_line_number = 0, int line_count = -1);
    void ColorizeRange(int from_line_number = 0, int to_line_number = 0);
    void ColorizeInternal();
    float TextDistanceToLineStart(const Coordinates &from) const;
    void EnsureCursorVisible(int cursor = -1);
    int GetPageSize() const;
    string GetText(const Coordinates &start, const Coordinates &end) const;
    Coordinates SanitizeCoordinates(const Coordinates &) const;
    void DeleteRange(const Coordinates &start, const Coordinates &end);
    int InsertTextAt(Coordinates &at, const char *);
    void AddUndo(UndoRecord &);
    Coordinates ScreenPosToCoordinates(const ImVec2 &position, bool is_insertion_mode = false, bool *is_over_line_number = nullptr) const;
    Coordinates FindWordStart(const Coordinates &from) const;
    Coordinates FindWordEnd(const Coordinates &from) const;
    int GetCharacterIndexL(const Coordinates &coords) const;
    int GetCharacterIndexR(const Coordinates &coords) const;
    int GetCharacterColumn(int line_number, int char_index) const;
    int GetLineMaxColumn(int line_number) const;

    LineT &InsertLine(int line_number);
    void RemoveLines(int start, int end);
    void RemoveLine(int line_number, const std::unordered_set<int> *handled_cursors = nullptr);
    void RemoveCurrentLines();
    void RemoveGlyphsFromLine(int line_number, int start_char_index, int end_char_index = -1);
    void AddGlyphsToLine(int line_number, int target_index, LineT::iterator source_start, LineT::iterator source_end);
    void AddGlyphToLine(int line_number, int target_index, Glyph glyph);
    void OnLineChanged(bool before_change, int line_number, int column, int char_count, bool is_deleted);

    void ChangeCurrentLinesIndentation(bool increase);
    void MoveUpCurrentLines();
    void MoveDownCurrentLines();
    void ToggleLineComment();

    void EnterCharacter(ImWchar character, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void DeleteSelection(int cursor = -1);
    ImU32 GetGlyphColor(const Glyph &glyph) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdatePalette();
    void Render(bool is_parent_focused = false);

    bool FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out, bool case_sensitive = true);
    bool FindMatchingBracket(int line, int char_index, Coordinates &out);

    std::vector<UndoRecord> UndoBuffer;
    int UndoIndex{0};

    int TabSize{4};
    int LastEnsureVisibleCursor{-1};
    bool ScrollToTop{false};
    float TextStart{20}; // Position (in pixels) where a code line starts relative to the left of the TextEditor.
    int LeftMargin{10};
    int ColorRangeMin{0}, ColorRangeMax{0};

    PaletteIdT PaletteId;
    PaletteT Palette;
    LanguageDefinitionIdT LanguageDefinitionId;
    const LanguageDefT *LanguageDef = nullptr;
    RegexListT RegexList;

    bool ShouldCheckComments{true};
    ImVec2 CharAdvance;
    string LineBuffer;
    float LastClickTime{-1}; // In ImGui time.
};
