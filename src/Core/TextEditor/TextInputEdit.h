#pragma once

using u32 = unsigned int;

/**
Holds the byte parts of `TSInputEdit` (not the points).
tree-sitter API functions generally handle only having bytes populated.
(E.g. see https://github.com/tree-sitter/tree-sitter/issues/445)
`StartByte`: Start position of the text change.
`OldEndByte`: End position of the original text before the change.
  - For insertion, same as `start`.
  - For replacement, where the replaced text ended.
  - For deletion, where the deleted text ended.
`NewEndByte`: End position of the new text after the change.
  - For insertion or replacement, where the new text ends.
  - For deletion, same as `start`.
**/
struct TextInputEdit {
    u32 StartByte{0}, OldEndByte{0}, NewEndByte{0};

    TextInputEdit Invert() const { return TextInputEdit{StartByte, NewEndByte, OldEndByte}; }
    bool IsInsert() const { return StartByte == OldEndByte; }
    bool IsDelete() const { return StartByte == NewEndByte; }

    bool operator==(const TextInputEdit &) const = default;
    bool operator!=(const TextInputEdit &) const = default;
    auto operator<=>(const TextInputEdit &o) const {
        if (auto cmp = StartByte <=> o.StartByte; cmp != 0) return cmp;
        if (auto cmp = OldEndByte <=> o.OldEndByte; cmp != 0) return cmp;
        return NewEndByte <=> o.NewEndByte;
    }
};
