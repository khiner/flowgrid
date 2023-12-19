#pragma once

#include "Core/Component.h"
#include "Core/Store/Patch/PatchOp.h"
#include "Helper/Paths.h"

struct Patch;

// todo next up: Get rid of `Field` class and move all its functionality into `Component`.
// A `Field` is a component that wraps around a value backed by the owning project's `Store`.
// Leafs in a component tree are always fields, but fields may have nested components/fields.
struct Field : Component {
    using References = std::vector<std::reference_wrapper<const Field>>;

    struct ChangeListener {
        // Called when at least one of the listened fields has changed.
        // Changed field(s) are not passed to the callback, but it's called while the fields are still marked as changed,
        // so listeners can use `field.IsChanged()` to check which listened fields were changed if they wish.
        virtual void OnFieldChanged() = 0;
    };

    Field(ComponentArgs &&, Menu &&menu);
    Field(ComponentArgs &&);
    virtual ~Field();

    Field &operator=(const Field &) = delete;

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static std::unordered_map<ID, Field *> FieldById;
    inline static std::unordered_map<StorePath, ID, PathHash> FieldIdByPath;

    // Component containers are fields that dynamically create/destroy child components.
    // Each component container has a single auxiliary field as a direct child which tracks the presence/ordering of its child component(s).
    inline static std::unordered_set<ID> ComponentContainerFields;
    inline static std::unordered_set<ID> ComponentContainerAuxiliaryFields;

    // Use when you expect a field with exactly this path to exist.
    inline static Field *ByPath(const StorePath &path) noexcept { return FieldById.at(FieldIdByPath.at(path)); }
    inline static Field *ByPath(StorePath &&path) noexcept { return FieldById.at(FieldIdByPath.at(std::move(path))); }

    inline static Field *Find(const StorePath &search_path) noexcept {
        if (FieldIdByPath.contains(search_path)) return ByPath(search_path);
        // Search for container fields.
        if (FieldIdByPath.contains(search_path.parent_path())) return ByPath(search_path.parent_path());
        if (FieldIdByPath.contains(search_path.parent_path().parent_path())) return ByPath(search_path.parent_path().parent_path());
        return nullptr;
    }

    static Field *FindComponentContainerFieldByPath(const StorePath &search_path);

    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersByFieldId;

    inline static void RegisterChangeListener(ChangeListener *listener, const Field &field) noexcept {
        ChangeListenersByFieldId[field.Id].insert(listener);
    }
    inline static void UnregisterChangeListener(ChangeListener *listener) noexcept {
        for (auto &[field_id, listeners] : ChangeListenersByFieldId) listeners.erase(listener);
        std::erase_if(ChangeListenersByFieldId, [](const auto &entry) { return entry.second.empty(); });
    }
    inline void RegisterChangeListener(ChangeListener *listener) const noexcept { RegisterChangeListener(listener, *this); }

    // IDs of all fields updated/added/removed during the latest action or undo/redo, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the paths will consist of only the root path.
    // For container fields, the paths will contain the container-relative paths of all affected elements.
    // All values are appended to `GestureChangedPaths` if the change occurred during a runtime action batch (as opposed to undo/redo, initialization, or project load).
    // `ChangedPaths` is cleared after each action (after refreshing all affected fields), and can thus be used to determine which fields were affected by the latest action.
    // (`LatestChangedPaths` is retained for the lifetime of the application.)
    // These same key IDs are also stored in the `ChangedFieldIds` set, which also includes IDs for all ancestor component of all changed fields.
    inline static std::unordered_map<ID, PathsMoment> ChangedPaths;

    // Latest (unique-field-relative-paths, store-commit-time) pair for each field over the lifetime of the application.
    // This is updated by both the forward action pass, and by undo/redo.
    inline static std::unordered_map<ID, PathsMoment> LatestChangedPaths{};

    // Chronological vector of (unique-field-relative-paths, store-commit-time) pairs for each field that has been updated during the current gesture.
    inline static std::unordered_map<ID, std::vector<PathsMoment>> GestureChangedPaths{};

    // IDs of all fields to which `ChangedPaths` are attributed.
    // These are the fields that should have their `Refresh()` called to update their cached values to synchronize with their backing store.
    inline static std::unordered_set<ID> ChangedFieldIds;

    inline static std::optional<TimePoint> LatestUpdateTime(ID field_id, std::optional<StorePath> relative_path = {}) noexcept {
        if (!LatestChangedPaths.contains(field_id)) return {};

        const auto &[update_time, paths] = LatestChangedPaths.at(field_id);
        if (!relative_path) return update_time;
        if (paths.contains(*relative_path)) return update_time;
        return {};
    }

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    static void RefreshChanged(const Patch &, bool add_to_gesture = false);

    inline static void ClearChanged() noexcept {
        ChangedPaths.clear();
        ChangedFieldIds.clear();
        ChangedAncestorComponentIds.clear();
    }

    // Refresh the cached values of all fields.
    // Only used during `main.cpp` initialization.
    static void RefreshAll();

    virtual void RenderValueTree(bool annotate, bool auto_select) const override = 0;

    void FlashUpdateRecencyBackground(std::optional<StorePath> relative_path = {}) const;

private:
    // Find the field whose `Refresh()` should be called in response to a patch with this path and op type.
    static Field *FindChanged(const StorePath &, PatchOp::Type);

    // Find and mark fields that are made stale with the provided patch.
    // If `Refresh()` is called on every field marked in `ChangedFieldIds`, the component tree will be fully refreshed.
    // This method also updates the following static fields for monitoring: ChangedAncestorComponentIds, ChangedPaths, LatestChangedPaths
    static void MarkAllChanged(const Patch &);
};
