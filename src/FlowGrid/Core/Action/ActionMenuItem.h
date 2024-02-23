#pragma once

#include "ActionableProducer.h"
#include "Core/MenuItemDrawable.h"
#include "Helper/Variant.h"

#include "imgui.h"

template<typename ActionType> struct ActionMenuItem : MenuItemDrawable {
    using EnqueueFn = ActionProducer<ActionType>::EnqueueFn;
    using ProducerOrQ = std::variant<std::reference_wrapper<const ActionableProducer<ActionType>>, EnqueueFn>;

    ActionMenuItem(const Actionable<ActionType> &actionable, EnqueueFn q, ActionType &&action = {}, std::string_view shortcut = "")
        : Actionable(actionable), Q(std::move(q)), Action(std::move(action)), Shortcut(shortcut) {}
    ActionMenuItem(const ActionableProducer<ActionType> &actionable, ActionType &&action = {}, std::string_view shortcut = "")
        : Actionable(actionable), Q(actionable), Action(std::move(action)), Shortcut(shortcut) {}
    ~ActionMenuItem() override = default;

    void MenuItem() const override {
        if (ImGui::MenuItem(Action.GetMenuLabel().c_str(), Shortcut.c_str(), false, Actionable.CanApply(Action))) {
            auto action = ActionType{Action}; // Make a copy.
            std::visit(
                Match{
                    [&action](const ActionProducer<ActionType> &producer) { producer.Q(std::move(action)); },
                    [&action](const EnqueueFn &q) { q(std::move(action)); },
                },
                Q
            );
        }
    }

    const Actionable<ActionType> &Actionable;
    ProducerOrQ Q;
    const ActionType Action{};
    std::string Shortcut;
};
