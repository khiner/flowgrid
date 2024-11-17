#pragma once

#include "Core/Component.h"

template<typename T> struct Primitive : Component {
    Primitive(ComponentArgs &&, T value = {});
    virtual ~Primitive();

    json ToJson() const override;
    void SetJson(json &&) const override;

    void Refresh() override;

    operator T() const { return Value; }
    bool operator==(T value) const { return Value == value; }

    void IssueSet(T) const; // Queue a set action.
    void Set(T) const; // Update the store
    void Set_(T); // Updates both store and cached value.

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void Erase() const override;

protected:
    T Value;
};
