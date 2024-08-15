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
};
