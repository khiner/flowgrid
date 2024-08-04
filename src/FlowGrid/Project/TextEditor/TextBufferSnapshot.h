#pragma once

#include <vector>

#include "immer/vector.hpp"
#include "immer/flex_vector.hpp"

#include "LineChar.h"
#include "TextInputEdit.h"

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

struct TextBufferSnapshot {
    TextBufferLines Text;
    std::vector<LineCharRange> Cursors, BeforeCursors;
    u32 LastAddedIndex{0}, BeforeLastAddedIndex{0};

    // If immer flex vectors provided a diff mechanism like its map does,
    // we wouldn't need this, and we could compute diffs across any two arbitrary snapshots.
    immer::vector<TextInputEdit> Edits;

    bool operator==(const TextBufferSnapshot &) const = default;
    bool operator!=(const TextBufferSnapshot &) const = default;
};
