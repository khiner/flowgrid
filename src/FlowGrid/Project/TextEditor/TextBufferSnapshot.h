#pragma once

#include <vector>

#include "immer/vector.hpp"
#include "immer/flex_vector.hpp"

#include "LineChar.h"
#include "TextInputEdit.h"

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

struct CursorsSnapshot {
    std::vector<LineCharRange> Cursors;
    u32 LastAddedIndex{0};
};

struct TextBufferSnapshot {
    TextBufferLines Text;
    CursorsSnapshot Cursors, BeforeCursors;

    // If immer flex vectors provided a diff mechanism like its map does,
    // we wouldn't need this, and we could compute diffs across any two arbitrary snapshots.
    immer::vector<TextInputEdit> Edits;
};
