#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Component.h"

#include "FieldActionHandler.h"

// A `Field` is a component backed by a store value.
struct Field : Component {
    inline static std::vector<Field *> Instances; // All fields.
    inline static std::map<StorePath, U32> IndexForPath; // Maps store paths to field indices.

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    static Field *FindByPath(const StorePath &);

    inline static FieldActionHandler ActionHandler;

    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    // Refresh the cached value based on the main store.
    // Should be called for each affected field after a state change to avoid stale values.
    virtual void RefreshValue() = 0;

    U32 Index; // Index in `Instances`.
};
