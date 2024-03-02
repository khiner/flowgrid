#pragma once

#include "Core/Component.h"
#include "Core/Action/Actionable.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"
#include "Vec2Action.h"

struct ImVec2;

// todo next up: Use `Float`/`Bool`, to avoid manual path construction.
//   (towards IDs instead of paths in store)
struct Vec2 : Component, Actionable<Action::Vec2::Any> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(ComponentArgs &&, std::pair<float, float> &&value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);
    ~Vec2() = default;

    operator ImVec2() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void SetX(float x) const { X.Set(x); }
    void SetY(float y) const { Y.Set(y); }
    void Set(const std::pair<float, float> &value) const {
        SetX(value.first);
        SetY(value.second);
    }

    Float X, Y;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;

    std::pair<float, float> Value;
};

struct Vec2Linked : Vec2 {
    // Defaults to linked.
    Vec2Linked(ComponentArgs &&, std::pair<float, float> &&value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);
    Vec2Linked(ComponentArgs &&, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt = nullptr);
    ~Vec2Linked() = default;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void SetLinked(bool linked) const { Linked.Set(linked); }

    Bool Linked;

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};
