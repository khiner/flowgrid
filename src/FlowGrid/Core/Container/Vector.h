#pragma once

#include "immer/flex_vector.hpp"

#include "Container.h"

template<typename T> struct Vector : Component {
    using ContainerT = immer::flex_vector<T>;

    Vector(ComponentArgs &&args) : Component(std::move(args)) {
        FieldIds.insert(Id);
        Refresh();
    }
    ~Vector() {
        Erase();
        FieldIds.erase(Id);
    }

    void Refresh() override {} // Not cached.
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Empty() const { return Get().empty(); }
    T operator[](u32 i) const { return Get()[i]; }
    u32 Size() const { return Get().size(); }

    ContainerT Get() const;
    void Set(const std::vector<T> &) const;
    void Set(size_t i, const T &) const;
    void Set(const std::unordered_map<size_t, T> &values) const {
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
