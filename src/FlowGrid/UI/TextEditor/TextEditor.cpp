#include "TextEditor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <regex>
#include <string>

#include "imgui.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!p(*first1, *first2))
            return false;
    }
    return first1 == last1 && first2 == last2;
}

TextEditor::TextEditor()
    : LineSpacing(1.0f), UndoIndex(0), TabSize(4), Overwrite(false), ReadOnly(false), WithinRender(false), ScrollToCursor(false), ScrollToTop(false), TextChanged(false), ColorizerEnabled(true), TextStart(20.0f), LeftMargin(10), ColorRangeMin(0), ColorRangeMax(0), SelectionMode(SelectionModeT::Normal), ShouldCheckComments(true), ShouldHandleKeyboardInputs(true), ShouldHandleMouseInputs(true), IgnoreImGuiChild(false), ShowWhitespaces(true), ShowShortTabGlyphs(false), StartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), LastClickTime(-1.0f) {
    SetPalette(GetMarianaPalette());
    Lines.push_back(LineT());
}

TextEditor::~TextEditor() {
}

void TextEditor::SetLanguageDefinition(const LanguageDefT &aLanguageDef) {
    LanguageDef = &aLanguageDef;
    RegexList.clear();

    for (const auto &r : LanguageDef->TokenRegexStrings)
        RegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

    Colorize();
}

const char *TextEditor::GetLanguageDefinitionName() const {
    return LanguageDef != nullptr ? LanguageDef->Name.c_str() : "unknown";
}

void TextEditor::SetPalette(const PaletteT &aValue) {
    PalletteBase = aValue;
}

string TextEditor::GetText(const Coordinates &aStart, const Coordinates &aEnd) const {
    string result;

    auto lstart = aStart.mLine;
    auto lend = aEnd.mLine;
    auto istart = GetCharacterIndexR(aStart);
    auto iend = GetCharacterIndexR(aEnd);
    size_t s = 0;

    for (size_t i = lstart; i < lend; i++)
        s += Lines[i].size();

    result.reserve(s + s / 8);

    while (istart < iend || lstart < lend) {
        if (lstart >= (int)Lines.size())
            break;

        auto &line = Lines[lstart];
        if (istart < (int)line.size()) {
            result += line[istart].mChar;
            istart++;
        } else {
            istart = 0;
            ++lstart;
            result += '\n';
        }
    }

    return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates(int aCursor) const {
    if (aCursor == -1)
        return SanitizeCoordinates(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition);
    else
        return SanitizeCoordinates(EditorState.Cursors[aCursor].CursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates &aValue) const {
    auto line = aValue.mLine;
    auto column = aValue.mColumn;
    if (line >= (int)Lines.size()) {
        if (Lines.empty()) {
            line = 0;
            column = 0;
        } else {
            line = (int)Lines.size() - 1;
            column = GetLineMaxColumn(line);
        }
        return Coordinates(line, column);
    } else {
        column = Lines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
        return Coordinates(line, column);
    }
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::CharT c) {
    if ((c & 0xFE) == 0xFC)
        return 6;
    if ((c & 0xFC) == 0xF8)
        return 5;
    if ((c & 0xF8) == 0xF0)
        return 4;
    else if ((c & 0xF0) == 0xE0)
        return 3;
    else if ((c & 0xE0) == 0xC0)
        return 2;
    return 1;
}

// "Borrowed" from ImGui source
static inline int ITextCharToUtf8(char *buf, int buf_size, unsigned int c) {
    if (c < 0x80) {
        buf[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        if (buf_size < 2) return 0;
        buf[0] = (char)(0xc0 + (c >> 6));
        buf[1] = (char)(0x80 + (c & 0x3f));
        return 2;
    }
    if (c >= 0xdc00 && c < 0xe000) {
        return 0;
    }
    if (c >= 0xd800 && c < 0xdc00) {
        if (buf_size < 4) return 0;
        buf[0] = (char)(0xf0 + (c >> 18));
        buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[3] = (char)(0x80 + ((c)&0x3f));
        return 4;
    }
    // else if (c < 0x10000)
    {
        if (buf_size < 3) return 0;
        buf[0] = (char)(0xe0 + (c >> 12));
        buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 + ((c)&0x3f));
        return 3;
    }
}

void TextEditor::Advance(Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= (int)Lines.size())
        return;

    auto &line = Lines[aCoordinates.mLine];
    auto cindex = GetCharacterIndexL(aCoordinates);

    if (cindex < (int)line.size()) {
        auto delta = UTF8CharLength(line[cindex].mChar);
        cindex = std::min(cindex + delta, (int)line.size());
    } else if (Lines.size() > aCoordinates.mLine + 1) {
        ++aCoordinates.mLine;
        cindex = 0;
    }
    aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
}

void TextEditor::DeleteRange(const Coordinates &aStart, const Coordinates &aEnd) {
    assert(aEnd >= aStart);
    assert(!ReadOnly);

    // printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

    if (aEnd == aStart)
        return;

    auto start = GetCharacterIndexL(aStart);
    auto end = GetCharacterIndexR(aEnd);

    if (aStart.mLine == aEnd.mLine) {
        auto n = GetLineMaxColumn(aStart.mLine);
        if (aEnd.mColumn >= n)
            RemoveGlyphsFromLine(aStart.mLine, start); // from start to end of line
        else
            RemoveGlyphsFromLine(aStart.mLine, start, end);
    } else {
        RemoveGlyphsFromLine(aStart.mLine, start); // from start to end of line
        RemoveGlyphsFromLine(aEnd.mLine, 0, end);
        auto &firstLine = Lines[aStart.mLine];
        auto &lastLine = Lines[aEnd.mLine];

        if (aStart.mLine < aEnd.mLine)
            AddGlyphsToLine(aStart.mLine, firstLine.size(), lastLine.begin(), lastLine.end());

        if (aStart.mLine < aEnd.mLine)
            RemoveLines(aStart.mLine + 1, aEnd.mLine + 1);
    }

    TextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates & /* inout */ aWhere, const char *aValue) {
    assert(!ReadOnly);

    int cindex = GetCharacterIndexR(aWhere);
    int totalLines = 0;
    while (*aValue != '\0') {
        assert(!Lines.empty());

        if (*aValue == '\r') {
            // skip
            ++aValue;
        } else if (*aValue == '\n') {
            if (cindex < (int)Lines[aWhere.mLine].size()) {
                auto &newLine = InsertLine(aWhere.mLine + 1);
                auto &line = Lines[aWhere.mLine];
                AddGlyphsToLine(aWhere.mLine + 1, 0, line.begin() + cindex, line.end());
                RemoveGlyphsFromLine(aWhere.mLine, cindex);
            } else {
                InsertLine(aWhere.mLine + 1);
            }
            ++aWhere.mLine;
            aWhere.mColumn = 0;
            cindex = 0;
            ++totalLines;
            ++aValue;
        } else {
            auto &line = Lines[aWhere.mLine];
            auto d = UTF8CharLength(*aValue);
            while (d-- > 0 && *aValue != '\0')
                AddGlyphToLine(aWhere.mLine, cindex++, Glyph(*aValue++, PaletteIndexT::Default));
            aWhere.mColumn = GetCharacterColumn(aWhere.mLine, cindex);
        }

        TextChanged = true;
    }

    return totalLines;
}

void TextEditor::AddUndo(UndoRecord &aValue) {
    assert(!ReadOnly);
    // printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
    //	aValue.Before.CursorPosition.mLine, aValue.Before.CursorPosition.mColumn,
    //	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
    //	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
    //	aValue.After.CursorPosition.mLine, aValue.After.CursorPosition.mColumn
    //	);

    UndoBuffer.resize((size_t)(UndoIndex + 1));
    UndoBuffer.back() = aValue;
    ++UndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &aPosition, bool aInsertionMode, bool *isOverLineNumber) const {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(aPosition.x - origin.x + 3.0f, aPosition.y - origin.y);

    if (isOverLineNumber != nullptr)
        *isOverLineNumber = local.x < TextStart;

    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;

    int lineNo = std::max(0, (int)floor(local.y / CharAdvance.y));

    int columnCoord = 0;

    if (lineNo >= 0 && lineNo < (int)Lines.size()) {
        auto &line = Lines.at(lineNo);

        int columnIndex = 0;
        string cumulatedString = "";
        float columnWidth = 0.0f;
        float columnX = 0.0f;
        int delta = 0;

        // First we find the hovered column coord.
        for (size_t columnIndex = 0; columnIndex < line.size(); ++columnIndex) {
            float columnWidth = 0.0f;
            int delta = 0;

            if (line[columnIndex].mChar == '\t') {
                float oldX = columnX;
                columnX = (1.0f + std::floor((1.0f + columnX) / (float(TabSize) * spaceSize))) * (float(TabSize) * spaceSize);
                columnWidth = columnX - oldX;
                delta = TabSize - (columnCoord % TabSize);
            } else {
                char buf[7];
                auto d = UTF8CharLength(line[columnIndex].mChar);
                int i = 0;
                while (i < 6 && d-- > 0)
                    buf[i++] = line[columnIndex].mChar;
                buf[i] = '\0';
                columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
                columnX += columnWidth;
                delta = 1;
            }

            if (TextStart + columnX - (aInsertionMode ? 0.5f : 0.0f) * columnWidth < local.x)
                columnCoord += delta;
            else
                break;
        }

        // Then we reduce by 1 column coord if cursor is on the left side of the hovered column.
        // if (aInsertionMode && TextStart + columnX - columnWidth * 2.0f < local.x)
        //	columnIndex = std::min((int)line.size() - 1, columnIndex + 1);
    }

    return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)Lines.size())
        return at;

    auto &line = Lines[at.mLine];
    auto cindex = GetCharacterIndexL(at);

    if (cindex >= (int)line.size())
        return at;

    bool initialIsWordChar = IsGlyphWordChar(line[cindex]);
    bool initialIsSpace = isspace(line[cindex].mChar);
    uint8_t initialChar = line[cindex].mChar;
    bool needToAdvance = false;
    while (true) {
        --cindex;
        if (cindex < 0) {
            cindex = 0;
            break;
        }

        auto c = line[cindex].mChar;
        if ((c & 0xC0) != 0x80) // not UTF code sequence 10xxxxxx
        {
            bool isWordChar = IsGlyphWordChar(line[cindex]);
            bool isSpace = isspace(line[cindex].mChar);
            if (initialIsSpace && !isSpace || initialIsWordChar && !isWordChar || !initialIsWordChar && !initialIsSpace && initialChar != line[cindex].mChar) {
                needToAdvance = true;
                break;
            }
        }
    }
    at.mColumn = GetCharacterColumn(at.mLine, cindex);
    if (needToAdvance)
        Advance(at);
    return at;
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)Lines.size())
        return at;

    auto &line = Lines[at.mLine];
    auto cindex = GetCharacterIndexL(at);

    if (cindex >= (int)line.size())
        return at;

    bool initialIsWordChar = IsGlyphWordChar(line[cindex]);
    bool initialIsSpace = isspace(line[cindex].mChar);
    uint8_t initialChar = line[cindex].mChar;
    while (true) {
        auto d = UTF8CharLength(line[cindex].mChar);
        cindex += d;
        if (cindex >= (int)line.size())
            break;

        bool isWordChar = IsGlyphWordChar(line[cindex]);
        bool isSpace = isspace(line[cindex].mChar);
        if (initialIsSpace && !isSpace || initialIsWordChar && !isWordChar || !initialIsWordChar && !initialIsSpace && initialChar != line[cindex].mChar)
            break;
    }
    at.mColumn = GetCharacterColumn(at.mLine, cindex);
    return at;
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)Lines.size())
        return at;

    // skip to the next non-word character
    auto cindex = GetCharacterIndexR(aFrom);
    bool isword = false;
    bool skip = false;
    if (cindex < (int)Lines[at.mLine].size()) {
        auto &line = Lines[at.mLine];
        isword = !!isalnum(line[cindex].mChar);
        skip = isword;
    }

    while (!isword || skip) {
        if (at.mLine >= Lines.size()) {
            auto l = std::max(0, (int)Lines.size() - 1);
            return Coordinates(l, GetLineMaxColumn(l));
        }

        auto &line = Lines[at.mLine];
        if (cindex < (int)line.size()) {
            isword = isalnum(line[cindex].mChar);

            if (isword && !skip)
                return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));

            if (!isword)
                skip = false;

            cindex++;
        } else {
            cindex = 0;
            ++at.mLine;
            skip = false;
            isword = false;
        }
    }

    return at;
}

int TextEditor::GetCharacterIndexL(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= Lines.size())
        return -1;

    auto &line = Lines[aCoordinates.mLine];
    int c = 0;
    int i = 0;
    int tabCoordsLeft = 0;

    for (; i < line.size() && c < aCoordinates.mColumn;) {
        if (line[i].mChar == '\t') {
            if (tabCoordsLeft == 0)
                tabCoordsLeft = TabSize - (c % TabSize);
            if (tabCoordsLeft > 0)
                tabCoordsLeft--;
            c++;
        } else
            ++c;
        if (tabCoordsLeft == 0)
            i += UTF8CharLength(line[i].mChar);
    }
    return i;
}

int TextEditor::GetCharacterIndexR(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= Lines.size())
        return -1;
    auto &line = Lines[aCoordinates.mLine];
    int c = 0;
    int i = 0;
    for (; i < line.size() && c < aCoordinates.mColumn;) {
        if (line[i].mChar == '\t')
            c = (c / TabSize) * TabSize + TabSize;
        else
            ++c;
        i += UTF8CharLength(line[i].mChar);
    }
    return i;
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const {
    if (aLine >= Lines.size())
        return 0;
    auto &line = Lines[aLine];
    int col = 0;
    int i = 0;
    while (i < aIndex && i < (int)line.size()) {
        auto c = line[i].mChar;
        i += UTF8CharLength(c);
        if (c == '\t')
            col = (col / TabSize) * TabSize + TabSize;
        else
            col++;
    }
    return col;
}

int TextEditor::GetLineCharacterCount(int aLine) const {
    if (aLine >= Lines.size())
        return 0;
    auto &line = Lines[aLine];
    int c = 0;
    for (unsigned i = 0; i < line.size(); c++)
        i += UTF8CharLength(line[i].mChar);
    return c;
}

int TextEditor::GetLineMaxColumn(int aLine) const {
    if (aLine >= Lines.size())
        return 0;
    auto &line = Lines[aLine];
    int col = 0;
    for (unsigned i = 0; i < line.size();) {
        auto c = line[i].mChar;
        if (c == '\t')
            col = (col / TabSize) * TabSize + TabSize;
        else
            col++;
        i += UTF8CharLength(c);
    }
    return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates &aAt) const {
    if (aAt.mLine >= (int)Lines.size() || aAt.mColumn == 0)
        return true;

    auto &line = Lines[aAt.mLine];
    auto cindex = GetCharacterIndexR(aAt);
    if (cindex >= (int)line.size())
        return true;

    if (ColorizerEnabled)
        return line[cindex].mColorIndex != line[size_t(cindex - 1)].mColorIndex;

    return isspace(line[cindex].mChar) != isspace(line[cindex - 1].mChar);
}

void TextEditor::RemoveLines(int aStart, int aEnd) {
    assert(!ReadOnly);
    assert(aEnd >= aStart);
    assert(Lines.size() > (size_t)(aEnd - aStart));

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers) {
        ErrorMarkersT::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
        if (e.first >= aStart && e.first <= aEnd)
            continue;
        etmp.insert(e);
    }
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints) {
        if (i >= aStart && i <= aEnd)
            continue;
        btmp.insert(i >= aStart ? i - 1 : i);
    }
    Breakpoints = std::move(btmp);

    Lines.erase(Lines.begin() + aStart, Lines.begin() + aEnd);
    assert(!Lines.empty());

    TextChanged = true;

    OnLinesDeleted(aStart, aEnd);
}

void TextEditor::RemoveLine(int aIndex, const std::unordered_set<int> *aHandledCursors) {
    assert(!ReadOnly);
    assert(Lines.size() > 1);

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers) {
        ErrorMarkersT::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
        if (e.first - 1 == aIndex)
            continue;
        etmp.insert(e);
    }
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints) {
        if (i == aIndex)
            continue;
        btmp.insert(i >= aIndex ? i - 1 : i);
    }
    Breakpoints = std::move(btmp);

    Lines.erase(Lines.begin() + aIndex);
    assert(!Lines.empty());

    TextChanged = true;

    OnLineDeleted(aIndex, aHandledCursors);
}

void TextEditor::RemoveCurrentLines() {
    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    }

    for (int c = EditorState.CurrentCursor; c > -1; c--) {
        int currentLine = EditorState.Cursors[c].CursorPosition.mLine;
        int nextLine = currentLine + 1;
        int prevLine = currentLine - 1;

        Coordinates toDeleteStart, toDeleteEnd;
        if (Lines.size() > nextLine) // next line exists
        {
            toDeleteStart = Coordinates(currentLine, 0);
            toDeleteEnd = Coordinates(nextLine, 0);
            SetCursorPosition({EditorState.Cursors[c].CursorPosition.mLine, 0}, c);
        } else if (prevLine > -1) // previous line exists
        {
            toDeleteStart = Coordinates(prevLine, GetLineMaxColumn(prevLine));
            toDeleteEnd = Coordinates(currentLine, GetLineMaxColumn(currentLine));
            SetCursorPosition({prevLine, 0}, c);
        } else {
            toDeleteStart = Coordinates(currentLine, 0);
            toDeleteEnd = Coordinates(currentLine, GetLineMaxColumn(currentLine));
            SetCursorPosition({currentLine, 0}, c);
        }

        u.Operations.push_back({GetText(toDeleteStart, toDeleteEnd), toDeleteStart, toDeleteEnd, UndoOperationType::Delete});

        std::unordered_set<int> handledCursors = {c};
        if (toDeleteStart.mLine != toDeleteEnd.mLine)
            RemoveLine(currentLine, &handledCursors);
        else
            DeleteRange(toDeleteStart, toDeleteEnd);
    }

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::OnLineChanged(bool aBeforeChange, int aLine, int aColumn, int aCharCount, bool aDeleted) {
    static std::unordered_map<int, int> cursorCharIndices;
    if (aBeforeChange) {
        cursorCharIndices.clear();
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            if (EditorState.Cursors[c].CursorPosition.mLine == aLine) {
                if (EditorState.Cursors[c].CursorPosition.mColumn > aColumn) {
                    cursorCharIndices[c] = GetCharacterIndexR({aLine, EditorState.Cursors[c].CursorPosition.mColumn});
                    cursorCharIndices[c] += aDeleted ? -aCharCount : aCharCount;
                }
            }
        }
    } else {
        for (auto &item : cursorCharIndices)
            SetCursorPosition({aLine, GetCharacterColumn(aLine, item.second)}, item.first);
    }
}

void TextEditor::RemoveGlyphsFromLine(int aLine, int aStartChar, int aEndChar) {
    int column = GetCharacterColumn(aLine, aStartChar);
    int deltaX = GetCharacterColumn(aLine, aEndChar) - column;
    auto &line = Lines[aLine];
    OnLineChanged(true, aLine, column, aEndChar - aStartChar, true);
    line.erase(line.begin() + aStartChar, aEndChar == -1 ? line.end() : line.begin() + aEndChar);
    OnLineChanged(false, aLine, column, aEndChar - aStartChar, true);
}

void TextEditor::AddGlyphsToLine(int aLine, int aTargetIndex, LineT::iterator aSourceStart, LineT::iterator aSourceEnd) {
    int targetColumn = GetCharacterColumn(aLine, aTargetIndex);
    int charsInserted = std::distance(aSourceStart, aSourceEnd);
    auto &line = Lines[aLine];
    OnLineChanged(true, aLine, targetColumn, charsInserted, false);
    line.insert(line.begin() + aTargetIndex, aSourceStart, aSourceEnd);
    OnLineChanged(false, aLine, targetColumn, charsInserted, false);
}

void TextEditor::AddGlyphToLine(int aLine, int aTargetIndex, Glyph aGlyph) {
    int targetColumn = GetCharacterColumn(aLine, aTargetIndex);
    auto &line = Lines[aLine];
    OnLineChanged(true, aLine, targetColumn, 1, false);
    line.insert(line.begin() + aTargetIndex, aGlyph);
    OnLineChanged(false, aLine, targetColumn, 1, false);
}

TextEditor::LineT &TextEditor::InsertLine(int aIndex) {
    assert(!ReadOnly);

    auto &result = *Lines.insert(Lines.begin() + aIndex, LineT());

    ErrorMarkersT etmp;
    for (auto &i : ErrorMarkers)
        etmp.insert(ErrorMarkersT::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
    ErrorMarkers = std::move(etmp);

    BreakpointsT btmp;
    for (auto i : Breakpoints)
        btmp.insert(i >= aIndex ? i + 1 : i);
    Breakpoints = std::move(btmp);

    OnLineAdded(aIndex);

    return result;
}

string TextEditor::GetWordUnderCursor() const {
    auto c = GetCursorPosition();
    return GetWordAt(c);
}

string TextEditor::GetWordAt(const Coordinates &aCoords) const {
    auto start = FindWordStart(aCoords);
    auto end = FindWordEnd(aCoords);

    string r;

    auto istart = GetCharacterIndexR(start);
    auto iend = GetCharacterIndexR(end);

    for (auto it = istart; it < iend; ++it)
        r.push_back(Lines[aCoords.mLine][it].mChar);

    return r;
}

ImU32 TextEditor::GetGlyphColor(const Glyph &aGlyph) const {
    if (!ColorizerEnabled)
        return Pallette[(int)PaletteIndexT::Default];
    if (aGlyph.mComment)
        return Pallette[(int)PaletteIndexT::Comment];
    if (aGlyph.mMultiLineComment)
        return Pallette[(int)PaletteIndexT::MultiLineComment];
    auto const color = Pallette[(int)aGlyph.mColorIndex];
    if (aGlyph.mPreprocessor) {
        const auto ppcolor = Pallette[(int)PaletteIndexT::Preprocessor];
        const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
        const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

bool TextEditor::IsGlyphWordChar(const Glyph &aGlyph) {
    int sizeInBytes = UTF8CharLength(aGlyph.mChar);
    return sizeInBytes > 1 ||
        aGlyph.mChar >= 'a' && aGlyph.mChar <= 'z' ||
        aGlyph.mChar >= 'A' && aGlyph.mChar <= 'Z' ||
        aGlyph.mChar >= '0' && aGlyph.mChar <= '9' ||
        aGlyph.mChar == '_';
}

void TextEditor::HandleKeyboardInputs(bool aParentIsFocused) {
    if (ImGui::IsWindowFocused() || aParentIsFocused) {
        if (ImGui::IsWindowHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        // ImGui::CaptureKeyboardFromApp(true);

        ImGuiIO &io = ImGui::GetIO();
        auto isOSX = io.ConfigMacOSXBehaviors;
        auto alt = io.KeyAlt;
        auto ctrl = io.KeyCtrl;
        auto shift = io.KeyShift;
        auto super = io.KeySuper;

        auto isShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && !alt && !shift;
        auto isShiftShortcut = (isOSX ? (super && !ctrl) : (ctrl && !super)) && shift && !alt;
        auto isWordmoveKey = isOSX ? alt : ctrl;
        auto isAltOnly = alt && !ctrl && !shift && !super;
        auto isCtrlOnly = ctrl && !alt && !shift && !super;
        auto isShiftOnly = shift && !alt && !ctrl && !super;

        io.WantCaptureKeyboard = true;
        io.WantTextInput = true;

        if (!ReadOnly && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
            Undo();
        else if (!ReadOnly && isAltOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
            Undo();
        else if (!ReadOnly && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Y)))
            Redo();
        else if (!ReadOnly && isShiftShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
            Redo();
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
            MoveUp(1, shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
            MoveDown(1, shift);
        else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
            MoveLeft(1, shift, isWordmoveKey);
        else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
            MoveRight(1, shift, isWordmoveKey);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
            MoveUp(GetPageSize() - 4, shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
            MoveDown(GetPageSize() - 4, shift);
        else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
            MoveTop(shift);
        else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
            MoveBottom(shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
            MoveHome(shift);
        else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
            MoveEnd(shift);
        else if (!ReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
            Delete(ctrl);
        else if (!ReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
            Backspace(ctrl);
        else if (!ReadOnly && !alt && ctrl && shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_K)))
            RemoveCurrentLines();
        else if (!ReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftBracket)))
            ChangeCurrentLinesIndentation(false);
        else if (!ReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightBracket)))
            ChangeCurrentLinesIndentation(true);
        else if (!alt && !ctrl && !shift && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Overwrite ^= true;
        else if (isCtrlOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Copy();
        else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C)))
            Copy();
        else if (!ReadOnly && isShiftOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert)))
            Paste();
        else if (!ReadOnly && isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)))
            Paste();
        else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X)))
            Cut();
        else if (isShiftOnly && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
            Cut();
        else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
            SelectAll();
        else if (isShortcut && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_D)))
            AddCursorForNextOccurrence();
        else if (!ReadOnly && !alt && !ctrl && !shift && !super && (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_KeypadEnter))))
            EnterCharacter('\n', false);
        else if (!ReadOnly && !alt && !ctrl && !super && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
            EnterCharacter('\t', shift);
        if (!ReadOnly && !io.InputQueueCharacters.empty() && !ctrl && !super) {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
                auto c = io.InputQueueCharacters[i];
                if (c != 0 && (c == '\n' || c >= 32))
                    EnterCharacter(c, shift);
            }
            io.InputQueueCharacters.resize(0);
        }
    }
}

void TextEditor::HandleMouseInputs() {
    ImGuiIO &io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ImGui::IsWindowHovered()) {
        auto click = ImGui::IsMouseClicked(0);
        if (!shift && !alt) {
            auto doubleClick = ImGui::IsMouseDoubleClicked(0);
            auto t = ImGui::GetTime();
            auto tripleClick = click && !doubleClick && (LastClickTime != -1.0f && (t - LastClickTime) < io.MouseDoubleClickTime);

            /*
            Left mouse button triple click
            */

            if (tripleClick) {
                if (ctrl)
                    EditorState.AddCursor();
                else
                    EditorState.CurrentCursor = 0;

                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                SelectionMode = SelectionModeT::Line;
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);

                LastClickTime = -1.0f;
            }

            /*
            Left mouse button double click
            */

            else if (doubleClick) {
                if (ctrl)
                    EditorState.AddCursor();
                else
                    EditorState.CurrentCursor = 0;

                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = FindWordStart(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition);
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = FindWordEnd(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition);
                if (SelectionMode == SelectionModeT::Line)
                    SelectionMode = SelectionModeT::Normal;
                else
                    SelectionMode = SelectionModeT::Word;
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);

                LastClickTime = (float)ImGui::GetTime();
            }

            /*
            Left mouse button click
            */
            else if (click) {
                if (ctrl)
                    EditorState.AddCursor();
                else
                    EditorState.CurrentCursor = 0;

                bool isOverLineNumber;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite, &isOverLineNumber);
                if (isOverLineNumber)
                    SelectionMode = SelectionModeT::Line;
                else if (ctrl)
                    SelectionMode = SelectionModeT::Word;
                else
                    SelectionMode = SelectionModeT::Normal;
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode, -1, ctrl);

                LastClickTime = (float)ImGui::GetTime();
            }
            // Mouse left button dragging (=> update selection)
            else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                IsDraggingSelection = true;
                io.WantCaptureMouse = true;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
                SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode);
            } else if (ImGui::IsMouseReleased(0)) {
                IsDraggingSelection = false;
                EditorState.SortCursorsFromTopToBottom();
                MergeCursorsIfPossible();
            }
        } else if (shift) {
            if (click) {
                Coordinates oldCursorPosition = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
                Coordinates newSelection = ScreenPosToCoordinates(ImGui::GetMousePos(), !Overwrite);
                if (newSelection > EditorState.Cursors[EditorState.CurrentCursor].CursorPosition)
                    SetSelectionEnd(newSelection);
                else
                    SetSelectionStart(newSelection);
                EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = EditorState.Cursors[EditorState.CurrentCursor].SelectionEnd;
                EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].SelectionStart;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = newSelection;
                EditorState.Cursors[EditorState.CurrentCursor].CursorPositionChanged = oldCursorPosition != newSelection;
            }
        }
    }
}

void TextEditor::UpdatePalette() {
    /* Update palette with the current alpha from style */
    for (int i = 0; i < (int)PaletteIndexT::Max; ++i) {
        auto color = U32ColorToVec4(PalletteBase[i]);
        color.w *= ImGui::GetStyle().Alpha;
        Pallette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }
}

void TextEditor::Render(bool aParentIsFocused) {
    /* Compute CharAdvance regarding to scaled font size (Ctrl + mouse wheel)*/
    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    CharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * LineSpacing);

    assert(LineBuffer.empty());

    auto contentSize = ImGui::GetWindowContentRegionMax();
    auto drawList = ImGui::GetWindowDrawList();
    float longest(TextStart);

    if (ScrollToTop) {
        ScrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    auto scrollX = ImGui::GetScrollX();
    auto scrollY = ImGui::GetScrollY();

    auto lineNo = (int)floor(scrollY / CharAdvance.y);
    auto globalLineMax = (int)Lines.size();
    auto lineMax = std::max(0, std::min((int)Lines.size() - 1, lineNo + (int)floor((scrollY + contentSize.y) / CharAdvance.y)));

    // Deduce TextStart by evaluating mLines size (global lineMax) plus two spaces as text width
    char buf[16];
    snprintf(buf, 16, " %d ", globalLineMax);
    TextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + LeftMargin;

    if (!Lines.empty()) {
        float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

        while (lineNo <= lineMax) {
            ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * CharAdvance.y);
            ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + TextStart, lineStartScreenPos.y);

            auto &line = Lines[lineNo];
            longest = std::max(TextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
            auto columnNo = 0;
            Coordinates lineStartCoord(lineNo, 0);
            Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

            // Draw selection for the current line
            for (int c = 0; c <= EditorState.CurrentCursor; c++) {
                float sstart = -1.0f;
                float ssend = -1.0f;

                assert(EditorState.Cursors[c].SelectionStart <= EditorState.Cursors[c].SelectionEnd);
                if (EditorState.Cursors[c].SelectionStart <= lineEndCoord)
                    sstart = EditorState.Cursors[c].SelectionStart > lineStartCoord ? TextDistanceToLineStart(EditorState.Cursors[c].SelectionStart) : 0.0f;
                if (EditorState.Cursors[c].SelectionEnd > lineStartCoord)
                    ssend = TextDistanceToLineStart(EditorState.Cursors[c].SelectionEnd < lineEndCoord ? EditorState.Cursors[c].SelectionEnd : lineEndCoord);

                if (EditorState.Cursors[c].SelectionEnd.mLine > lineNo)
                    ssend += CharAdvance.x;

                if (sstart != -1 && ssend != -1 && sstart < ssend) {
                    ImVec2 vstart(lineStartScreenPos.x + TextStart + sstart, lineStartScreenPos.y);
                    ImVec2 vend(lineStartScreenPos.x + TextStart + ssend, lineStartScreenPos.y + CharAdvance.y);
                    drawList->AddRectFilled(vstart, vend, Pallette[(int)PaletteIndexT::Selection]);
                }
            }

            // Draw breakpoints
            auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

            if (Breakpoints.count(lineNo + 1) != 0) {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + CharAdvance.y);
                drawList->AddRectFilled(start, end, Pallette[(int)PaletteIndexT::Breakpoint]);
            }

            // Draw error markers
            auto errorIt = ErrorMarkers.find(lineNo + 1);
            if (errorIt != ErrorMarkers.end()) {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + CharAdvance.y);
                drawList->AddRectFilled(start, end, Pallette[(int)PaletteIndexT::ErrorMarker]);

                if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end)) {
                    ImGui::BeginTooltip();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    ImGui::Text("Error at line %d:", errorIt->first);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.2f, 1.0f));
                    ImGui::Text("%s", errorIt->second.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndTooltip();
                }
            }

            // Draw line number (right aligned)
            snprintf(buf, 16, "%d  ", lineNo + 1);

            auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
            drawList->AddText(ImVec2(lineStartScreenPos.x + TextStart - lineNoWidth, lineStartScreenPos.y), Pallette[(int)PaletteIndexT::LineNumber], buf);

            std::vector<Coordinates> cursorCoordsInThisLine;
            for (int c = 0; c <= EditorState.CurrentCursor; c++) {
                if (EditorState.Cursors[c].CursorPosition.mLine == lineNo)
                    cursorCoordsInThisLine.push_back(EditorState.Cursors[c].CursorPosition);
            }
            if (cursorCoordsInThisLine.size() > 0) {
                auto focused = ImGui::IsWindowFocused() || aParentIsFocused;

                // Render the cursors
                if (focused) {
                    for (const auto &cursorCoords : cursorCoordsInThisLine) {
                        float width = 1.0f;
                        auto cindex = GetCharacterIndexR(cursorCoords);
                        float cx = TextDistanceToLineStart(cursorCoords);

                        if (Overwrite && cindex < (int)line.size()) {
                            auto c = line[cindex].mChar;
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + cx) / (float(TabSize) * spaceSize))) * (float(TabSize) * spaceSize);
                                width = x - cx;
                            } else {
                                char buf2[2];
                                buf2[0] = line[cindex].mChar;
                                buf2[1] = '\0';
                                width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
                            }
                        }
                        ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
                        ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + CharAdvance.y);
                        drawList->AddRectFilled(cstart, cend, Pallette[(int)PaletteIndexT::Cursor]);
                    }
                }
            }

            // Render colorized text
            auto prevColor = line.empty() ? Pallette[(int)PaletteIndexT::Default] : GetGlyphColor(line[0]);
            ImVec2 bufferOffset;

            for (int i = 0; i < line.size();) {
                auto &glyph = line[i];
                auto color = GetGlyphColor(glyph);

                if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !LineBuffer.empty()) {
                    const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                    drawList->AddText(newOffset, prevColor, LineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, LineBuffer.c_str(), nullptr, nullptr);
                    bufferOffset.x += textSize.x;
                    LineBuffer.clear();
                }
                prevColor = color;

                if (glyph.mChar == '\t') {
                    auto oldX = bufferOffset.x;
                    bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(TabSize) * spaceSize))) * (float(TabSize) * spaceSize);
                    ++i;

                    if (ShowWhitespaces) {
                        ImVec2 p1, p2, p3, p4;

                        if (ShowShortTabGlyphs) {
                            const auto s = ImGui::GetFontSize();
                            const auto x1 = textScreenPos.x + oldX + 1.0f;
                            const auto x2 = textScreenPos.x + oldX + CharAdvance.x - 1.0f;
                            const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;

                            p1 = ImVec2(x1, y);
                            p2 = ImVec2(x2, y);
                            p3 = ImVec2(x2 - s * 0.16f, y - s * 0.16f);
                            p4 = ImVec2(x2 - s * 0.16f, y + s * 0.16f);
                        } else {
                            const auto s = ImGui::GetFontSize();
                            const auto x1 = textScreenPos.x + oldX + 1.0f;
                            const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
                            const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;

                            p1 = ImVec2(x1, y);
                            p2 = ImVec2(x2, y);
                            p3 = ImVec2(x2 - s * 0.2f, y - s * 0.2f);
                            p4 = ImVec2(x2 - s * 0.2f, y + s * 0.2f);
                        }

                        drawList->AddLine(p1, p2, Pallette[(int)PaletteIndexT::ControlCharacter]);
                        drawList->AddLine(p2, p3, Pallette[(int)PaletteIndexT::ControlCharacter]);
                        drawList->AddLine(p2, p4, Pallette[(int)PaletteIndexT::ControlCharacter]);
                    }
                } else if (glyph.mChar == ' ') {
                    if (ShowWhitespaces) {
                        const auto s = ImGui::GetFontSize();
                        const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
                        const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
                    }
                    bufferOffset.x += spaceSize;
                    i++;
                } else {
                    auto l = UTF8CharLength(glyph.mChar);
                    while (l-- > 0)
                        LineBuffer.push_back(line[i++].mChar);
                }
                ++columnNo;
            }

            if (!LineBuffer.empty()) {
                const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                drawList->AddText(newOffset, prevColor, LineBuffer.c_str());
                LineBuffer.clear();
            }

            ++lineNo;
        }

        // Draw a tooltip on known identifiers/preprocessor symbols
        if (ImGui::IsMousePosValid() && ImGui::IsWindowHovered() && LanguageDef != nullptr) {
            auto mpos = ImGui::GetMousePos();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImVec2 local(mpos.x - origin.x, mpos.y - origin.y);
            // printf("Mouse: pos(%g, %g), origin(%g, %g), local(%g, %g)\n", mpos.x, mpos.y, origin.x, origin.y, local.x, local.y);
            if (local.x >= TextStart) {
                auto pos = ScreenPosToCoordinates(mpos);
                // printf("Coord(%d, %d)\n", pos.mLine, pos.mColumn);
                auto id = GetWordAt(pos);
                if (!id.empty()) {
                    auto it = LanguageDef->Identifiers.find(id);
                    if (it != LanguageDef->Identifiers.end()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(it->second.mDeclaration.c_str());
                        ImGui::EndTooltip();
                    } else {
                        auto pi = LanguageDef->PreprocIdentifiers.find(id);
                        if (pi != LanguageDef->PreprocIdentifiers.end()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(pi->second.mDeclaration.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
            }
        }
    }

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(ImVec2((longest + 2), Lines.size() * CharAdvance.y));

    if (ScrollToCursor) {
        EnsureCursorVisible();
        ScrollToCursor = false;
    }
}

bool TextEditor::FindNextOccurrence(const char *aText, int aTextSize, const Coordinates &aFrom, Coordinates &outStart, Coordinates &outEnd) {
    assert(aTextSize > 0);
    for (int i = 0; i < Lines.size(); i++) {
        int currentLine = (aFrom.mLine + i) % Lines.size();
        int lineStartIndex = i == 0 ? GetCharacterIndexR(aFrom) : 0;
        int aTextIndex = 0;
        int j = lineStartIndex;
        for (; j < Lines[currentLine].size(); j++) {
            if (aTextIndex == aTextSize || aText[aTextIndex] == '\0')
                break;
            if (aText[aTextIndex] == Lines[currentLine][j].mChar)
                aTextIndex++;
            else
                aTextIndex = 0;
        }
        if (aTextIndex == aTextSize || aText[aTextIndex] == '\0') {
            if (aText[aTextIndex] == '\0')
                aTextSize = aTextIndex;
            outStart = {currentLine, GetCharacterColumn(currentLine, j - aTextSize)};
            outEnd = {currentLine, GetCharacterColumn(currentLine, j)};
            return true;
        }
    }
    // in line where we started again but from char index 0 to aFrom.mColumn
    {
        int aTextIndex = 0;
        int j = 0;
        for (; j < GetCharacterIndexR(aFrom); j++) {
            if (aTextIndex == aTextSize || aText[aTextIndex] == '\0')
                break;
            if (aText[aTextIndex] == Lines[aFrom.mLine][j].mChar)
                aTextIndex++;
            else
                aTextIndex = 0;
        }
        if (aTextIndex == aTextSize || aText[aTextIndex] == '\0') {
            if (aText[aTextIndex] == '\0')
                aTextSize = aTextIndex;
            outStart = {aFrom.mLine, GetCharacterColumn(aFrom.mLine, j - aTextSize)};
            outEnd = {aFrom.mLine, GetCharacterColumn(aFrom.mLine, j)};
            return true;
        }
    }
    return false;
}

bool TextEditor::Render(const char *aTitle, bool aParentIsFocused, const ImVec2 &aSize, bool aBorder) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        if (EditorState.Cursors[c].CursorPositionChanged)
            OnCursorPositionChanged(c);
        if (c <= EditorState.CurrentCursor)
            EditorState.Cursors[c].CursorPositionChanged = false;
    }

    WithinRender = true;
    TextChanged = false;

    UpdatePalette();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(Pallette[(int)PaletteIndexT::Background]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!IgnoreImGuiChild)
        ImGui::BeginChild(aTitle, aSize, aBorder, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

    bool isFocused = ImGui::IsWindowFocused();
    if (ShouldHandleKeyboardInputs) {
        HandleKeyboardInputs(aParentIsFocused);
        ImGui::PushAllowKeyboardFocus(true);
    }

    if (ShouldHandleMouseInputs)
        HandleMouseInputs();

    ColorizeInternal();
    Render(aParentIsFocused);

    if (ShouldHandleKeyboardInputs)
        ImGui::PopAllowKeyboardFocus();

    if (!IgnoreImGuiChild)
        ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    WithinRender = false;
    return isFocused;
}

void TextEditor::SetText(const string &aText) {
    Lines.clear();
    Lines.emplace_back(LineT());
    for (auto chr : aText) {
        if (chr == '\r') {
            // ignore the carriage return character
        } else if (chr == '\n')
            Lines.emplace_back(LineT());
        else {
            Lines.back().emplace_back(Glyph(chr, PaletteIndexT::Default));
        }
    }

    TextChanged = true;
    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::SetTextLines(const std::vector<string> &aLines) {
    Lines.clear();

    if (aLines.empty()) {
        Lines.emplace_back(LineT());
    } else {
        Lines.resize(aLines.size());

        for (size_t i = 0; i < aLines.size(); ++i) {
            const string &aLine = aLines[i];

            Lines[i].reserve(aLine.size());
            for (size_t j = 0; j < aLine.size(); ++j)
                Lines[i].emplace_back(Glyph(aLine[j], PaletteIndexT::Default));
        }
    }

    TextChanged = true;
    ScrollToTop = true;

    UndoBuffer.clear();
    UndoIndex = 0;

    Colorize();
}

void TextEditor::ChangeCurrentLinesIndentation(bool aIncrease) {
    assert(!ReadOnly);

    UndoRecord u;
    u.Before = EditorState;

    for (int c = EditorState.CurrentCursor; c > -1; c--) {
        auto start = EditorState.Cursors[c].SelectionStart;
        auto end = EditorState.Cursors[c].SelectionEnd;
        auto originalEnd = end;

        if (start > end)
            std::swap(start, end);
        start.mColumn = 0;
        //			end.mColumn = end.mLine < mLines.size() ? mLines[end.mLine].size() : 0;
        if (end.mColumn == 0 && end.mLine > 0)
            --end.mLine;
        if (end.mLine >= (int)Lines.size())
            end.mLine = Lines.empty() ? 0 : (int)Lines.size() - 1;
        end.mColumn = GetLineMaxColumn(end.mLine);

        // if (end.mColumn >= GetLineMaxColumn(end.mLine))
        //	end.mColumn = GetLineMaxColumn(end.mLine) - 1;

        UndoOperation removeOperation = {GetText(start, end), start, end, UndoOperationType::Delete};

        bool modified = false;

        for (int i = start.mLine; i <= end.mLine; i++) {
            auto &line = Lines[i];
            if (!aIncrease) {
                if (!line.empty())

                {
                    if (line.front().mChar == '\t') {
                        RemoveGlyphsFromLine(i, 0, 1);
                        modified = true;
                    } else {
                        for (int j = 0; j < TabSize && !line.empty() && line.front().mChar == ' '; j++) {
                            RemoveGlyphsFromLine(i, 0, 1);
                            modified = true;
                        }
                    }
                }
            } else if (Lines[i].size() > 0) {
                AddGlyphToLine(i, 0, Glyph('\t', TextEditor::PaletteIndexT::Background));
                modified = true;
            }
        }

        if (modified) {
            start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
            Coordinates rangeEnd;
            string addedText;
            if (originalEnd.mColumn != 0) {
                end = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
                rangeEnd = end;
                addedText = GetText(start, end);
            } else {
                end = Coordinates(originalEnd.mLine, 0);
                rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
                addedText = GetText(start, rangeEnd);
            }

            u.Operations.push_back(removeOperation);
            u.Operations.push_back({addedText, start, rangeEnd, UndoOperationType::Add});
            u.After = EditorState;

            EditorState.Cursors[c].SelectionStart = start;
            EditorState.Cursors[c].SelectionEnd = end;

            TextChanged = true;
        }
    }

    EnsureCursorVisible();
    if (u.Operations.size() > 0)
        AddUndo(u);
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift) {
    assert(!ReadOnly);

    bool hasSelection = HasSelection();
    bool anyCursorHasMultilineSelection = false;
    for (int c = EditorState.CurrentCursor; c > -1; c--)
        if (EditorState.Cursors[c].SelectionStart.mLine != EditorState.Cursors[c].SelectionEnd.mLine) {
            anyCursorHasMultilineSelection = true;
            break;
        }
    bool isIndentOperation = hasSelection && anyCursorHasMultilineSelection && aChar == '\t';
    if (isIndentOperation) {
        ChangeCurrentLinesIndentation(!aShift);
        return;
    }

    UndoRecord u;
    u.Before = EditorState;

    if (hasSelection) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    } // HasSelection

    std::vector<Coordinates> coords;
    for (int c = EditorState.CurrentCursor; c > -1; c--) // order important here for typing \n in the same line at the same time
    {
        auto coord = GetActualCursorCoordinates(c);
        coords.push_back(coord);
        UndoOperation added;
        added.mType = UndoOperationType::Add;
        added.Start = coord;

        assert(!Lines.empty());

        if (aChar == '\n') {
            InsertLine(coord.mLine + 1);
            auto &line = Lines[coord.mLine];
            auto &newLine = Lines[coord.mLine + 1];

            added.Text = "";
            added.Text += (char)aChar;
            if (LanguageDef != nullptr && LanguageDef->AudioIndentation)
                for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it) {
                    newLine.push_back(line[it]);
                    added.Text += line[it].mChar;
                }

            const size_t whitespaceSize = newLine.size();
            auto cindex = GetCharacterIndexR(coord);
            AddGlyphsToLine(coord.mLine + 1, newLine.size(), line.begin() + cindex, line.end());
            RemoveGlyphsFromLine(coord.mLine, cindex);
            SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)), c);
        } else {
            char buf[7];
            int e = ITextCharToUtf8(buf, 7, aChar);
            if (e > 0) {
                buf[e] = '\0';
                auto &line = Lines[coord.mLine];
                auto cindex = GetCharacterIndexR(coord);

                if (Overwrite && cindex < (int)line.size()) {
                    auto d = UTF8CharLength(line[cindex].mChar);

                    UndoOperation removed;
                    removed.mType = UndoOperationType::Delete;
                    removed.Start = EditorState.Cursors[c].CursorPosition;
                    removed.End = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

                    while (d-- > 0 && cindex < (int)line.size()) {
                        removed.Text += line[cindex].mChar;
                        RemoveGlyphsFromLine(coord.mLine, cindex, cindex + 1);
                    }
                    u.Operations.push_back(removed);
                }

                for (auto p = buf; *p != '\0'; p++, ++cindex)
                    AddGlyphToLine(coord.mLine, cindex, Glyph(*p, PaletteIndexT::Default));
                added.Text = buf;

                SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)), c);
            } else
                continue;
        }

        TextChanged = true;

        added.End = GetActualCursorCoordinates(c);
        u.Operations.push_back(added);
    }

    u.After = EditorState;
    AddUndo(u);

    for (const auto &coord : coords)
        Colorize(coord.mLine - 1, 3);
    EnsureCursorVisible();
}

void TextEditor::OnCursorPositionChanged(int aCursor) {
    if (IsDraggingSelection)
        return;

    // std::cout << "Cursor position changed\n";
    EditorState.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
}

void TextEditor::SetCursorPosition(const Coordinates &aPosition, int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    // string log = "Moved cursor " + std::to_string(aCursor) + " from " +
    //	std::to_string(mState.Cursors[aCursor].CursorPosition.mLine) + "," + std::to_string(mState.Cursors[aCursor].CursorPosition.mColumn) + " to ";

    if (EditorState.Cursors[aCursor].CursorPosition != aPosition) {
        EditorState.Cursors[aCursor].CursorPosition = aPosition;
        EditorState.Cursors[aCursor].CursorPositionChanged = true;
        EnsureCursorVisible();
    }

    // log += std::to_string(mState.Cursors[aCursor].CursorPosition.mLine) + "," + std::to_string(mState.Cursors[aCursor].CursorPosition.mColumn);
    // std::cout << log << std::endl;
}

void TextEditor::SetCursorPosition(int aLine, int aCharIndex, int aCursor) {
    SetCursorPosition({aLine, GetCharacterColumn(aLine, aCharIndex)}, aCursor);
}

void TextEditor::SetSelectionStart(const Coordinates &aPosition, int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    EditorState.Cursors[aCursor].SelectionStart = SanitizeCoordinates(aPosition);
    if (EditorState.Cursors[aCursor].SelectionStart > EditorState.Cursors[aCursor].SelectionEnd)
        std::swap(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates &aPosition, int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    EditorState.Cursors[aCursor].SelectionEnd = SanitizeCoordinates(aPosition);
    if (EditorState.Cursors[aCursor].SelectionStart > EditorState.Cursors[aCursor].SelectionEnd)
        std::swap(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionEnd);
}

void TextEditor::SetSelection(const Coordinates &aStart, const Coordinates &aEnd, SelectionModeT aMode, int aCursor, bool isSpawningNewCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    auto oldSelStart = EditorState.Cursors[aCursor].SelectionStart;
    auto oldSelEnd = EditorState.Cursors[aCursor].SelectionEnd;

    EditorState.Cursors[aCursor].SelectionStart = SanitizeCoordinates(aStart);
    EditorState.Cursors[aCursor].SelectionEnd = SanitizeCoordinates(aEnd);
    if (EditorState.Cursors[aCursor].SelectionStart > EditorState.Cursors[aCursor].SelectionEnd)
        std::swap(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionEnd);

    switch (aMode) {
        case TextEditor::SelectionModeT::Normal:
        case TextEditor::SelectionModeT::Word:
            break;
        case TextEditor::SelectionModeT::Line: {
            const auto lineNo = EditorState.Cursors[aCursor].SelectionEnd.mLine;
            const auto lineSize = (size_t)lineNo < Lines.size() ? Lines[lineNo].size() : 0;
            EditorState.Cursors[aCursor].SelectionStart = Coordinates(EditorState.Cursors[aCursor].SelectionStart.mLine, 0);
            EditorState.Cursors[aCursor].SelectionEnd = Lines.size() > lineNo + 1 ? Coordinates(lineNo + 1, 0) : Coordinates(lineNo, GetLineMaxColumn(lineNo));
            EditorState.Cursors[aCursor].CursorPosition = EditorState.Cursors[aCursor].SelectionEnd;
            break;
        }
        default:
            break;
    }

    if (EditorState.Cursors[aCursor].SelectionStart != oldSelStart ||
        EditorState.Cursors[aCursor].SelectionEnd != oldSelEnd)
        if (!isSpawningNewCursor)
            EditorState.Cursors[aCursor].CursorPositionChanged = true;
}

void TextEditor::SetSelection(int aStartLine, int aStartCharIndex, int aEndLine, int aEndCharIndex, SelectionModeT aMode, int aCursor, bool isSpawningNewCursor) {
    SetSelection(
        {aStartLine, GetCharacterColumn(aStartLine, aStartCharIndex)},
        {aEndLine, GetCharacterColumn(aEndLine, aEndCharIndex)},
        aMode, aCursor, isSpawningNewCursor
    );
}

void TextEditor::SetTabSize(int aValue) {
    TabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::InsertText(const string &aValue, int aCursor) {
    InsertText(aValue.c_str(), aCursor);
}

void TextEditor::InsertText(const char *aValue, int aCursor) {
    if (aValue == nullptr)
        return;
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    auto pos = GetActualCursorCoordinates(aCursor);
    auto start = std::min(pos, EditorState.Cursors[aCursor].SelectionStart);
    int totalLines = pos.mLine - start.mLine;

    totalLines += InsertTextAt(pos, aValue);

    SetSelection(pos, pos, SelectionModeT::Normal, aCursor);
    SetCursorPosition(pos, aCursor);
    Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection(int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    assert(EditorState.Cursors[aCursor].SelectionEnd >= EditorState.Cursors[aCursor].SelectionStart);

    if (EditorState.Cursors[aCursor].SelectionEnd == EditorState.Cursors[aCursor].SelectionStart)
        return;

    DeleteRange(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionEnd);

    SetSelection(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionStart, SelectionModeT::Normal, aCursor);
    SetCursorPosition(EditorState.Cursors[aCursor].SelectionStart, aCursor);
    EditorState.Cursors[aCursor].InteractiveStart = EditorState.Cursors[aCursor].SelectionStart;
    EditorState.Cursors[aCursor].InteractiveEnd = EditorState.Cursors[aCursor].SelectionEnd;
    Colorize(EditorState.Cursors[aCursor].SelectionStart.mLine, 1);
}

void TextEditor::MoveUp(int aAmount, bool aSelect) {
    if (HasSelection() && !aSelect) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionStart, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionStart);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto oldPos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition.mLine = std::max(0, EditorState.Cursors[c].CursorPosition.mLine - aAmount);
            if (oldPos != EditorState.Cursors[c].CursorPosition) {
                if (aSelect) {
                    if (oldPos == EditorState.Cursors[c].InteractiveStart)
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    else if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    else {
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                        EditorState.Cursors[c].InteractiveEnd = oldPos;
                    }
                } else
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
            }
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveDown(int aAmount, bool aSelect) {
    if (HasSelection() && !aSelect) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionEnd, EditorState.Cursors[c].SelectionEnd, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionEnd);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            assert(EditorState.Cursors[c].CursorPosition.mColumn >= 0);
            auto oldPos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition.mLine = std::max(0, std::min((int)Lines.size() - 1, EditorState.Cursors[c].CursorPosition.mLine + aAmount));

            if (EditorState.Cursors[c].CursorPosition != oldPos) {
                if (aSelect) {
                    if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    else if (oldPos == EditorState.Cursors[c].InteractiveStart)
                        EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    else {
                        EditorState.Cursors[c].InteractiveStart = oldPos;
                        EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                    }
                } else
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
            }
        }
    }
    EnsureCursorVisible();
}

static bool IsUTFSequence(char c) {
    return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode) {
    if (Lines.empty())
        return;

    if (HasSelection() && !aSelect && !aWordMode) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionStart, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionStart);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            int amount = aAmount;
            auto oldPos = EditorState.Cursors[c].CursorPosition;
            EditorState.Cursors[c].CursorPosition = GetActualCursorCoordinates(c);
            auto line = EditorState.Cursors[c].CursorPosition.mLine;
            auto cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);

            while (amount-- > 0) {
                if (cindex == 0) {
                    if (line > 0) {
                        --line;
                        if ((int)Lines.size() > line)
                            cindex = (int)Lines[line].size();
                        else
                            cindex = 0;
                    }
                } else {
                    --cindex;
                    if (cindex > 0) {
                        if ((int)Lines.size() > line) {
                            while (cindex > 0 && IsUTFSequence(Lines[line][cindex].mChar))
                                --cindex;
                        }
                    }
                }

                EditorState.Cursors[c].CursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
                if (aWordMode) {
                    EditorState.Cursors[c].CursorPosition = FindWordStart(EditorState.Cursors[c].CursorPosition);
                    cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);
                }
            }

            EditorState.Cursors[c].CursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
            std::cout << "changed from " << oldPos.mColumn << " to " << EditorState.Cursors[c].CursorPosition.mColumn << std::endl;

            assert(EditorState.Cursors[c].CursorPosition.mColumn >= 0);
            if (aSelect) {
                if (oldPos == EditorState.Cursors[c].InteractiveStart)
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                else if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                    EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                else {
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                    EditorState.Cursors[c].InteractiveEnd = oldPos;
                }
            } else {
                if (HasSelection() && !aWordMode)
                    EditorState.Cursors[c].CursorPosition = EditorState.Cursors[c].InteractiveStart;
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
            std::cout << "Setting selection for " << c << std::endl;
            SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, aSelect && aWordMode ? SelectionModeT::Word : SelectionModeT::Normal, c);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode) {
    if (Lines.empty())
        return;

    if (HasSelection() && !aSelect && !aWordMode) {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            SetSelection(EditorState.Cursors[c].SelectionEnd, EditorState.Cursors[c].SelectionEnd, SelectionModeT::Normal, c);
            SetCursorPosition(EditorState.Cursors[c].SelectionEnd);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto oldPos = EditorState.Cursors[c].CursorPosition;
            if (oldPos.mLine >= Lines.size())
                continue;

            int amount = aAmount;
            auto cindex = GetCharacterIndexR(EditorState.Cursors[c].CursorPosition);
            while (amount-- > 0) {
                auto lindex = EditorState.Cursors[c].CursorPosition.mLine;
                auto &line = Lines[lindex];

                if (cindex >= line.size()) {
                    if (EditorState.Cursors[c].CursorPosition.mLine < Lines.size() - 1) {
                        EditorState.Cursors[c].CursorPosition.mLine = std::max(0, std::min((int)Lines.size() - 1, EditorState.Cursors[c].CursorPosition.mLine + 1));
                        EditorState.Cursors[c].CursorPosition.mColumn = 0;
                    } else
                        continue;
                } else {
                    cindex += UTF8CharLength(line[cindex].mChar);
                    EditorState.Cursors[c].CursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
                    if (aWordMode)
                        EditorState.Cursors[c].CursorPosition = FindWordEnd(EditorState.Cursors[c].CursorPosition);
                }
            }

            if (aSelect) {
                if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                    EditorState.Cursors[c].InteractiveEnd = SanitizeCoordinates(EditorState.Cursors[c].CursorPosition);
                else if (oldPos == EditorState.Cursors[c].InteractiveStart)
                    EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                else {
                    EditorState.Cursors[c].InteractiveStart = oldPos;
                    EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
                }
            } else {
                if (HasSelection() && !aWordMode)
                    EditorState.Cursors[c].CursorPosition = EditorState.Cursors[c].InteractiveEnd;
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
            SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, aSelect && aWordMode ? SelectionModeT::Word : SelectionModeT::Normal, c);
        }
    }
    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect) {
    EditorState.CurrentCursor = 0;
    auto oldPos = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
    SetCursorPosition(Coordinates(0, 0));

    if (EditorState.Cursors[EditorState.CurrentCursor].CursorPosition != oldPos) {
        if (aSelect) {
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = oldPos;
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
        } else
            EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = EditorState.Cursors[EditorState.CurrentCursor].CursorPosition;
        SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd);
    }
}

void TextEditor::TextEditor::MoveBottom(bool aSelect) {
    EditorState.CurrentCursor = 0;
    auto oldPos = GetCursorPosition();
    auto newPos = Coordinates((int)Lines.size() - 1, 0);
    SetCursorPosition(newPos);
    if (aSelect) {
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = oldPos;
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = newPos;
    } else
        EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = newPos;
    SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        auto oldPos = EditorState.Cursors[c].CursorPosition;
        SetCursorPosition(Coordinates(EditorState.Cursors[c].CursorPosition.mLine, 0), c);

        if (aSelect) {
            if (oldPos == EditorState.Cursors[c].InteractiveStart)
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
            else if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            else {
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
                EditorState.Cursors[c].InteractiveEnd = oldPos;
            }
        } else
            EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
        SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
    }
}

void TextEditor::MoveEnd(bool aSelect) {
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        auto oldPos = EditorState.Cursors[c].CursorPosition;
        SetCursorPosition(Coordinates(EditorState.Cursors[c].CursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)), c);

        if (aSelect) {
            if (oldPos == EditorState.Cursors[c].InteractiveEnd)
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            else if (oldPos == EditorState.Cursors[c].InteractiveStart)
                EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].CursorPosition;
            else {
                EditorState.Cursors[c].InteractiveStart = oldPos;
                EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
            }
        } else
            EditorState.Cursors[c].InteractiveStart = EditorState.Cursors[c].InteractiveEnd = EditorState.Cursors[c].CursorPosition;
        SetSelection(EditorState.Cursors[c].InteractiveStart, EditorState.Cursors[c].InteractiveEnd, SelectionModeT::Normal, c);
    }
}

void TextEditor::Delete(bool aWordMode) {
    assert(!ReadOnly);

    if (Lines.empty())
        return;

    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    } else {
        std::vector<Coordinates> positions;
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto pos = GetActualCursorCoordinates(c);
            positions.push_back(pos);
            SetCursorPosition(pos, c);
            auto &line = Lines[pos.mLine];

            if (pos.mColumn == GetLineMaxColumn(pos.mLine)) {
                if (pos.mLine == (int)Lines.size() - 1)
                    continue;

                Coordinates startCoords = GetActualCursorCoordinates(c);
                Coordinates endCoords = startCoords;
                Advance(endCoords);
                u.Operations.push_back({"\n", startCoords, endCoords, UndoOperationType::Delete});

                auto &nextLine = Lines[pos.mLine + 1];
                AddGlyphsToLine(pos.mLine, line.size(), nextLine.begin(), nextLine.end());
                for (int otherCursor = c + 1;
                     otherCursor <= EditorState.CurrentCursor && EditorState.Cursors[otherCursor].CursorPosition.mLine == pos.mLine + 1;
                     otherCursor++) // move up cursors in next line
                {
                    int otherCursorCharIndex = GetCharacterIndexR(EditorState.Cursors[otherCursor].CursorPosition);
                    int otherCursorNewCharIndex = GetCharacterIndexR(pos) + otherCursorCharIndex;
                    auto targetCoords = Coordinates(pos.mLine, GetCharacterColumn(pos.mLine, otherCursorNewCharIndex));
                    SetCursorPosition(targetCoords, otherCursor);
                }
                RemoveLine(pos.mLine + 1);
            } else {
                if (aWordMode) {
                    Coordinates end = FindWordEnd(EditorState.Cursors[c].CursorPosition);
                    u.Operations.push_back({GetText(EditorState.Cursors[c].CursorPosition, end), EditorState.Cursors[c].CursorPosition, end, UndoOperationType::Delete});
                    DeleteRange(EditorState.Cursors[c].CursorPosition, end);
                    int charactersDeleted = end.mColumn - EditorState.Cursors[c].CursorPosition.mColumn;
                } else {
                    auto cindex = GetCharacterIndexR(pos);

                    Coordinates start = GetActualCursorCoordinates(c);
                    Coordinates end = start;
                    end.mColumn++;
                    u.Operations.push_back({GetText(start, end), start, end, UndoOperationType::Delete});

                    auto d = UTF8CharLength(line[cindex].mChar);
                    while (d-- > 0 && cindex < (int)line.size())
                        RemoveGlyphsFromLine(pos.mLine, cindex, cindex + 1);
                }
            }
        }

        TextChanged = true;

        for (const auto &pos : positions)
            Colorize(pos.mLine, 1);
    }

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::Backspace(bool aWordMode) {
    assert(!ReadOnly);

    if (Lines.empty())
        return;

    UndoRecord u;
    u.Before = EditorState;

    if (HasSelection()) {
        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
            DeleteSelection(c);
        }
    } else {
        for (int c = 0; c <= EditorState.CurrentCursor; c++) {
            auto pos = GetActualCursorCoordinates(c);
            SetCursorPosition(pos, c);

            if (EditorState.Cursors[c].CursorPosition.mColumn == 0) {
                if (EditorState.Cursors[c].CursorPosition.mLine == 0)
                    continue;

                Coordinates startCoords = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
                Coordinates endCoords = startCoords;
                Advance(endCoords);
                u.Operations.push_back({"\n", startCoords, endCoords, UndoOperationType::Delete});

                auto &line = Lines[EditorState.Cursors[c].CursorPosition.mLine];
                int prevLineIndex = EditorState.Cursors[c].CursorPosition.mLine - 1;
                auto &prevLine = Lines[prevLineIndex];
                auto prevSize = GetLineMaxColumn(prevLineIndex);
                AddGlyphsToLine(prevLineIndex, prevLine.size(), line.begin(), line.end());
                std::unordered_set<int> cursorsHandled = {c};
                for (int otherCursor = c + 1;
                     otherCursor <= EditorState.CurrentCursor && EditorState.Cursors[otherCursor].CursorPosition.mLine == EditorState.Cursors[c].CursorPosition.mLine;
                     otherCursor++) // move up cursors in same line
                {
                    int otherCursorCharIndex = GetCharacterIndexR(EditorState.Cursors[otherCursor].CursorPosition);
                    int otherCursorNewCharIndex = GetCharacterIndexR({prevLineIndex, prevSize}) + otherCursorCharIndex;
                    auto targetCoords = Coordinates(prevLineIndex, GetCharacterColumn(prevLineIndex, otherCursorNewCharIndex));
                    SetCursorPosition(targetCoords, otherCursor);
                    cursorsHandled.insert(otherCursor);
                }

                ErrorMarkersT etmp;
                for (auto &i : ErrorMarkers)
                    etmp.insert(ErrorMarkersT::value_type(i.first - 1 == EditorState.Cursors[c].CursorPosition.mLine ? i.first - 1 : i.first, i.second));
                ErrorMarkers = std::move(etmp);

                RemoveLine(EditorState.Cursors[c].CursorPosition.mLine, &cursorsHandled);
                SetCursorPosition({EditorState.Cursors[c].CursorPosition.mLine - 1, prevSize}, c);
            } else {
                auto &line = Lines[EditorState.Cursors[c].CursorPosition.mLine];

                if (aWordMode) {
                    Coordinates start = FindWordStart(EditorState.Cursors[c].CursorPosition - Coordinates(0, 1));
                    u.Operations.push_back({GetText(start, EditorState.Cursors[c].CursorPosition), start, EditorState.Cursors[c].CursorPosition, UndoOperationType::Delete});
                    DeleteRange(start, EditorState.Cursors[c].CursorPosition);
                    int charactersDeleted = EditorState.Cursors[c].CursorPosition.mColumn - start.mColumn;
                    EditorState.Cursors[c].CursorPosition.mColumn -= charactersDeleted;
                } else {
                    auto cindex = GetCharacterIndexR(pos) - 1;
                    auto cend = cindex + 1;
                    while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
                        --cindex;

                    // if (cindex > 0 && UTF8CharLength(line[cindex].mChar) > 1)
                    //	--cindex;

                    UndoOperation removed;
                    removed.mType = UndoOperationType::Delete;
                    removed.Start = removed.End = GetActualCursorCoordinates(c);

                    if (line[cindex].mChar == '\t') {
                        int tabStartColumn = GetCharacterColumn(removed.Start.mLine, cindex);
                        int tabLength = removed.Start.mColumn - tabStartColumn;
                        EditorState.Cursors[c].CursorPosition.mColumn -= tabLength;
                        removed.Start.mColumn -= tabLength;
                    } else {
                        --EditorState.Cursors[c].CursorPosition.mColumn;
                        --removed.Start.mColumn;
                    }

                    while (cindex < line.size() && cend-- > cindex) {
                        removed.Text += line[cindex].mChar;
                        RemoveGlyphsFromLine(EditorState.Cursors[c].CursorPosition.mLine, cindex, cindex + 1);
                    }
                    u.Operations.push_back(removed);
                }
                EditorState.Cursors[c].CursorPositionChanged = true;
            }
        }

        TextChanged = true;

        EnsureCursorVisible();
        for (int c = 0; c <= EditorState.CurrentCursor; c++)
            Colorize(EditorState.Cursors[c].CursorPosition.mLine, 1);
    }

    u.After = EditorState;
    AddUndo(u);
}

void TextEditor::SelectWordUnderCursor() {
    auto c = GetCursorPosition();
    SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll() {
    SetSelection(Coordinates(0, 0), Coordinates((int)Lines.size(), 0), SelectionModeT::Line);
}

bool TextEditor::HasSelection() const {
    for (int c = 0; c <= EditorState.CurrentCursor; c++)
        if (EditorState.Cursors[c].SelectionEnd > EditorState.Cursors[c].SelectionStart)
            return true;
    return false;
}

void TextEditor::Copy() {
    if (HasSelection()) {
        string clipboardText = GetClipboardText();
        ImGui::SetClipboardText(clipboardText.c_str());
    } else {
        if (!Lines.empty()) {
            string str;
            auto &line = Lines[GetActualCursorCoordinates().mLine];
            for (auto &g : line)
                str.push_back(g.mChar);
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut() {
    if (ReadOnly) {
        Copy();
    } else {
        if (HasSelection()) {
            UndoRecord u;
            u.Before = EditorState;

            Copy();
            for (int c = EditorState.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
                DeleteSelection(c);
            }

            u.After = EditorState;
            AddUndo(u);
        }
    }
}

void TextEditor::Paste() {
    if (ReadOnly)
        return;

    // check if we should do multicursor paste
    string clipText = ImGui::GetClipboardText();
    bool canPasteToMultipleCursors = false;
    std::vector<std::pair<int, int>> clipTextLines;
    if (EditorState.CurrentCursor > 0) {
        clipTextLines.push_back({0, 0});
        for (int i = 0; i < clipText.length(); i++) {
            if (clipText[i] == '\n') {
                clipTextLines.back().second = i;
                clipTextLines.push_back({i + 1, 0});
            }
        }
        clipTextLines.back().second = clipText.length();
        canPasteToMultipleCursors = clipTextLines.size() == EditorState.CurrentCursor + 1;
    }

    if (clipText.length() > 0) {
        UndoRecord u;
        u.Before = EditorState;

        if (HasSelection()) {
            for (int c = EditorState.CurrentCursor; c > -1; c--) {
                u.Operations.push_back({GetSelectedText(c), EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd, UndoOperationType::Delete});
                DeleteSelection(c);
            }
        }

        for (int c = EditorState.CurrentCursor; c > -1; c--) {
            Coordinates start = GetActualCursorCoordinates(c);
            if (canPasteToMultipleCursors) {
                string clipSubText = clipText.substr(clipTextLines[c].first, clipTextLines[c].second - clipTextLines[c].first);
                InsertText(clipSubText, c);
                u.Operations.push_back({clipSubText, start, GetActualCursorCoordinates(c), UndoOperationType::Add});
            } else {
                InsertText(clipText, c);
                u.Operations.push_back({clipText, start, GetActualCursorCoordinates(c), UndoOperationType::Add});
            }
        }

        u.After = EditorState;
        AddUndo(u);
    }
}

int TextEditor::GetUndoIndex() const {
    return UndoIndex;
}

bool TextEditor::CanUndo() const {
    return !ReadOnly && UndoIndex > 0;
}

bool TextEditor::CanRedo() const {
    return !ReadOnly && UndoIndex < (int)UndoBuffer.size();
}

void TextEditor::Undo(int aSteps) {
    while (CanUndo() && aSteps-- > 0)
        UndoBuffer[--UndoIndex].Undo(this);
}

void TextEditor::Redo(int aSteps) {
    while (CanRedo() && aSteps-- > 0)
        UndoBuffer[UndoIndex++].Redo(this);
}

void TextEditor::ClearExtraCursors() {
    EditorState.CurrentCursor = 0;
}

void TextEditor::ClearSelections() {
    for (int c = EditorState.CurrentCursor; c > -1; c--)
        EditorState.Cursors[c].InteractiveEnd =
            EditorState.Cursors[c].InteractiveStart =
                EditorState.Cursors[c].SelectionEnd =
                    EditorState.Cursors[c].SelectionStart = EditorState.Cursors[c].CursorPosition;
}

void TextEditor::SelectNextOccurrenceOf(const char *aText, int aTextSize, int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;
    Coordinates nextStart, nextEnd;
    FindNextOccurrence(aText, aTextSize, EditorState.Cursors[aCursor].CursorPosition, nextStart, nextEnd);
    EditorState.Cursors[aCursor].InteractiveStart = nextStart;
    EditorState.Cursors[aCursor].CursorPosition = EditorState.Cursors[aCursor].InteractiveEnd = nextEnd;
    SetSelection(EditorState.Cursors[aCursor].InteractiveStart, EditorState.Cursors[aCursor].InteractiveEnd, SelectionMode, aCursor);
    EnsureCursorVisible(aCursor);
}

void TextEditor::AddCursorForNextOccurrence() {
    const Cursor &currentCursor = EditorState.Cursors[EditorState.GetLastAddedCursorIndex()];
    if (currentCursor.SelectionStart == currentCursor.SelectionEnd)
        return;

    string selectionText = GetText(currentCursor.SelectionStart, currentCursor.SelectionEnd);
    Coordinates nextStart, nextEnd;
    if (!FindNextOccurrence(selectionText.c_str(), selectionText.length(), currentCursor.SelectionEnd, nextStart, nextEnd))
        return;

    EditorState.AddCursor();
    EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart = nextStart;
    EditorState.Cursors[EditorState.CurrentCursor].CursorPosition = EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd = nextEnd;
    SetSelection(EditorState.Cursors[EditorState.CurrentCursor].InteractiveStart, EditorState.Cursors[EditorState.CurrentCursor].InteractiveEnd, SelectionMode, -1, true);
    EditorState.SortCursorsFromTopToBottom();
    MergeCursorsIfPossible();
    EnsureCursorVisible();
}

const TextEditor::PaletteT &TextEditor::GetDarkPalette() {
    const static PaletteT p = {{
        0xb0b0b0ff, // Default
        0x569cd6ff, // Keyword
        0x00ff00ff, // Number
        0xe07070ff, // String
        0xe0a070ff, // Char literal
        0xffffffff, // Punctuation
        0x808040ff, // Preprocessor
        0xaaaaaaff, // Identifier
        0x4dc69bff, // Known identifier
        0xa040c0ff, // Preproc identifier
        0x206020ff, // Comment (single line)
        0x206040ff, // Comment (multi line)
        0x101010ff, // Background
        0xe0e0e0ff, // Cursor
        0x2060a080, // Selection
        0xff200080, // ErrorMarker
        0x90909090, // ControlCharacter
        0x0080f040, // Breakpoint
        0x007070ff, // Line number
        0x00000040, // Current line fill
        0x80808040, // Current line fill (inactive)
        0xa0a0a040, // Current line edge
    }};
    return p;
}

const TextEditor::PaletteT &TextEditor::GetMarianaPalette() {
    const static PaletteT p = {{
        0xffffffff, // Default
        0xc695c6ff, // Keyword
        0xf9ae58ff, // Number
        0x99c794ff, // String
        0xe0a070ff, // Char literal
        0x5fb4b4ff, // Punctuation
        0x808040ff, // Preprocessor
        0xffffffff, // Identifier
        0x4dc69bff, // Known identifier
        0xe0a0ffff, // Preproc identifier
        0xa6acb9ff, // Comment (single line)
        0xa6acb9ff, // Comment (multi line)
        0x303841ff, // Background
        0xe0e0e0ff, // Cursor
        0x4e5a6580, // Selection
        0xec5f6680, // ErrorMarker
        0xffffff30, // ControlCharacter
        0x0080f040, // Breakpoint
        0xffffffb0, // Line number
        0x4e5a6580, // Current line fill
        0x4e5a6530, // Current line fill (inactive)
        0x4e5a65b0, // Current line edge
    }};
    return p;
}

const TextEditor::PaletteT &TextEditor::GetLightPalette() {
    const static PaletteT p = {{
        0x404040ff, // None
        0x060cffff, // Keyword
        0x008000ff, // Number
        0xa02020ff, // String
        0x704030ff, // Char literal
        0x000000ff, // Punctuation
        0x606040ff, // Preprocessor
        0x404040ff, // Identifier
        0x106060ff, // Known identifier
        0xa040c0ff, // Preproc identifier
        0x205020ff, // Comment (single line)
        0x205040ff, // Comment (multi line)
        0xffffffff, // Background
        0x000000ff, // Cursor
        0x00006040, // Selection
        0xff1000a0, // ErrorMarker
        0x90909090, // ControlCharacter
        0x0080f080, // Breakpoint
        0x005050ff, // Line number
        0x00000040, // Current line fill
        0x80808040, // Current line fill (inactive)
        0x00000040, // Current line edge
    }};
    return p;
}

const TextEditor::PaletteT &TextEditor::GetRetroBluePalette() {
    const static PaletteT p = {{
        0xffff00ff, // None
        0x00ffffff, // Keyword
        0x00ff00ff, // Number
        0x008080ff, // String
        0x008080ff, // Char literal
        0xffffffff, // Punctuation
        0x008000ff, // Preprocessor
        0xffff00ff, // Identifier
        0xffffffff, // Known identifier
        0xff00ffff, // Preproc identifier
        0x808080ff, // Comment (single line)
        0x404040ff, // Comment (multi line)
        0x000080ff, // Background
        0xff8000ff, // Cursor
        0x00ffff80, // Selection
        0xff0000a0, // ErrorMarker
        0x0080ff80, // Breakpoint
        0x008080ff, // Line number
        0x00000040, // Current line fill
        0x80808040, // Current line fill (inactive)
        0x00000040, // Current line edge
    }};
    return p;
}

void TextEditor::MergeCursorsIfPossible() {
    // requires the cursors to be sorted from top to bottom
    std::unordered_set<int> cursorsToDelete;
    if (HasSelection()) {
        // merge cursors if they overlap
        for (int c = EditorState.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1;

            bool pcContainsC = EditorState.Cursors[pc].SelectionEnd >= EditorState.Cursors[c].SelectionEnd;
            bool pcContainsStartOfC = EditorState.Cursors[pc].SelectionEnd >= EditorState.Cursors[c].SelectionStart;

            if (pcContainsC) {
                cursorsToDelete.insert(c);
            } else if (pcContainsStartOfC) {
                EditorState.Cursors[pc].SelectionEnd = EditorState.Cursors[c].SelectionEnd;
                EditorState.Cursors[pc].InteractiveEnd = EditorState.Cursors[c].SelectionEnd;
                EditorState.Cursors[pc].InteractiveStart = EditorState.Cursors[pc].SelectionStart;
                EditorState.Cursors[pc].CursorPosition = EditorState.Cursors[c].SelectionEnd;
                cursorsToDelete.insert(c);
            }
        }
    } else {
        // merge cursors if they are at the same position
        for (int c = EditorState.CurrentCursor; c > 0; c--) // iterate backwards through pairs
        {
            int pc = c - 1;
            if (EditorState.Cursors[pc].CursorPosition == EditorState.Cursors[c].CursorPosition)
                cursorsToDelete.insert(c);
        }
    }
    for (int c = EditorState.CurrentCursor; c > -1; c--) // iterate backwards through each of them
    {
        if (cursorsToDelete.find(c) != cursorsToDelete.end())
            EditorState.Cursors.erase(EditorState.Cursors.begin() + c);
    }
    EditorState.CurrentCursor -= cursorsToDelete.size();
}

string TextEditor::GetText() const {
    auto lastLine = (int)Lines.size() - 1;
    auto lastLineLength = GetLineMaxColumn(lastLine);
    return GetText(Coordinates(), Coordinates(lastLine, lastLineLength));
}

std::vector<string> TextEditor::GetTextLines() const {
    std::vector<string> result;

    result.reserve(Lines.size());

    for (auto &line : Lines) {
        string text;

        text.resize(line.size());

        for (size_t i = 0; i < line.size(); ++i)
            text[i] = line[i].mChar;

        result.emplace_back(std::move(text));
    }

    return result;
}

string TextEditor::GetClipboardText() const {
    string result;
    for (int c = 0; c <= EditorState.CurrentCursor; c++) {
        if (EditorState.Cursors[c].SelectionStart < EditorState.Cursors[c].SelectionEnd) {
            if (result.length() != 0)
                result += '\n';
            result += GetText(EditorState.Cursors[c].SelectionStart, EditorState.Cursors[c].SelectionEnd);
        }
    }
    return result;
}

string TextEditor::GetSelectedText(int aCursor) const {
    if (aCursor == -1)
        aCursor = EditorState.CurrentCursor;

    return GetText(EditorState.Cursors[aCursor].SelectionStart, EditorState.Cursors[aCursor].SelectionEnd);
}

string TextEditor::GetCurrentLineText() const {
    auto lineLength = GetLineMaxColumn(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.mLine);
    return GetText(
        Coordinates(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.mLine, 0),
        Coordinates(EditorState.Cursors[EditorState.CurrentCursor].CursorPosition.mLine, lineLength)
    );
}

void TextEditor::ProcessInputs() {
}

void TextEditor::Colorize(int aFromLine, int aLines) {
    int toLine = aLines == -1 ? (int)Lines.size() : std::min((int)Lines.size(), aFromLine + aLines);
    ColorRangeMin = std::min(ColorRangeMin, aFromLine);
    ColorRangeMax = std::max(ColorRangeMax, toLine);
    ColorRangeMin = std::max(0, ColorRangeMin);
    ColorRangeMax = std::max(ColorRangeMin, ColorRangeMax);
    ShouldCheckComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine) {
    if (Lines.empty() || aFromLine >= aToLine || LanguageDef == nullptr)
        return;

    string buffer;
    std::cmatch results;
    string id;

    int endLine = std::max(0, std::min((int)Lines.size(), aToLine));
    for (int i = aFromLine; i < endLine; ++i) {
        auto &line = Lines[i];

        if (line.empty())
            continue;

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j) {
            auto &col = line[j];
            buffer[j] = col.mChar;
            col.mColorIndex = PaletteIndexT::Default;
        }

        const char *bufferBegin = &buffer.front();
        const char *bufferEnd = bufferBegin + buffer.size();

        auto last = bufferEnd;

        for (auto first = bufferBegin; first != last;) {
            const char *token_begin = nullptr;
            const char *token_end = nullptr;
            PaletteIndexT token_color = PaletteIndexT::Default;

            bool hasTokenizeResult = false;

            if (LanguageDef->Tokenize != nullptr) {
                if (LanguageDef->Tokenize(first, last, token_begin, token_end, token_color))
                    hasTokenizeResult = true;
            }

            if (hasTokenizeResult == false) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

                for (const auto &p : RegexList) {
                    if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous)) {
                        hasTokenizeResult = true;

                        auto &v = *results.begin();
                        token_begin = v.first;
                        token_end = v.second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (hasTokenizeResult == false) {
                first++;
            } else {
                const size_t token_length = token_end - token_begin;

                if (token_color == PaletteIndexT::Identifier) {
                    id.assign(token_begin, token_end);

                    // todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                    if (!LanguageDef->IsCaseSensitive)
                        std::transform(id.begin(), id.end(), id.begin(), ::toupper);

                    if (!line[first - bufferBegin].mPreprocessor) {
                        if (LanguageDef->mKeywords.count(id) != 0)
                            token_color = PaletteIndexT::Keyword;
                        else if (LanguageDef->Identifiers.count(id) != 0)
                            token_color = PaletteIndexT::KnownIdentifier;
                        else if (LanguageDef->PreprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndexT::PreprocIdentifier;
                    } else {
                        if (LanguageDef->PreprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndexT::PreprocIdentifier;
                    }
                }

                for (size_t j = 0; j < token_length; ++j)
                    line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

                first = token_end;
            }
        }
    }
}

void TextEditor::ColorizeInternal() {
    if (Lines.empty() || !ColorizerEnabled || LanguageDef == nullptr)
        return;

    if (ShouldCheckComments) {
        auto endLine = Lines.size();
        auto endIndex = 0;
        auto commentStartLine = endLine;
        auto commentStartIndex = endIndex;
        auto withinString = false;
        auto withinSingleLineComment = false;
        auto withinPreproc = false;
        auto firstChar = true; // there is no other non-whitespace characters in the line before
        auto concatenate = false; // '\' on the very end of the line
        auto currentLine = 0;
        auto currentIndex = 0;
        while (currentLine < endLine || currentIndex < endIndex) {
            auto &line = Lines[currentLine];

            if (currentIndex == 0 && !concatenate) {
                withinSingleLineComment = false;
                withinPreproc = false;
                firstChar = true;
            }

            concatenate = false;

            if (!line.empty()) {
                auto &g = line[currentIndex];
                auto c = g.mChar;

                if (c != LanguageDef->PreprocChar && !isspace(c))
                    firstChar = false;

                if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].mChar == '\\')
                    concatenate = true;

                bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

                if (withinString) {
                    line[currentIndex].mMultiLineComment = inComment;

                    if (c == '\"') {
                        if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].mChar == '\"') {
                            currentIndex += 1;
                            if (currentIndex < (int)line.size())
                                line[currentIndex].mMultiLineComment = inComment;
                        } else
                            withinString = false;
                    } else if (c == '\\') {
                        currentIndex += 1;
                        if (currentIndex < (int)line.size())
                            line[currentIndex].mMultiLineComment = inComment;
                    }
                } else {
                    if (firstChar && c == LanguageDef->PreprocChar)
                        withinPreproc = true;

                    if (c == '\"') {
                        withinString = true;
                        line[currentIndex].mMultiLineComment = inComment;
                    } else {
                        auto pred = [](const char &a, const Glyph &b) { return a == b.mChar; };
                        auto from = line.begin() + currentIndex;
                        auto &startStr = LanguageDef->CommentStart;
                        auto &singleStartStr = LanguageDef->SingleLineComment;

                        if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() &&
                            equals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred)) {
                            commentStartLine = currentLine;
                            commentStartIndex = currentIndex;
                        } else if (singleStartStr.size() > 0 && currentIndex + singleStartStr.size() <= line.size() && equals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred)) {
                            withinSingleLineComment = true;
                        }

                        inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

                        line[currentIndex].mMultiLineComment = inComment;
                        line[currentIndex].mComment = withinSingleLineComment;

                        auto &endStr = LanguageDef->CommentEnd;
                        if (currentIndex + 1 >= (int)endStr.size() &&
                            equals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred)) {
                            commentStartIndex = endIndex;
                            commentStartLine = endLine;
                        }
                    }
                }
                if (currentIndex < (int)line.size())
                    line[currentIndex].mPreprocessor = withinPreproc;
                currentIndex += UTF8CharLength(c);
                if (currentIndex >= (int)line.size()) {
                    currentIndex = 0;
                    ++currentLine;
                }
            } else {
                currentIndex = 0;
                ++currentLine;
            }
        }
        ShouldCheckComments = false;
    }

    if (ColorRangeMin < ColorRangeMax) {
        const int increment = (LanguageDef->Tokenize == nullptr) ? 10 : 10000;
        const int to = std::min(ColorRangeMin + increment, ColorRangeMax);
        ColorizeRange(ColorRangeMin, to);
        ColorRangeMin = to;

        if (ColorRangeMax == ColorRangeMin) {
            ColorRangeMin = std::numeric_limits<int>::max();
            ColorRangeMax = 0;
        }
        return;
    }
}

float TextEditor::TextDistanceToLineStart(const Coordinates &aFrom) const {
    auto &line = Lines[aFrom.mLine];
    float distance = 0.0f;
    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int colIndex = GetCharacterIndexR(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;) {
        if (line[it].mChar == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(TabSize) * spaceSize))) * (float(TabSize) * spaceSize);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it].mChar);
            char tempCString[7];
            int i = 0;
            for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
                tempCString[i] = line[it].mChar;

            tempCString[i] = '\0';
            distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
        }
    }

    return distance;
}

void TextEditor::EnsureCursorVisible(int aCursor) {
    if (aCursor == -1)
        aCursor = EditorState.GetLastAddedCursorIndex();

    if (!WithinRender) {
        ScrollToCursor = true;
        return;
    }

    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();

    auto height = ImGui::GetWindowHeight();
    auto width = ImGui::GetWindowWidth();

    auto top = 1 + (int)ceil(scrollY / CharAdvance.y);
    auto bottom = (int)ceil((scrollY + height) / CharAdvance.y);

    auto left = (int)ceil(scrollX / CharAdvance.x);
    auto right = (int)ceil((scrollX + width) / CharAdvance.x);

    auto pos = GetActualCursorCoordinates(aCursor);
    auto len = TextDistanceToLineStart(pos);

    if (pos.mLine < top)
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * CharAdvance.y));
    if (pos.mLine > bottom - 4)
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 4) * CharAdvance.y - height));
    if (len + TextStart < left + 4)
        ImGui::SetScrollX(std::max(0.0f, len + TextStart - 4));
    if (len + TextStart > right - 4)
        ImGui::SetScrollX(std::max(0.0f, len + TextStart + 4 - width));
}

int TextEditor::GetPageSize() const {
    auto height = ImGui::GetWindowHeight() - 20.0f;
    return (int)floor(height / CharAdvance.y);
}

TextEditor::UndoRecord::UndoRecord(
    const std::vector<UndoOperation> &aOperations,
    TextEditor::EditorStateT &aBefore,
    TextEditor::EditorStateT &aAfter
)
    : Operations(aOperations), Before(aBefore), After(aAfter) {
    for (const UndoOperation &o : Operations)
        assert(o.Start <= o.End);
}

void TextEditor::UndoRecord::Undo(TextEditor *aEditor) {
    for (int i = Operations.size() - 1; i > -1; i--) {
        const UndoOperation &operation = Operations[i];
        if (!operation.Text.empty()) {
            switch (operation.mType) {
                case UndoOperationType::Delete: {
                    auto start = operation.Start;
                    aEditor->InsertTextAt(start, operation.Text.c_str());
                    aEditor->Colorize(operation.Start.mLine - 1, operation.End.mLine - operation.Start.mLine + 2);
                    break;
                }
                case UndoOperationType::Add: {
                    aEditor->DeleteRange(operation.Start, operation.End);
                    aEditor->Colorize(operation.Start.mLine - 1, operation.End.mLine - operation.Start.mLine + 2);
                    break;
                }
            }
        }
    }

    aEditor->EditorState = Before;
    aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *aEditor) {
    for (int i = 0; i < Operations.size(); i++) {
        const UndoOperation &operation = Operations[i];
        if (!operation.Text.empty()) {
            switch (operation.mType) {
                case UndoOperationType::Delete: {
                    aEditor->DeleteRange(operation.Start, operation.End);
                    aEditor->Colorize(operation.Start.mLine - 1, operation.End.mLine - operation.Start.mLine + 1);
                    break;
                }
                case UndoOperationType::Add: {
                    auto start = operation.Start;
                    aEditor->InsertTextAt(start, operation.Text.c_str());
                    aEditor->Colorize(operation.Start.mLine - 1, operation.End.mLine - operation.Start.mLine + 1);
                    break;
                }
            }
        }
    }

    aEditor->EditorState = After;
    aEditor->EnsureCursorVisible();
}
