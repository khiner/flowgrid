#pragma once

#include "Core/Component/Window.h"
#include "Field.h"
#include "String.h"

// struct MultilineString : TypedField<string>, Window {
struct MultilineString : Window {
    MultilineString(Component *parent, string_view path_leaf, string_view meta_str, string_view value = "");

    bool operator==(const std::string &value) const { return Value == value; }
    operator std::string() const { return Value; }
    operator string_view() const { return Value; };
    operator bool() const { return Value; }

    DefineWindow(Metrics);

    Prop(String, Value);
    Prop_(Metrics, Metrics, "Editor metrics");

private:
    void Render() const override;
};
