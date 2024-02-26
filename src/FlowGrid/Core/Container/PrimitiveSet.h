#pragma once

#include <set>

#include "immer/set.hpp"

#include "Core/Action/Actionable.h"
#include "Core/Component.h"
#include "PrimitiveSetAction.h"

// todo unite with `Primitive` after moving `PrimitiveVector` to this pattern
template<IsPrimitive T> struct PrimitiveSet : Component, Actionable<typename Action::PrimitiveSet<T>::Any> {
    using typename Actionable<typename Action::PrimitiveSet<T>::Any>::ActionType;
    using ContainerT = immer::set<T>;

    PrimitiveSet(ComponentArgs &&args) : Component(std::move(args)) {
        FieldIds.insert(Id);
    }
    ~PrimitiveSet() {
        Erase();
        FieldIds.erase(Id);
    }
    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const Action::PrimitiveSet<T>::Insert &a) { Insert(a.value); },
                [this](const Action::PrimitiveSet<T>::Erase &a) { Erase_(a.value); },
            },
            action
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    void Refresh() override {} // Not cached.
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Contains(const T &) const;
    bool Empty() const;

    void Insert(const T &) const;
    void Erase_(const T &) const;

    ContainerT Get() const;
    void Set(const std::set<T> &) const;

    bool Exists() const; // Check if exists in store.
    void Erase() const override;
};
