#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Field/Field.h"

template<IsPrimitive T> struct PrimitiveField : Field {
    PrimitiveField(ComponentArgs &&args, T value = {}) : Field(std::move(args)), Value(value) {
        Set(value);
    }
    ~PrimitiveField() {
        Erase();
    }

    void SetJson(json &&) const override;
    json ToJson() const override;

    // Refresh the cached value based on the main store. Should be called for each affected field after a state change.
    void Refresh() override { Value = Get(); }

    operator T() const { return Value; }
    bool operator==(const T &value) const { return Value == value; }

    T Get() const; // Get from store.

    // Non-mutating set. Only updates store. Used during action application.
    void Set(const T &) const;
    void Set(T &&) const;

    // Mutating set. Updates both store and cached value.
    // Should only be used during initialization and side effect handling pass.
    void Set_(const T &value) {
        Set(value);
        Value = value;
    }

    void RenderValueTree(bool annotate, bool auto_select) const override;

    void Erase() const override;

protected:
    T Value;
};
