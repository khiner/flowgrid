#pragma once

#include "immer/flex_vector.hpp"
#include "immer/vector.hpp"

#include "LineChar.h"
#include "TextBufferStyle.h"
#include "TextInputEdit.h"

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

// Represents a character coordinate from the user's point of view,
// i. e. consider a uniform grid (assuming fixed-width font) on the screen as it is rendered, and each cell has its own coordinate, starting from 0.
// Tabs are counted as [1..NumTabSpaces] u32 empty spaces, depending on how many space is necessary to reach the next tab stop.
// For example, `Coords{1, 5}` represents the character 'B' in the line "\tABC", when NumTabSpaces = 4, since it is rendered as "    ABC".
struct TextBufferCoords {
    u32 L{0}, C{0}; // Line, Column

    auto operator<=>(const TextBufferCoords &o) const {
        if (auto cmp = L <=> o.L; cmp != 0) return cmp;
        return C <=> o.C;
    }
    bool operator==(const TextBufferCoords &) const = default;
    bool operator!=(const TextBufferCoords &) const = default;

    TextBufferCoords operator-(const TextBufferCoords &o) const { return {L - o.L, C - o.C}; }
    TextBufferCoords operator+(const TextBufferCoords &o) const { return {L + o.L, C + o.C}; }
};

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
constexpr u32 UTF8CharLength(char ch) {
    if ((ch & 0xFE) == 0xFC) return 6;
    if ((ch & 0xFC) == 0xF8) return 5;
    if ((ch & 0xF8) == 0xF0) return 4;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xE0) == 0xC0) return 2;
    return 1;
}

struct TextBufferData {
    using Cursor = LineCharRange;
    using Line = TextBufferLine;
    using Coords = TextBufferCoords;

    TextBufferLines Text{Line{}};
    // If immer vectors provided a diff mechanism like its map does,
    // we could efficiently compute diffs across any two arbitrary text buffers, and we wouldn't need this.
    immer::vector<TextInputEdit> Edits{};
    immer::vector<LineCharRange> Cursors{{}};
    u32 LastAddedCursorIndex{0};

    bool operator==(const TextBufferData &) const = default;

    const Cursor &LastAddedCursor() const { return Cursors[LastAddedCursorIndex]; }

    bool Empty() const { return Text.empty() || (Text.size() == 1 && Text[0].empty()); }
    u32 LineCount() const { return Text.size(); }
    const Line &GetLine(u32 li) const { return Text[li]; }
    LineChar GetCursorPosition() const { return Cursors.back().LC(); }
    LineChar CheckedNextLineBegin(u32 li) const { return li < Text.size() - 1 ? LineChar{li + 1, 0} : EndLC(); }
    u32 GetLineMaxCharIndex(u32 li) const { return Text[li].size(); }
    LineChar LineMaxLC(u32 li) const { return {li, GetLineMaxCharIndex(li)}; }

    static LineChar BeginLC() { return {0, 0}; }
    LineChar EndLC() const { return LineMaxLC(Text.size() - 1); }

    u32 ToByteIndex(LineChar) const;
    u32 EndByteIndex() const { return ToByteIndex(EndLC()); }

    Cursor Clamped(LineChar start, LineChar end) const {
        const auto begin_lc = BeginLC(), end_lc = EndLC();
        return {std::clamp(start, begin_lc, end_lc), std::clamp(end, begin_lc, end_lc)};
    }

    std::string GetText(LineChar start, LineChar end) const;
    std::string GetText(const Cursor &c) const { return GetText(c.Min(), c.Max()); }
    std::string GetText() const { return GetText(BeginLC(), EndLC()); }
    std::string GetSelectedText() const; // Copy ranged cursors' text, separated by newlines.

    bool AnyCursorsRanged() const;
    bool AllCursorsRanged() const;
    bool AnyCursorsMultiline() const;

    void AssertCursorsSorted() const {
        for (u32 j = 1; j < Cursors.size(); ++j) assert(Cursors[j - 1] <= Cursors[j]);
    }

    TextBufferData SetText(const std::string &) const;

    // Column calculation functions (dependent on tab width).
    static std::pair<u32, u32> NextCharIndexAndColumn(const Line &line, u32 ci, u32 column) {
        const char ch = line[ci];
        return {ci + UTF8CharLength(ch), ch == '\t' ? GTextBufferStyle.NextTabstop(column) : column + 1};
    }

    static u32 GetCharIndex(const Line &line, u32 column) {
        u32 ci = 0, column_i = 0;
        while (ci < line.size() && column_i < column) std::tie(ci, column_i) = NextCharIndexAndColumn(line, ci, column_i);
        return ci;
    }

    static u32 GetColumn(const Line &line, u32 char_index) {
        u32 ci = 0, column = 0;
        while (ci < char_index && ci < line.size()) std::tie(ci, column) = NextCharIndexAndColumn(line, ci, column);
        return column;
    }
    static u32 GetFirstVisibleCharIndex(const Line &line, u32 first_visible_column) {
        u32 ci = 0, column = 0;
        while (column < first_visible_column && ci < line.size()) std::tie(ci, column) = NextCharIndexAndColumn(line, ci, column);
        return column > first_visible_column && ci > 0 ? ci - 1 : ci;
    }

    static u32 GetLineMaxColumn(const Line &line) {
        u32 ci = 0, column = 0;
        while (ci < line.size()) std::tie(ci, column) = NextCharIndexAndColumn(line, ci, column);
        return column;
    }
    static u32 GetLineMaxColumn(const Line &line, u32 limit) {
        u32 ci = 0, column = 0;
        while (ci < line.size() && column < limit) std::tie(ci, column) = NextCharIndexAndColumn(line, ci, column);
        return column;
    }

    u32 NumStartingSpaceColumns(u32 li) const {
        const auto &line = Text[li];
        u32 ci = 0, column = 0;
        while (ci < line.size() && isblank(line[ci])) std::tie(ci, column) = NextCharIndexAndColumn(line, ci, column);
        return column;
    }
    u32 GetCharIndex(Coords coords) const { return GetCharIndex(Text[coords.L], coords.C); }
    u32 GetColumn(LineChar lc) const { return GetColumn(Text[lc.L], lc.C); }
    Coords ToCoords(LineChar lc) const { return {lc.L, GetColumn(Text[lc.L], lc.C)}; }
    LineChar ToLineChar(Coords coords) const { return {coords.L, GetCharIndex(std::move(coords))}; }
};
