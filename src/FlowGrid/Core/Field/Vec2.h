#pragma once

#include "Bool.h"
#include "Field.h"
#include "Float.h"
#include "Vec2Action.h"

struct ImVec2;

struct Vec2 : ExtendedPrimitiveField, Actionable<Action::Vec2::Any> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(ComponentArgs &&, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);

    operator ImVec2() const;

    const float Min, Max;
    const char *Format;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RefreshValue() override;

    inline float X() const { return Value.first; }
    inline float Y() const { return Value.second; }

    FieldValue GetValue() const override { return Value; }

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
