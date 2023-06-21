#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Component.h"

#include "FieldActionHandler.h"

// A `Field` is a component backed by a store value.
struct Field : Component {
    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    inline static std::vector<Field *> Instances; // All fields.
    inline static std::map<StorePath, U32> IndexForPath; // Maps store paths to field indices.
    inline static std::vector<bool> ValueIsStale;

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static void RefreshStale() {
        for (size_t i = 0; i < ValueIsStale.size(); i++) {
            if (ValueIsStale[i]) Instances[i]->RefreshValue();
        }
        ValueIsStale.assign(ValueIsStale.size(), false);
    }

    inline static void RefreshAll() {
        for (auto *field : Instances) field->RefreshValue();
    }

    static Field *FindByPath(const StorePath &);

    inline static FieldActionHandler ActionHandler;

    // Refresh the cached value based on the main store.
    // Should be called for each affected field after a state change to avoid stale values.
    virtual void RefreshValue() = 0;

    inline bool IsValueStale() const noexcept { return ValueIsStale[Index]; }
    inline void MarkValueStale() const noexcept { ValueIsStale[Index] = true; }

    unsigned int Index; // Index in `Instances`.
};
