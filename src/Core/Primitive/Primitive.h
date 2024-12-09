#pragma once

#include "Core/Component.h"

struct TransientStore;

template<typename T> struct Primitive : Component {
    Primitive(ComponentArgs &&, T value = {});
    virtual ~Primitive();

    json ToJson() const override;
    void SetJson(TransientStore &, json &&) const override;

    void Refresh() override;

    operator T() const { return Value; }
    bool operator==(T value) const { return Value == value; }

    void Set(TransientStore &, T) const; // Update the store
    void Set_(TransientStore &, T); // Updates both store and cached value.
    void IssueSet(T) const; // Queue a set action.
    void Erase(TransientStore &) const override;

    void RenderValueTree(bool annotate, bool auto_select) const override;

protected:
    T Value;
};
