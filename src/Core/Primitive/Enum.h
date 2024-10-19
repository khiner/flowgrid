#pragma once

#include "Primitive.h"

struct Enum : Primitive<int>, MenuItemDrawable {
    Enum(ComponentArgs &&, std::vector<std::string> names, int value = 0);
    Enum(ComponentArgs &&, std::function<std::string(int)> get_name, int value = 0);

    void Render(const std::vector<int> &options) const;
    void MenuItem() const override;

    const std::vector<std::string> Names;

private:
    void Render() const override;
    std::string OptionName(const int option) const;

    const std::optional<std::function<const std::string(int)>> GetName{};
};
