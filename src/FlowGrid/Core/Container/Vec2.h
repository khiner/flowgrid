#pragma once

#include "Container.h"
#include "Core/Action/Actionable.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"
#include "Vec2Action.h"

struct ImVec2;

struct Vec2 : Container, Actionable<Action::Vec2::Any> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(ComponentArgs &&, std::pair<float, float> &&value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);
    ~Vec2();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void SetJson(json &&) const override;
    json ToJson() const override;

    void Refresh() override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    operator ImVec2() const;

    void SetX(float) const;
    void SetY(float) const;
    void Set(const std::pair<float, float> &) const;

    float X() const noexcept { return Value.first; }
    float Y() const noexcept { return Value.second; }

    const float Min, Max;
    const char *Format;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;

    std::pair<float, float> Value;
};

struct Vec2Linked : Vec2 {
    // Defaults to linked.
    Vec2Linked(ComponentArgs &&, std::pair<float, float> &&value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);
    Vec2Linked(ComponentArgs &&, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt = nullptr);
    ~Vec2Linked();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void Refresh() override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetLinked(bool) const;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Linked;

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};
