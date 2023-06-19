#pragma once

#include "Core/Field/Field.h"

struct MultilineString : TypedField<string> {
    MultilineString(ComponentArgs &&, string_view value = "");

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }

    struct Metrics : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    Prop_(Metrics, Metrics, "Editor metrics");

private:
    void Render() const override;
};
