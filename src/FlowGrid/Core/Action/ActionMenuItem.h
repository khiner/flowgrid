#pragma once

#include "ActionableProducer.h"
#include "Core/MenuItemDrawable.h"

#include "imgui.h"

template<typename ActionType> struct ActionMenuItem : MenuItemDrawable {
    ActionMenuItem(const Actionable<ActionType> &actionable, ActionProducer<ActionType>::EnqueueFn q, ActionType &&action = {})
        : Actionable(actionable), Q(std::move(q)), Action(std::move(action)) {}
    ActionMenuItem(const ActionableProducer<ActionType> &actionable, ActionType &&action = {})
        : Actionable(actionable), Q([&actionable](auto &&a) -> bool { return actionable.Q(a); }), Action(std::move(action)) {}

    void MenuItem() const override {
        if (ImGui::MenuItem(Action.GetMenuLabel().c_str(), Action.GetShortcut().c_str(), false, Actionable.CanApply(Action))) {
            Q(ActionType{Action});
        }
    }

    const Actionable<ActionType> &Actionable;
    ActionProducer<ActionType>::EnqueueFn Q;
    const ActionType Action{};
};
