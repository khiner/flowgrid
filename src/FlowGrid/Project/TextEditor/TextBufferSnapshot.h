#pragma once

#include "immer/flex_vector.hpp"
#include "immer/vector.hpp"

#include "LineChar.h"
#include "TextInputEdit.h"

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

struct CursorsSnapshot {
    immer::vector<LineCharRange> Cursors;
    u32 LastAddedCursorIndex{0};
};

struct TextBufferSnapshot {
    TextBufferLines Text;
    CursorsSnapshot Cursors, BeforeCursors;

    // If immer vectors provided a diff mechanism like its map does,
    // we could efficiently compute diffs across any two arbitrary text buffers, and we wouldn't need this.
    immer::vector<TextInputEdit> Edits;
};
