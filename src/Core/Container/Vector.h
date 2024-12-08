#pragma once

#include "immer/flex_vector.hpp"

#include "Core/Component.h"

template<typename T> struct Vector : Component {
    using Component::Component;
    using ContainerT = immer::flex_vector<T>;

    ~Vector() {
        Erase(_S);
    }

    ContainerT Get() const;
    T operator[](u32 i) const;

    void Set(TransientStore &, const std::vector<T> &) const;
    void Set(TransientStore &, size_t i, T) const;
    void PushBack(TransientStore &, T) const;
    void PopBack(TransientStore &) const;
    void Resize(TransientStore &, size_t) const;
    void Clear(TransientStore &) const;
    void Erase(TransientStore &, size_t i) const;

    void Erase(TransientStore &) const override;

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(TransientStore &, json &&) const override;
    json ToJson() const override;
};
