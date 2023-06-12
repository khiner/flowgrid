#pragma once

#include "Field.h"

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : TypedField<int>, MenuItemDrawable {
    struct Item {
        Item(const char *name_and_help);

        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(Component *parent, string_view path_leaf, string_view meta_str, std::vector<Item> items, int value = 0);

    void MenuItem() const override;

    const std::vector<Item> Items;

private:
    void Render() const override;
};
