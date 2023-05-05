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
        int mLine;
        bool mEnabled;
        string mCondition;

        Breakpoint()
            : mLine(-1), mEnabled(false) {}
    };

    // Represents a character coordinate from the user's point of view,
    // i. e. consider an uniform grid (assuming fixed-width font) on the
    // screen as it is rendered, and each cell has its own coordinate, starting from 0.
    // Tabs are counted as [1..TabSize] count empty spaces, depending on
    // how many space is necessary to reach the next tab stop.
    // For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when TabSize = 4,
    // because it is rendered as "    ABC" on the screen.
    struct Coordinates {
        int mLine, mColumn;
        Coordinates() : mLine(0), mColumn(0) {}
        Coordinates(int aLine, int aColumn) : mLine(aLine), mColumn(aColumn) {
            assert(aLine >= 0);
            assert(aColumn >= 0);
        }
        static Coordinates Invalid() {
            static Coordinates invalid(-1, -1);
            return invalid;
        }

        bool operator==(const Coordinates &o) const {
            return mLine == o.mLine &&
                mColumn == o.mColumn;
        }

        bool operator!=(const Coordinates &o) const {
            return mLine != o.mLine ||
                mColumn != o.mColumn;
        }

        bool operator<(const Coordinates &o) const {
            if (mLine != o.mLine)
                return mLine < o.mLine;
            return mColumn < o.mColumn;
        }

        bool operator>(const Coordinates &o) const {
            if (mLine != o.mLine)
                return mLine > o.mLine;
            return mColumn > o.mColumn;
        }

        bool operator<=(const Coordinates &o) const {
            if (mLine != o.mLine)
                return mLine < o.mLine;
            return mColumn <= o.mColumn;
        }

        bool operator>=(const Coordinates &o) const {
            if (mLine != o.mLine)
                return mLine > o.mLine;
            return mColumn >= o.mColumn;
        }

        Coordinates operator-(const Coordinates &o) {
            return Coordinates(mLine - o.mLine, mColumn - o.mColumn);
        }
    };

    struct Identifier {
        Coordinates mLocation;
        string mDeclaration;
    };

    using IdentifiersT = std::unordered_map<string, Identifier>;
    using KeywordsT = std::unordered_set<string>;
    using ErrorMarkersT = std::map<int, string>;
    using BreakpointsT = std::unordered_set<int>;
    using PaletteT = std::array<ImU32, (unsigned)PaletteIndexT::Max>;
    using CharT = uint8_t;

    struct Glyph {
        CharT mChar;
        PaletteIndexT mColorIndex = PaletteIndexT::Default;
        bool mComment : 1;
        bool mMultiLineComment : 1;
        bool mPreprocessor : 1;

        Glyph(CharT aChar, PaletteIndexT aColorIndex) : mChar(aChar), mColorIndex(aColorIndex),
                                                        mComment(false), mMultiLineComment(false), mPreprocessor(false) {}
    };

    using LineT = std::vector<Glyph>;
    using LinesT = std::vector<LineT>;

    struct LanguageDefT {
        using TokenRegexStringT = std::pair<string, PaletteIndexT>;
        using TokenRegexStringsT = std::vector<TokenRegexStringT>;
        using TokenizeCallbackT = bool (*)(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end, PaletteIndexT &paletteIndex);

        string Name;
        KeywordsT mKeywords;
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
        static const LanguageDefT &Json();
    };

    enum class UndoOperationType {
        Add,
        Delete,
    };
    struct UndoOperation {
        string Text;
        TextEditor::Coordinates Start;
        TextEditor::Coordinates End;
        UndoOperationType mType;
    };

    TextEditor();
    ~TextEditor();

    void SetLanguageDefinition(const LanguageDefT &);
    const char *GetLanguageDefinitionName() const;

    const PaletteT &GetPalette() const { return PalletteBase; }
    void SetPalette(const PaletteT &);

    bool Render(const char *aTitle, bool aParentIsFocused = false, const ImVec2 &aSize = ImVec2(), bool aBorder = false);
    void SetText(const string &aText);
    string GetText() const;

    void SetTextLines(const std::vector<string> &);
    std::vector<string> GetTextLines() const;

    string GetClipboardText() const;
    string GetSelectedText(int aCursor = -1) const;
    string GetCurrentLineText() const;

    int GetTotalLines() const { return (int)Lines.size(); }

    void OnCursorPositionChanged(int aCursor);

    Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
    void SetCursorPosition(const Coordinates &aPosition, int aCursor = -1);
    void SetCursorPosition(int aLine, int aCharIndex, int aCursor = -1);

    inline void OnLineDeleted(int aLineIndex, const std::unordered_set<int> *aHandledCursors = nullptr) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.mLine >= aLineIndex) {
                if (aHandledCursors == nullptr || aHandledCursors->find(c) == aHandledCursors->end()) // move up if has not been handled already
                    SetCursorPosition({EditorState.Cursors[c].CursorPosition.mLine - 1, EditorState.Cursors[c].CursorPosition.mColumn}, c);
            }
        }
    }
    inline void OnLinesDeleted(int aFirstLineIndex, int aLastLineIndex) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.mLine >= aFirstLineIndex)
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.mLine - (aLastLineIndex - aFirstLineIndex), EditorState.Cursors[c].CursorPosition.mColumn}, c);
        }
    }
    inline void OnLineAdded(int aLineIndex) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.mLine >= aLineIndex)
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.mLine + 1, EditorState.Cursors[c].CursorPosition.mColumn}, c);
        }
    }

    inline ImVec4 U32ColorToVec4(ImU32 in) {
        float s = 1.0f / 255.0f;
        return ImVec4(
            ((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
            ((in >> IM_COL32_R_SHIFT) & 0xFF) * s
        );
    }

    void SetTabSize(int aValue);
    inline int GetTabSize() const { return TabSize; }

    void InsertText(const string &aValue, int aCursor = -1);
    void InsertText(const char *aValue, int aCursor = -1);

    void MoveUp(int aAmount = 1, bool aSelect = false);
    void MoveDown(int aAmount = 1, bool aSelect = false);
    void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
    void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
    void MoveTop(bool aSelect = false);
    void MoveBottom(bool aSelect = false);
    void MoveHome(bool aSelect = false);
    void MoveEnd(bool aSelect = false);

    void SetSelectionStart(const Coordinates &aPosition, int aCursor = -1);
    void SetSelectionEnd(const Coordinates &aPosition, int aCursor = -1);
    void SetSelection(const Coordinates &aStart, const Coordinates &aEnd, SelectionModeT aMode = SelectionModeT::Normal, int aCursor = -1, bool isSpawningNewCursor = false);
    void SetSelection(int aStartLine, int aStartCharIndex, int aEndLine, int aEndCharIndex, SelectionModeT aMode = SelectionModeT::Normal, int aCursor = -1, bool isSpawningNewCursor = false);
    void SelectWordUnderCursor();
    void SelectAll();
    bool HasSelection() const;

    void Copy();
    void Cut();
    void Paste();
    void Delete(bool aWordMode = false);

    int GetUndoIndex() const;
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo(int aSteps = 1);
    void Redo(int aSteps = 1);

    void ClearExtraCursors();
    void ClearSelections();
    void SelectNextOccurrenceOf(const char *aText, int aTextSize, int aCursor = -1);
    void AddCursorForNextOccurrence();

    static const PaletteT &GetMarianaPalette();
    static const PaletteT &GetDarkPalette();
    static const PaletteT &GetLightPalette();
    static const PaletteT &GetRetroBluePalette();

    static bool IsGlyphWordChar(const Glyph &aGlyph);

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
            const std::vector<UndoOperation> &aOperations,
            TextEditor::EditorStateT &aBefore,
            TextEditor::EditorStateT &aAfter
        );

        void Undo(TextEditor *aEditor);
        void Redo(TextEditor *aEditor);

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
    void ProcessInputs();
    void Colorize(int aFromLine = 0, int aCount = -1);
    void ColorizeRange(int aFromLine = 0, int aToLine = 0);
    void ColorizeInternal();
    float TextDistanceToLineStart(const Coordinates &aFrom) const;
    void EnsureCursorVisible(int aCursor = -1);
    int GetPageSize() const;
    string GetText(const Coordinates &aStart, const Coordinates &aEnd) const;
    Coordinates GetActualCursorCoordinates(int aCursor = -1) const;
    Coordinates SanitizeCoordinates(const Coordinates &aValue) const;
    void Advance(Coordinates &aCoordinates) const;
    void DeleteRange(const Coordinates &aStart, const Coordinates &aEnd);
    int InsertTextAt(Coordinates &aWhere, const char *aValue);
    void AddUndo(UndoRecord &aValue);
    Coordinates ScreenPosToCoordinates(const ImVec2 &aPosition, bool aInsertionMode = false, bool *isOverLineNumber = nullptr) const;
    Coordinates FindWordStart(const Coordinates &aFrom) const;
    Coordinates FindWordEnd(const Coordinates &aFrom) const;
    Coordinates FindNextWord(const Coordinates &aFrom) const;
    int GetCharacterIndexL(const Coordinates &aCoordinates) const;
    int GetCharacterIndexR(const Coordinates &aCoordinates) const;
    int GetCharacterColumn(int aLine, int aIndex) const;
    int GetLineCharacterCount(int aLine) const;
    int GetLineMaxColumn(int aLine) const;
    bool IsOnWordBoundary(const Coordinates &aAt) const;
    void RemoveLines(int aStart, int aEnd);
    void RemoveLine(int aIndex, const std::unordered_set<int> *aHandledCursors = nullptr);
    void RemoveCurrentLines();
    void OnLineChanged(bool aBeforeChange, int aLine, int aColumn, int aCharCount, bool aDeleted);
    void RemoveGlyphsFromLine(int aLine, int aStartChar, int aEndChar = -1);
    void AddGlyphsToLine(int aLine, int aTargetIndex, LineT::iterator aSourceStart, LineT::iterator aSourceEnd);
    void AddGlyphToLine(int aLine, int aTargetIndex, Glyph aGlyph);
    LineT &InsertLine(int aIndex);
    void ChangeCurrentLinesIndentation(bool aIncrease);
    void EnterCharacter(ImWchar aChar, bool aShift);
    void Backspace(bool aWordMode = false);
    void DeleteSelection(int aCursor = -1);
    string GetWordUnderCursor() const;
    string GetWordAt(const Coordinates &aCoords) const;
    ImU32 GetGlyphColor(const Glyph &aGlyph) const;

    void HandleKeyboardInputs(bool aParentIsFocused = false);
    void HandleMouseInputs();
    void UpdatePalette();
    void Render(bool aParentIsFocused = false);

    bool FindNextOccurrence(const char *aText, int aTextSize, const Coordinates &aFrom, Coordinates &outStart, Coordinates &outEnd);

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
