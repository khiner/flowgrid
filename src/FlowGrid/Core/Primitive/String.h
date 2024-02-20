#pragma once

#include "Core/Action/Actionable.h"
#include "Primitive.h"
#include "StringAction.h"

struct String : Primitive<string>, Actionable<Action::Primitive::String::Any> {
    String(ComponentArgs &&, string_view value = "");
    String(ComponentArgs &&, fs::path value);

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }
    operator fs::path() const { return Value; }

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void Render(const std::vector<string> &options) const;

private:
    void Render() const override;
};
