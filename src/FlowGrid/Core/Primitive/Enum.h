#pragma once

#include "Core/Action/Actionable.h"
#include "EnumAction.h"
#include "Primitive.h"

struct Enum : Primitive<int>, Actionable<Action::Primitive::Enum::Any>, MenuItemDrawable {
    Enum(ComponentArgs &&, std::vector<string> names, int value = 0);
    Enum(ComponentArgs &&, std::function<string(int)> get_name, int value = 0);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void Render(const std::vector<int> &options) const;
    void MenuItem() const override;

    const std::vector<string> Names;

private:
    void Render() const override;
    string OptionName(const int option) const;

    const std::optional<std::function<const string(int)>> GetName{};
};
