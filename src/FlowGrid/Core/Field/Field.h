#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "Core/Component.h"

#include "FieldActionHandler.h"

struct Patch;

// A `Field` is a component backed by a store value.
struct Field : Component {
    struct ChangeListener {
        // Called when at least one of the listened fields has changed.
        // Changed field(s) are not passed to the callback
        // However, this callback is called before the changed values are marked as non-stale,
        // so listeners can use `field.IsChanged()` to check which listened fields were changed if they wish.
        virtual void OnFieldChanged() = 0;
    };

    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    inline static std::unordered_map<ID, Field *> FieldById;
    inline static std::unordered_map<StorePath, ID, PathHash> FieldIdByPath;
    inline static std::unordered_set<ID> ChangedFieldIds; // Fields updated during the current action pass (cleared at the end of it).
    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersForField;

    inline static void RegisterChangeListener(const Field &field, ChangeListener *listener) {
        if (!ChangeListenersForField.contains(field.Id)) {
            ChangeListenersForField.emplace(field.Id, std::unordered_set<ChangeListener *>{});
        }
        ChangeListenersForField[field.Id].insert(listener);
    }
    inline static void UnregisterChangeListener(ChangeListener *listener) {
        for (auto &[field_id, listeners] : ChangeListenersForField) {
            listeners.erase(listener);
            if (listeners.empty()) ChangeListenersForField.erase(field_id);
        }
    }

    static Field *FindByPath(const StorePath &);

    // Refresh the cached values of all fields affected by the patch, and notifies all listeners of the affected fields.
    static void RefreshChanged(const Patch &);

    // Refresh the cached values of all fields and notifies all listeners.
    // Only used during `main.cpp` initialization.
    inline static void RefreshAndNotifyAll() {
        for (auto &[id, field] : FieldById) field->RefreshValue();
        std::unordered_set<ChangeListener *> all_listeners;
        for (auto &[id, listeners] : ChangeListenersForField) {
            all_listeners.insert(listeners.begin(), listeners.end());
        }
        for (auto *listener : all_listeners) listener->OnFieldChanged();
    }

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static FieldActionHandler ActionHandler;

    // Refresh the cached value based on the main store.
    // Should be called for each affected field after a state change to avoid stale values.
    virtual void RefreshValue() = 0;

    inline bool IsChanged() const noexcept { return ChangedFieldIds.contains(Id); }

private:
    // Find and mark fields with values that were made stale during the most recent action pass.
    // Used internally by `RefreshChanged`.
    static void FindAndMarkChanged(const Patch &);
};
