#pragma once

using u32 = unsigned int;

// For now, this just holds what's needed for column calculation, and temporarily a global singleton.
struct TextBufferStyle {
    u32 NumTabSpaces{4};

    u32 NumTabSpacesAtColumn(u32 column) const { return NumTabSpaces - (column % NumTabSpaces); }
    u32 NextTabstop(u32 column) const { return ((column / NumTabSpaces) + 1) * NumTabSpaces; }
};

extern TextBufferStyle GTextBufferStyle;
