#pragma once

#include "Core/Field/Field.h"
#include "Core/Primitive/Bool.h"

/*
A component that is created/destroyed dynamically.
Think of it like a store-backed `std::unique_ptr<ComponentType>`.
*/
template<typename ComponentType> struct DynamicComponent : Field {
    DynamicComponent(ComponentArgs &&args) : Field(std::move(args)) {
        ComponentContainerFields.insert(Id);
        ComponentContainerAuxiliaryFields.insert(HasValue.Id);
    }
    ~DynamicComponent() {
        ComponentContainerAuxiliaryFields.erase(HasValue.Id);
        ComponentContainerFields.erase(Id);
    }

    operator bool() const { return bool(Value); }
    auto operator->() const { return Value.get(); }

    inline void Create() {
        Value = std::make_unique<ComponentType>(ComponentArgs{this, "Value"});
    }

    inline void Reset() { Value.reset(); }

    void Toggle() {
        if (Value) Reset();
        else Create();
    }

    void Refresh() override {
        if (HasValue && !Value) Create();
        else if (!HasValue && Value) Reset();
    }

    void Erase() const override {
        if (Value) Value->Erase();
    }

    void Render() const override {
        HasValue.Render(ImGuiLabel);
    }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        if (!Value) {
            if (auto_select) ScrollToChanged();
            TextUnformatted(std::format("{} (empty)", Name));
            return;
        }

        if (TreeNode(Name)) {
            Value->RenderValueTree(annotate, auto_select);
            TreePop();
        }
    }

    Prop(Bool, HasValue, false);

private:
    std::unique_ptr<ComponentType> Value;
};
