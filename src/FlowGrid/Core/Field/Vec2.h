#pragma once

#include "Bool.h"
#include "Field.h"
#include "Float.h"

struct ImVec2;

struct Vec2 : UIStateful {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(Stateful *parent, string_view path_leaf, string_view meta_str, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);

    operator ImVec2() const;

    Float X, Y;
    const char *Format;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;
    Vec2Linked(Stateful *parent, string_view path_leaf, string_view meta_str, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, bool linked = true, const char *fmt = nullptr);

    Prop(Bool, Linked, true);

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};
