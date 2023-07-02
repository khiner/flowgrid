#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "Core/Component.h"

#include "FieldActionHandler.h"

struct Patch;

// A `Field` is a component that wraps around a value backed by the application `Store`.
// Fields are always leafs in the `App` component tree, and leafs are always fields, making Fields 1:1 with `App` component leafs.
// todo Enforce Fields have no children (best done with types).
struct Field : Component {
    struct ChangeListener {
        // Called when at least one of the listened fields has changed.
        // Changed field(s) are not passed to the callback, but it's called while the fields are still marked as changed,
        // so listeners can use `field.IsChanged()` to check which listened fields were changed if they wish.
        virtual void OnFieldChanged() = 0;
    };

    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    inline static std::unordered_map<ID, Field *> FieldById;
    inline static std::unordered_map<StorePath, ID, PathHash> FieldIdByPath;
    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersByFieldId;
    // IDs of all fields updated during the latest action pass, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the path set will only contain the root path.
    // For container fields, the path set will contain the container-relative paths of all affected elements.
    // These same key IDs are also stored in the `ChangedComponentIds` set,
    // which also includes IDs for all ancestor component of all changed fields.
    using UniquePaths = std::unordered_set<StorePath, PathHash>;
    inline static std::unordered_map<ID, UniquePaths> ChangedPathsByFieldId;

    // Chronological vector of (Unique field-relative-paths, update-time) pairs for each field that has been updated during the current gesture.
    using PathsMoment = std::pair<TimePoint, UniquePaths>;
    inline static std::unordered_map<ID, std::vector<PathsMoment>> GestureChangedPathsByFieldId{};

    static std::optional<TimePoint> LatestUpdateTime(const ID component_id);

    inline static void RegisterChangeListener(ChangeListener *listener, const Field &field) {
        if (!ChangeListenersByFieldId.contains(field.Id)) {
            ChangeListenersByFieldId.emplace(field.Id, std::unordered_set<ChangeListener *>{});
        }
        ChangeListenersByFieldId[field.Id].insert(listener);
    }
    inline static void UnregisterChangeListener(ChangeListener *listener) {
        for (auto &[field_id, listeners] : ChangeListenersByFieldId) {
            listeners.erase(listener);
            if (listeners.empty()) ChangeListenersByFieldId.erase(field_id);
        }
    }
    inline void RegisterChangeListener(ChangeListener *listener) const { RegisterChangeListener(listener, *this); }

    inline static Field *FindByPath(const StorePath &search_path) noexcept {
        if (FieldIdByPath.contains(search_path)) return FieldById[FieldIdByPath[search_path]];
        // Search for container fields.
        if (FieldIdByPath.contains(search_path.parent_path())) return FieldById[FieldIdByPath[search_path.parent_path()]];
        if (FieldIdByPath.contains(search_path.parent_path().parent_path())) return FieldById[FieldIdByPath[search_path.parent_path().parent_path()]];
        return nullptr;
    }

    // Refresh the cached values of all fields affected by the patch, and notifies all listeners of the affected fields.
    static void RefreshChanged(const Patch &);
    inline static void ClearChanged() noexcept {
        ChangedPathsByFieldId.clear();
        ChangedComponentIds.clear();
    }

    // Refresh the cached values of all fields.
    // Only used during `main.cpp` initialization.
    inline static void RefreshAll() {
        for (auto &[id, field] : FieldById) field->RefreshValue();
        std::unordered_set<ChangeListener *> all_listeners;
        for (auto &[id, listeners] : ChangeListenersByFieldId) {
            all_listeners.insert(listeners.begin(), listeners.end());
        }
    }

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static FieldActionHandler ActionHandler;

    // Refresh the cached value based on the main store.
    // Should be called for each affected field after a state change to avoid stale values.
    virtual void RefreshValue() = 0;

    inline bool IsChanged() const noexcept { return ChangedPathsByFieldId.contains(Id); }

    virtual void RenderValueTree(ValueTreeLabelMode, bool auto_select) const override;

private:
    // Find and mark fields with values that were made stale during the most recent action pass.
    // Used internally by `RefreshChanged`.
    static void FindAndMarkChanged(const Patch &);
};
