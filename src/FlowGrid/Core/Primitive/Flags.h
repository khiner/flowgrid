#pragma once

#include "Core/Action/Actionable.h"
#include "FlagsAction.h"
#include "PrimitiveField.h"

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : PrimitiveField<int>, Actionable<Action::Primitive::Flags::Any>, MenuItemDrawable {
    struct Item {
        Item(const char *name_and_help);
        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(ComponentArgs &&, std::vector<Item> items, int value = 0);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void MenuItem() const override;

    const std::vector<Item> Items;

private:
    void Render() const override;
};
