#pragma once

#include "Core/Primitive/Bool.h"

/*
A component that is created/destroyed dynamically.
Think of it like a store-backed `std::unique_ptr<ComponentType>`.
*/
template<typename ComponentType> struct Optional : Component {
    Optional(ComponentArgs &&args) : Component(std::move(args)) {
        ContainerIds.insert(Id);
        ContainerAuxiliaryIds.insert(HasValue.Id);
        Refresh();
    }
    ~Optional() {
        ContainerAuxiliaryIds.erase(HasValue.Id);
        ContainerIds.erase(Id);
    }

    operator bool() const { return bool(Value); }
    auto operator->() const { return Value.get(); }
    inline auto Get() const { return Value.get(); }

    void Refresh() override {
        HasValue.Refresh();
        if (HasValue && !Value) {
            Value = std::make_unique<ComponentType>(ComponentArgs{this, "Value"});
            Value->Refresh();
        } else if (!HasValue && Value) {
            Reset();
        }
    }

    void Toggle_() {
        HasValue.Toggle_();
        Refresh();
    }

    void IssueToggle() const {
        HasValue.IssueToggle();
    }

    void Erase() const override {
        HasValue.Set(false);
        if (Value) Value->Erase();
    }

    void Reset() { Value.reset(); }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        if (!Value || Value->Children.empty()) {
            if (auto_select) ScrollToChanged();
            TextUnformatted(std::format("{} (empty)", Name));
            return;
        }

        if (TreeNode(Name, false, nullptr, false, auto_select)) {
            for (const auto *value_child : Value->Children) {
                value_child->RenderValueTree(annotate, auto_select);
            }
            TreePop();
        }
    }

    Prop(Bool, HasValue, false);

private:
    std::unique_ptr<ComponentType> Value;
};
