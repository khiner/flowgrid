#pragma once

#include "Core/Action/Actionable.h"
#include "Primitive.h"
#include "StringAction.h"

struct String : Primitive<std::string>, Actionable<Action::Primitive::String::Any> {
    String(ComponentArgs &&, std::string_view value = "");
    String(ComponentArgs &&, fs::path value);

    operator bool() const { return !Value.empty(); }
    operator std::string_view() const { return Value; }
    operator fs::path() const { return Value; }

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void Render(const std::vector<std::string> &options) const;

private:
    void Render() const override;
};
