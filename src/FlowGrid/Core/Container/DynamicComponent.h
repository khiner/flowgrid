#pragma once

#include "Core/Field/Field.h"
#include "Core/Primitive/Bool.h"

#include "imgui_internal.h"

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
        ComponentContainerFields.erase(Id);
        ComponentContainerAuxiliaryFields.erase(HasValue.Id);
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

    // Gets called when its `HasValue` field is changed.
    void Refresh() override {
        // if (ChangedPaths.contains(Id) && !ChangedPaths.at(Id).second.empty()) Toggle();
        if (IsChanged()) {
            if (HasValue) Create();
            else Reset();
        }
    }

    void Erase() const override {
        if (Value) Value->Erase();
    }

    void RefreshFromJson(const json &j) override {
        auto &&flattened = std::move(j).flatten();
        for (auto &&[key, value] : flattened.items()) {
            const StorePath path = key; // Already relative.
            auto it = path.begin();
            it++; // First segment is just a "/".
            if (it->string() == HasValue.PathSegment) {
                HasValue.SetJson(std::move(value));
                HasValue.Refresh();
            } else if (!Value) {
                Create(); // At least one non-auxiliary descendent field is set, so create the component.
                return;
            }
        }

        if (Value) Reset(); // No non-auxiliary descendent fields are set, so destroy the component.
    }

    void Render() const override {
        HasValue.Render(ImGuiLabel);
    }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        if (!Value) {
            if (auto_select && IsChanged() && ImGui::IsItemVisible()) {
                ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
            }
            ImGui::TextUnformatted(std::format("{} (empty)", Name).c_str());
            return;
        }

        if (TreeNode(Name)) {
            Value->RenderValueTree(annotate, auto_select);
            ImGui::TreePop();
        }
    }

    Prop(Bool, HasValue, false);

private:
    std::unique_ptr<ComponentType> Value;
};
