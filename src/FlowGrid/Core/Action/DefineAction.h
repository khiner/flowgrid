#pragma once

#include "Action.h"
#include "Core/Json.h"

// A set of macros for defining actions.

#define MergeType_NoMerge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &) const { return false; }
#define MergeType_Merge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { return other; }
#define MergeType_CustomMerge(ActionType) std::variant<ActionType, bool> Merge(const ActionType &) const;
#define MergeType_SamePathMerge(ActionType)                                      \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { \
        if (this->path == other.path) return other;                              \
        return false;                                                            \
    }

/**
* Pass `is_savable = 1` to declare the action as savable (undoable, gesture history, saved in `.fga` projects).
* Use `action.q()` to queue the action. _Note: `q` methods for all action types are defined in `App.cpp`._
* Merge types:
  - `NoMerge`: Cannot be merged with any other action.
  - `Merge`: Can be merged with any other action of the same type.
  - `CustomMerge`: Override the action type's `Merge` function with a custom implementation.
*/
#define DefineActionInternal(ActionType, is_savable, merge_type, meta_str, ...) \
    struct ActionType {                                                         \
        inline static const Metadata _Meta{#ActionType, meta_str};              \
        static constexpr bool IsSavable = is_savable;                           \
        void q() const;                                                         \
        static void MenuItem();                                                 \
        static fs::path GetPath() { return _TypePath / _Meta.PathLeaf; }        \
        static const std::string &GetName() { return _Meta.Name; }              \
        static const std::string &GetMenuLabel() { return _Meta.MenuLabel; }    \
        static const std::string &GetShortcut() { return _Meta.Shortcut; }      \
        MergeType_##merge_type(ActionType);                                     \
        __VA_ARGS__;                                                            \
    };

#define DefineAction(ActionType, merge_type, meta_str, ...) \
    DefineActionInternal(ActionType, 1, merge_type, meta_str, __VA_ARGS__)

#define DefineUnsavedAction(ActionType, merge_type, meta_str, ...) \
    DefineActionInternal(ActionType, 0, merge_type, meta_str, __VA_ARGS__)

#define DefineFieldAction(ActionType, meta_str, ...)                  \
    DefineActionInternal(                                             \
        ActionType, 1, SamePathMerge, meta_str,                       \
        fs::path GetFieldPath() const { return path; } fs::path path; \
        __VA_ARGS__                                                   \
    )

#define DefineActionType(TypePath, ...)                \
    namespace Action {                                 \
    namespace TypePath {                               \
    inline static const fs::path _TypePath{#TypePath}; \
    __VA_ARGS__;                                       \
    }                                                  \
    }
