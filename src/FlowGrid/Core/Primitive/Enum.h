#pragma once

#include "Core/Field/Field.h"

struct Enum : TypedField<int>, MenuItemDrawable {
    Enum(ComponentArgs &&, std::vector<string> names, int value = 0);
    Enum(ComponentArgs &&, std::function<const string(int)> get_name, int value = 0);

    void Render(const std::vector<int> &options) const;
    void MenuItem() const override;

    const std::vector<string> Names;

private:
    void Render() const override;
    string OptionName(const int option) const;

    const std::optional<std::function<const string(int)>> GetName{};
};
