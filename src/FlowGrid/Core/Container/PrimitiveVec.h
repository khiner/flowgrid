#pragma once

#include "immer/vector.hpp"

#include "Container.h"
#include "Core/Action/Actionable.h"
#include "PrimitiveVectorAction.h"

template<typename T> struct PrimitiveVec : Component, Actionable<typename Action::PrimitiveVector<T>::Any> {
    // `ActionType` is a type alias in `Actionable`, but it is not accessible here.
    // `Actionable` is templated on `Action::PrimitiveVector::Type<T>::type`, which is a dependent type (it depends on `T`),
    // and base class members that use dependent template types are not visible in subclasses at compile time.
    using typename Actionable<typename Action::PrimitiveVector<T>::Any>::ActionType;
    using ContainerT = immer::vector<T>;

    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const Action::PrimitiveVector<T>::Set &a) { Set(a.i, a.value); },
            },
            action
        );
    }

    PrimitiveVec(ComponentArgs &&args) : Component(std::move(args)) {
        FieldIds.insert(Id);
        Refresh();
    }
    ~PrimitiveVec() {
        Erase();
        FieldIds.erase(Id);
    }

    bool CanApply(const ActionType &) const override { return true; }

    void Refresh() override {} // Not cached.
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Empty() const;
    T operator[](u32 i) const;

    u32 Size() const;

    ContainerT Get() const;
    void Set(const std::vector<T> &) const;
    void Set(size_t i, const T &) const;
    void Set(const std::vector<std::pair<size_t, T>> &) const;
    void PushBack(const T &) const;
    void PopBack() const;
    void Resize(u32) const;
    bool Exists() const; // Check if exists in store.
    void Erase() const override;
};