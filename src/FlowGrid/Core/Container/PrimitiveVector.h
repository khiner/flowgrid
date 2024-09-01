#pragma once

#include "immer/flex_vector.hpp"

#include "Container.h"
#include "Core/Action/Actionable.h"
#include "PrimitiveVectorAction.h"

template<typename T> struct PrimitiveVector : Component, Actionable<typename Action::PrimitiveVector<T>::Any> {
    // `ActionType` is a type alias in `Actionable`, but it is not accessible here.
    // `Actionable` is templated on `Action::PrimitiveVector::Type<T>::type`, which is a dependent type (it depends on `T`),
    // and base class members that use dependent template types are not visible in subclasses at compile time.
    using typename Actionable<typename Action::PrimitiveVector<T>::Any>::ActionType;
    using ContainerT = immer::flex_vector<T>;

    void Apply(const ActionType &action) const override {
        std::visit(
            Match{
                [this](const Action::PrimitiveVector<T>::Set &a) { Set(a.i, a.value); },
            },
            action
        );
    }

    PrimitiveVector(ComponentArgs &&args) : Component(std::move(args)) {
        FieldIds.insert(Id);
        Refresh();
    }
    ~PrimitiveVector() {
        Erase();
        FieldIds.erase(Id);
    }

    bool CanApply(const ActionType &) const override { return true; }

    void Refresh() override {} // Not cached.
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Empty() const { return Get().empty(); }
    T operator[](u32 i) const { return Get()[i]; }
    u32 Size() const { return Get().size(); }

    ContainerT Get() const;
    void Set(ContainerT) const;
    void Set(const std::vector<T> &) const;
    void Set(size_t i, const T &) const;
    void Set(const std::vector<std::pair<size_t, T>> &values) const {
        for (const auto &[i, value] : values) Set(i, value);
    }

    void PushBack(const T &) const;
    void PopBack() const;
    void Resize(size_t) const;
    void Clear() const;
    bool Exists() const; // Check if exists in store.
    void Erase() const override;

    size_t IndexOf(const T &) const;
    bool Contains(const T &value) const { return IndexOf(value) != Size(); }
    void Erase(size_t i) const;
};
