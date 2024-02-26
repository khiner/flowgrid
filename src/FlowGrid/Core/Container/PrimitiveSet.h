#pragma once

#include <set>

#include "immer/set.hpp"

#include "Container.h"
#include "Core/Action/Actionable.h"
#include "PrimitiveSetAction.h"

template<IsPrimitive T> struct PrimitiveSet : Container, Actionable<typename Action::PrimitiveSet<T>::Any> {
    using Container::Container;
    using typename Actionable<typename Action::PrimitiveSet<T>::Any>::ActionType;
    using ContainerT = immer::set<T>;

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

    void Refresh() override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Contains(const T &) const;
    bool Empty() const;

    void Insert(const T &) const;
    void Erase_(const T &) const;

    ContainerT Get() const;
    void Set(const std::set<T> &) const;
};
