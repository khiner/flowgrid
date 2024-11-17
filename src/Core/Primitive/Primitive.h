#pragma once

#include "Core/Component.h"

template<typename T> struct Primitive : Component {
    Primitive(ComponentArgs &&, T value = {});
    virtual ~Primitive();

    json ToJson() const override;
    void SetJson(json &&) const override;

    // Refresh the cached value based on the main store. Should be called for each affected field after a state change.
    void Refresh() override;

    operator T() const { return Value; }
    bool operator==(const T &value) const { return Value == value; }

    void IssueSet(const T &) const; // Queue a set action.
    void Set(T) const; // Update the store
    void Set_(T); // Updates both store and cached value.

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void Erase() const override;

protected:
    T Value;
};
