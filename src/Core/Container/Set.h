#pragma once

#include "immer/set.hpp"

#include "Core/Component.h"

template<typename T> struct Set : Component {
    using Component::Component;
    using ContainerT = immer::set<T>;

    ~Set() {
        Erase(_S);
    }

    ContainerT Get() const;

    void Insert(TransientStore &, T) const;
    void Erase(TransientStore &, T) const;
    void Clear(TransientStore &) const;

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(TransientStore &, json &&) const override;
    json ToJson() const override;

    void Erase(TransientStore &) const override;
};
