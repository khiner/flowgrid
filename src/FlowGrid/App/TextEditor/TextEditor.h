#pragma once

#include "imgui.h"
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;

struct IMGUI_API TextEditor {
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

    enum class SelectionModeT {
        Normal,
        Word,
        Line
    };

    struct Breakpoint {
        int Line;
        bool Enabled;

        Breakpoint() : Line(-1), Enabled(false) {}
    };

    // Represents a character coordinate from the user's point of view,
    // i. e. consider an uniform grid (assuming fixed-width font) on the
    // screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..TabSize] count empty spaces, depending on
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
    };

    struct Identifier {
        Coordinates Location;
        string Declaration;
    };

    using IdentifiersT = std::unordered_map<string, Identifier>;
    using KeywordsT = std::unordered_set<string>;
    using ErrorMarkersT = std::map<int, string>;
    using BreakpointsT = std::unordered_set<int>;
    using PaletteT = std::array<ImU32, (unsigned)PaletteIndexT::Max>;
    using CharT = uint8_t;

    struct Glyph {
        CharT Char;
        PaletteIndexT ColorIndex = PaletteIndexT::Default;
        bool IsComment : 1;
        bool IsMultiLineComment : 1;
        bool IsPreprocessor : 1;

        Glyph(CharT character, PaletteIndexT color_index)
            : Char(character), ColorIndex(color_index), IsComment(false), IsMultiLineComment(false), IsPreprocessor(false) {}
    };

    using LineT = std::vector<Glyph>;
    using LinesT = std::vector<LineT>;

    struct LanguageDefT {
        using TokenRegexStringT = std::pair<string, PaletteIndexT>;
        using TokenRegexStringsT = std::vector<TokenRegexStringT>;
        using TokenizeCallbackT = bool (*)(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndexT &palette_index);

        string Name;
        KeywordsT Keywords;
        IdentifiersT Identifiers;
        IdentifiersT PreprocIdentifiers;
        string CommentStart, CommentEnd, SingleLineComment;
        char PreprocChar;
        bool AudioIndentation;

        TokenizeCallbackT Tokenize;
        TokenRegexStringsT TokenRegexStrings;

        bool IsCaseSensitive;

        LanguageDefT() : PreprocChar('#'), AudioIndentation(true), Tokenize(nullptr), IsCaseSensitive(true) {}

        static const LanguageDefT &CPlusPlus();
        static const LanguageDefT &HLSL();
        static const LanguageDefT &GLSL();
        static const LanguageDefT &Python();
        static const LanguageDefT &C();
        static const LanguageDefT &SQL();
        static const LanguageDefT &AngelScript();
        static const LanguageDefT &Lua();
        static const LanguageDefT &CSharp();
        static const LanguageDefT &Jsn();
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

    TextEditor();
    ~TextEditor();

    void SetLanguageDefinition(const LanguageDefT &);
    const char *GetLanguageDefinitionName() const;

    const PaletteT &GetPalette() const { return PalletteBase; }
    void SetPalette(const PaletteT &);

    bool Render(const char *title, bool is_parent_focused = false, const ImVec2 &size = ImVec2(), bool border = false);
    void SetText(const string &text);
    string GetText() const;

    void SetTextLines(const std::vector<string> &);
    std::vector<string> GetTextLines() const;

    string GetClipboardText() const;
    string GetSelectedText(int cursor = -1) const;
    string GetCurrentLineText() const;

    int GetTotalLines() const { return (int)Lines.size(); }

    void OnCursorPositionChanged();

    Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
    void SetCursorPosition(const Coordinates &position, int cursor = -1);
    void SetCursorPosition(int line_number, int characterIndex, int cursor = -1);

    inline void OnLineDeleted(int line_number, const std::unordered_set<int> *handled_cursors = nullptr) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.Line >= line_number) {
                if (handled_cursors == nullptr || handled_cursors->find(c) == handled_cursors->end()) // move up if has not been handled already
                    SetCursorPosition({EditorState.Cursors[c].CursorPosition.Line - 1, EditorState.Cursors[c].CursorPosition.Column}, c);
            }
        }
    }
    inline void OnLinesDeleted(int first_line_number, int last_line_number) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.Line >= first_line_number)
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.Line - (last_line_number - first_line_number), EditorState.Cursors[c].CursorPosition.Column}, c);
        }
    }
    inline void OnLineAdded(int line_number) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.Line >= line_number)
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.Line + 1, EditorState.Cursors[c].CursorPosition.Column}, c);
        }
    }

    inline ImVec4 U32ColorToVec4(ImU32 in) {
        static const float s = 1.0f / 255.0f;
        return ImVec4(
            ((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_R_SHIFT) & 0xFF) * s
        );
    }

    void SetTabSize(int);
    inline int GetTabSize() const { return TabSize; }

    void InsertText(const string &, int cursor = -1);
    void InsertText(const char *, int cursor = -1);

    void MoveUp(int amount = 1, bool select = false);
    void MoveDown(int amount = 1, bool select = false);
    void MoveLeft(int amount = 1, bool select = false, bool is_word_mode = false);
    void MoveRight(int amount = 1, bool select = false, bool is_word_mode = false);
    void MoveTop(bool select = false);
    void MoveBottom(bool select = false);
    void MoveHome(bool select = false);
    void MoveEnd(bool select = false);

    void SetSelectionStart(const Coordinates &position, int cursor = -1);
    void SetSelectionEnd(const Coordinates &position, int cursor = -1);
    void SetSelection(const Coordinates &start, const Coordinates &end, SelectionModeT mode = SelectionModeT::Normal, int cursor = -1, bool is_spawning_new_cursor = false);
    void SetSelection(int start_line_number, int start_char_index, int end_line_number, int end_char_indexIndex, SelectionModeT mode = SelectionModeT::Normal, int cursor = -1, bool is_spawning_new_cursor = false);
    void SelectWordUnderCursor();
    void SelectAll();
    bool HasSelection() const;

    void Copy();
    void Cut();
    void Paste();
    void Delete(bool is_word_mode = false);

    int GetUndoIndex() const;
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo(int aSteps = 1);
    void Redo(int aSteps = 1);

    void ClearExtrcursors();
    void ClearSelections();
    void SelectNextOccurrenceOf(const char *text, int text_size, int cursor = -1);
    void AddCursorForNextOccurrence();

    static const PaletteT &GetMarianaPalette();
    static const PaletteT &GetDarkPalette();
    static const PaletteT &GetLightPalette();
    static const PaletteT &GetRetroBluePalette();

    static bool IsGlyphWordChar(const Glyph &glyph);

    void DebugPanel();
    void UnitTests();

    bool ReadOnly;
    bool Overwrite;
    bool TextChanged;
    bool ColorizerEnabled;
    bool ShouldHandleKeyboardInputs;
    bool ShouldHandleMouseInputs;
    bool IgnoreImGuiChild;
    bool ShowWhitespaces;
    bool ShowShortTabGlyphs;

    float LineSpacing;

    using RegexListT = std::vector<std::pair<std::regex, PaletteIndexT>>;

    struct Cursor {
        Coordinates CursorPosition = {0, 0};
        Coordinates SelectionStart = {0, 0};
        Coordinates SelectionEnd = {0, 0};
        Coordinates InteractiveStart = {0, 0};
        Coordinates InteractiveEnd = {0, 0};
        bool CursorPositionChanged = false;
    };

    struct EditorStateT {
        int CurrentCursor = 0;
        int LastAddedCursor = 0;
        std::vector<Cursor> Cursors = {{{0, 0}}};
        void AddCursor() {
            // vector is never resized to smaller size, CurrentCursor points to last available cursor in vector
            CurrentCursor++;
            Cursors.resize(CurrentCursor + 1);
            LastAddedCursor = CurrentCursor;
        }
        int GetLastAddedCursorIndex() {
            return LastAddedCursor > CurrentCursor ? 0 : LastAddedCursor;
        }
        void SortCursorsFromTopToBottom() {
            Coordinates lastAddedCursorPos = Cursors[GetLastAddedCursorIndex()].CursorPosition;
            std::sort(Cursors.begin(), Cursors.begin() + (CurrentCursor + 1), [](const Cursor &a, const Cursor &b) -> bool {
                return a.SelectionStart < b.SelectionStart;
            });
            // update last added cursor index to be valid after sort
            for (int c = CurrentCursor; c > -1; c--)
                if (Cursors[c].CursorPosition == lastAddedCursorPos)
                    LastAddedCursor = c;
        }
    };

    void MergeCursorsIfPossible();

    struct UndoRecord {
        UndoRecord() {}
        ~UndoRecord() {}

        UndoRecord(
            const std::vector<UndoOperation> &operations,
            TextEditor::EditorStateT &before,
            TextEditor::EditorStateT &after
        );

        void Undo(TextEditor *editor);
        void Redo(TextEditor *editor);

        std::vector<UndoOperation> Operations;

        EditorStateT Before;
        EditorStateT After;
    };

    using UndoBufferT = std::vector<UndoRecord>;

    LinesT Lines;
    EditorStateT EditorState;
    UndoBufferT UndoBuffer;
    int UndoIndex;

private:
    void Colorize(int from_line_number = 0, int line_count = -1);
    void ColorizeRange(int from_line_number = 0, int to_line_number = 0);
    void ColorizeInternal();
    float TextDistanceToLineStart(const Coordinates &from) const;
    void EnsureCursorVisible(int cursor = -1);
    int GetPageSize() const;
    string GetText(const Coordinates &start, const Coordinates &end) const;
    Coordinates GetActualCursorCoordinates(int cursor = -1) const;
    Coordinates SanitizeCoordinates(const Coordinates &) const;
    void Advance(Coordinates &coords) const;
    void DeleteRange(const Coordinates &start, const Coordinates &end);
    int InsertTextAt(Coordinates &at, const char *);
    void AddUndo(UndoRecord &);
    Coordinates ScreenPosToCoordinates(const ImVec2 &position, bool is_insertion_mode = false, bool *is_over_line_number = nullptr) const;
    Coordinates FindWordStart(const Coordinates &from) const;
    Coordinates FindWordEnd(const Coordinates &from) const;
    Coordinates FindNextWord(const Coordinates &from) const;
    int GetCharacterIndexL(const Coordinates &coords) const;
    int GetCharacterIndexR(const Coordinates &coords) const;
    int GetCharacterColumn(int line_number, int char_index) const;
    int GetLineCharacterCount(int line_number) const;
    int GetLineMaxColumn(int line_number) const;
    bool IsOnWordBoundary(const Coordinates &at) const;
    void RemoveLines(int start, int end);
    void RemoveLine(int line_number, const std::unordered_set<int> *handled_cursors = nullptr);
    void RemoveCurrentLines();
    void OnLineChanged(bool before_change, int line_number, int column, int char_count, bool is_deleted);
    void RemoveGlyphsFromLine(int line_number, int start_char_index, int end_char_index = -1);
    void AddGlyphsToLine(int line_number, int target_index, LineT::iterator source_start, LineT::iterator source_end);
    void AddGlyphToLine(int line_number, int target_index, Glyph glyph);
    LineT &InsertLine(int line_number);
    void ChangeCurrentLinesIndentation(bool increase);
    void EnterCharacter(ImWchar character, bool is_shift);
    void Backspace(bool is_word_mode = false);
    void DeleteSelection(int cursor = -1);
    string GetWordUnderCursor() const;
    string GetWordAt(const Coordinates &coords) const;
    ImU32 GetGlyphColor(const Glyph &glyph) const;

    void HandleKeyboardInputs(bool is_parent_focused = false);
    void HandleMouseInputs();
    void UpdatePalette();
    void Render(bool is_parent_focused = false);

    bool FindNextOccurrence(const char *text, int text_size, const Coordinates &from, Coordinates &start_out, Coordinates &end_out);

    int TabSize;
    bool WithinRender;
    bool ScrollToCursor;
    bool ScrollToTop;
    float TextStart; // position (in pixels) where a code line starts relative to the left of the TextEditor.
    int LeftMargin;
    int ColorRangeMin, ColorRangeMax;
    SelectionModeT SelectionMode;

    bool IsDraggingSelection = false;

    PaletteT PalletteBase;
    PaletteT Pallette;
    const LanguageDefT *LanguageDef = nullptr;
    RegexListT RegexList;

    bool ShouldCheckComments;
    BreakpointsT Breakpoints;
    ErrorMarkersT ErrorMarkers;
    ImVec2 CharAdvance;
    string LineBuffer;
    uint64_t StartTime;

    float LastClickTime; // In ImGui time
};
