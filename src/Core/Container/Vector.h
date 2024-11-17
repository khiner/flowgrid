#pragma once

#include "immer/flex_vector.hpp"

#include "Core/Component.h"

template<typename T> struct Vector : Component {
    using Component::Component;
    using ContainerT = immer::flex_vector<T>;

    ~Vector() {
        Erase();
    }

    ContainerT Get() const;
    T operator[](u32 i) const;

    void Set(const std::vector<T> &) const;
    void Set(size_t i, T) const;
    void PushBack(T) const;
    void PopBack() const;
    void Resize(size_t) const;
    void Clear() const;
    void Erase(size_t i) const;

    void Erase() const override;

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;
};
