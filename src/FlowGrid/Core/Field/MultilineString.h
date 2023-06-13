#pragma once

#include "Field.h"
#include "String.h"

// struct MultilineString : TypedField<string>, Window {
struct MultilineString : Component, Drawable {
    MultilineString(ComponentArgs &&, string_view value = "");

    bool operator==(const std::string &value) const { return Value == value; }
    operator std::string() const { return Value; }
    operator string_view() const { return Value; };
    operator bool() const { return Value; }

    struct Metrics : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    Prop(String, Value);
    Prop_(Metrics, Metrics, "Editor metrics");

private:
    void Render() const override;
};
