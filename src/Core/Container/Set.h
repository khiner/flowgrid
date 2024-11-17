#pragma once

#include "immer/set.hpp"

#include "Core/Component.h"

template<typename T> struct Set : Component {
    using Component::Component;
    using ContainerT = immer::set<T>;

    ~Set() {
        Erase();
    }

    ContainerT Get() const;

    void Insert(T) const;
    void Erase(T) const;
    void Clear() const;

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void SetJson(json &&) const override;
    json ToJson() const override;

    void Erase() const override;
};
