#pragma once

#include "immer/flex_vector.hpp"
#include "immer/vector.hpp"

#include "LineChar.h"
#include "TextInputEdit.h"

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

struct TextBufferData {
    using Cursor = LineCharRange;

    TextBufferLines Text{TextBufferLine{}};
    // If immer vectors provided a diff mechanism like its map does,
    // we could efficiently compute diffs across any two arbitrary text buffers, and we wouldn't need this.
    immer::vector<TextInputEdit> Edits{};
    immer::vector<LineCharRange> Cursors{{}};
    u32 LastAddedCursorIndex{0};

    bool operator==(const TextBufferData &) const = default;

    const Cursor &LastAddedCursor() const { return Cursors[LastAddedCursorIndex]; }

    bool Empty() const { return Text.empty() || (Text.size() == 1 && Text[0].empty()); }
    u32 LineCount() const { return Text.size(); }
    const TextBufferLine &GetLine(u32 li) const { return Text[li]; }
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
};
