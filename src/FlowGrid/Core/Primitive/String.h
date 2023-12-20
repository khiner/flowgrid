#pragma once

#include "Core/Action/Actionable.h"
#include "Primitive.h"
#include "StringAction.h"

struct String : Primitive<string>, Actionable<Action::Primitive::String::Any> {
    String(ComponentArgs &&, string_view value = "");

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void IssueSet(const string &value) const override { Action::Primitive::String::Set{Path, value}.q(); }

    void Render(const std::vector<string> &options) const;

private:
    void Render() const override;
};
