#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Core/Action/Actionable.h"
#include "Core/Component.h"
#include "FieldActionHandler.h"
#include "Helper/Paths.h"

struct Patch;

// A `Field` is a component that wraps around a value backed by the owning project's `Store`.
// Fields are always leafs in a component tree, and leafs are always fields.
struct Field : Component {
    using References = std::vector<std::reference_wrapper<const Field>>;

    struct ChangeListener {
        // Called when at least one of the listened fields has changed.
        // Changed field(s) are not passed to the callback, but it's called while the fields are still marked as changed,
        // so listeners can use `field.IsChanged()` to check which listened fields were changed if they wish.
        virtual void OnFieldChanged() = 0;
    };

    Field(ComponentArgs &&);
    virtual ~Field();

    Field &operator=(const Field &) = delete;

    inline static FieldActionHandler ActionHandler;

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static std::unordered_map<ID, Field *> FieldById;
    inline static std::unordered_map<StorePath, ID, PathHash> FieldIdByPath;


    // Use when you expect a field with exactly this path to exist.
    inline static Field *ByPath(const StorePath &path) noexcept { return FieldById.at(FieldIdByPath.at(path)); }
    inline static Field *ByPath(StorePath &&path) noexcept { return FieldById.at(FieldIdByPath.at(std::move(path))); }

    inline static Field *FindByPath(const StorePath &search_path) noexcept {
        if (FieldIdByPath.contains(search_path)) return ByPath(search_path);
        // Search for container fields.
        if (FieldIdByPath.contains(search_path.parent_path())) return ByPath(search_path.parent_path());
        if (FieldIdByPath.contains(search_path.parent_path().parent_path())) return ByPath(search_path.parent_path().parent_path());
        return nullptr;
    }

    static std::optional<std::filesystem::path> FindLongestIntegerSuffixSubpath(const StorePath &);
    static Field *FindVectorFieldByChildPath(const StorePath &search_path);

    inline static std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersByFieldId;

    inline static void RegisterChangeListener(ChangeListener *listener, const Field &field) noexcept {
        ChangeListenersByFieldId[field.Id].insert(listener);
    }
    inline static void UnregisterChangeListener(ChangeListener *listener) noexcept {
        for (auto &[field_id, listeners] : ChangeListenersByFieldId) listeners.erase(listener);
        std::erase_if(ChangeListenersByFieldId, [](const auto &entry) { return entry.second.empty(); });
    }
    inline void RegisterChangeListener(ChangeListener *listener) const noexcept { RegisterChangeListener(listener, *this); }

    // IDs of all fields updated during the latest action batch or undo/redo, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the paths will consist of only the root path.
    // For container fields, the paths will contain the container-relative paths of all affected elements.
    // All values are appended to `GestureChangedPaths` if the change occurred during an action batch.
    // This is cleared at the end of each action batch, and can thus be used to determine which fields were affected by the latest action batch.
    // (`LatestChangedPaths` is retained for the lifetime of the application.)
    // These same key IDs are also stored in the `ChangedComponentIds` set, which also includes IDs for all ancestor component of all changed fields.
    inline static std::unordered_map<ID, PathsMoment> ChangedPaths;

    // Latest (unique-field-relative-paths, store-commit-time) pair for each field over the lifetime of the application.
    // This is updated by both the forward action pass, and by undo/redo.
    inline static std::unordered_map<ID, PathsMoment> LatestChangedPaths{};

    // Chronological vector of (unique-field-relative-paths, store-commit-time) pairs for each field that has been updated during the current gesture.
    inline static std::unordered_map<ID, std::vector<PathsMoment>> GestureChangedPaths{};

    static std::optional<TimePoint> LatestUpdateTime(const ID component_id);

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    static void RefreshChanged(const Patch &, bool add_to_gesture = false);

    inline static void ClearChanged() noexcept {
        ChangedPaths.clear();
        ChangedComponentIds.clear();
    }

    // Refresh the cached values of all fields.
    // Only used during `main.cpp` initialization.
    static void RefreshAll();

    virtual void RenderValueTree(bool annotate, bool auto_select) const override;

private:
    // Find and mark fields with values that were made stale during the most recent action pass.
    // Used internally by `RefreshChanged`.
    static void FindAndMarkChanged(const Patch &);
};
