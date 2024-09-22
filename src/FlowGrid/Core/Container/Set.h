#pragma once

#include "immer/set.hpp"

#include "Core/Component.h"

template<typename T> struct Set : Component {
    using Component::Component;
    using ContainerT = immer::set<T>;

    ~Set() {
        Erase();
    }

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    bool Contains(const T &value) const { return Get().count(value); }
    bool Empty() const { return Get().empty(); }

    void Insert(const T &) const;
    void Erase(const T &) const;
    void Clear() const;

    ContainerT Get() const;

    bool Exists() const; // Check if exists in store.
    void Erase() const override;
};
