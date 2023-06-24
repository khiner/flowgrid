#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "Core/Component.h"

#include "FieldActionHandler.h"

// A `Field` is a component backed by a store value.
struct Field : Component {
    struct ChangeListener {
        // Called when at least one of the listened fields has changed.
        // Changed field(s) are not passed to the callback.
        // However, this callback is called before the changed values are marked as non-stale,
        // so listeners can use `field.IsValueStale()` to check which listened fields were changed if they wish.
        // todo change `IsValueStale` to `IsValueChanged`.
        virtual void OnFieldChanged() = 0;
    };

    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    inline static std::vector<Field *> Instances; // All fields.
    inline static std::unordered_map<StorePath, U32, PathHash> IndexForPath; // Maps store paths to field indices.
    inline static std::vector<bool> ValueIsStale;

    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListeners;
    inline static void RegisterChangeListener(const Field &field, ChangeListener *listener) {
        if (ChangeListeners.find(field.Id) == ChangeListeners.end()) {
            ChangeListeners.emplace(field.Id, std::unordered_set<ChangeListener *>{});
        }
        ChangeListeners[field.Id].insert(listener);
    }
    inline static void UnregisterChangeListener(ChangeListener *listener) {
        for (auto &[field_id, listeners] : ChangeListeners) {
            listeners.erase(listener);
            if (listeners.empty()) ChangeListeners.erase(field_id);
        }
    }

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static void RefreshStale() {
        static std::unordered_set<ChangeListener *> AffectedListeners;
        for (size_t i = 0; i < Instances.size(); i++) {
            if (ValueIsStale[i]) {
                auto *field = Instances[i];
                field->RefreshValue();
                const auto &listeners = ChangeListeners[field->Id];
                AffectedListeners.insert(listeners.begin(), listeners.end());
            }
        }

        for (auto *listener : AffectedListeners) listener->OnFieldChanged();
        AffectedListeners.clear();

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
