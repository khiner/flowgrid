#pragma once

#include "Core/Field/Field.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"
#include "Vec2Action.h"

struct ImVec2;

struct Vec2 : Field, Actionable<Action::Vec2::Any> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(ComponentArgs &&, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RefreshValue() override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    operator ImVec2() const;

    void Set(const std::pair<float, float> &) const;

    inline float X() const { return Value.first; }
    inline float Y() const { return Value.second; }

    const float Min, Max;
    const char *Format;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;

    std::pair<float, float> Value;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;
    Vec2Linked(ComponentArgs &&, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, bool linked = true, const char *fmt = nullptr);

    Prop(Bool, Linked, true);

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};
