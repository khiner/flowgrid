#pragma once

#include "Core/Field/Field.h"
#include "MultilineStringAction.h"

struct MultilineString : TypedField<string>, Actionable<Action::MultilineString::Any> {
    MultilineString(ComponentArgs &&, string_view value = "");

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    struct Metrics : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    Prop_(Metrics, Metrics, "Editor metrics");

private:
    void Render() const override;
};
