#pragma once

#include "Action.h"
#include "Core/Json.h"
#include "Core/Scalar.h" // Not actually used in this file, but included as a convenience for action definitions.

// Component actions hold the `component_id` of the component they act on.
#define ComponentActionJson(ActionType, ...) Json(ActionType, component_id __VA_OPT__(, ) __VA_ARGS__);

template<class...> constexpr bool always_false_v = false;

#define MergeType_NoMerge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &) const { return false; }
#define MergeType_Merge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { return other; }
#define MergeType_CustomMerge(ActionType) std::variant<ActionType, bool> Merge(const ActionType &) const;
#define MergeType_SameIdMerge(ActionType)                                        \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { \
        if (this->component_id == other.component_id) return other;              \
        return false;                                                            \
    }

/**
* Pass `is_savable = 1` to declare the action as savable (undoable, gesture history, saved in `.fga` projects).
* Merge types:
  - `NoMerge`: Cannot be merged with any other action.
  - `Merge`: Can be merged with any other action of the same type.
  - `CustomMerge`: Override the action type's `Merge` function with a custom implementation.
*/
#define DefineAction(ActionType, is_savable, merge_type, meta_str, ...)      \
    struct ActionType {                                                      \
        inline static const Metadata _Meta{#ActionType, meta_str};           \
        static constexpr bool IsSaved = is_savable;                          \
        static void MenuItem();                                              \
        static fs::path GetPath() { return _TypePath / _Meta.PathLeaf; }     \
        static const std::string &GetName() { return _Meta.Name; }           \
        static const std::string &GetMenuLabel() { return _Meta.MenuLabel; } \
        MergeType_##merge_type(ActionType);                                  \
        __VA_ARGS__;                                                         \
    };

inline static constexpr bool Saved = true, Unsaved = false;

#define DefineComponentAction(ActionType, is_savable, merge_type, meta_str, ...) \
    DefineAction(                                                                \
        ActionType, is_savable, merge_type, meta_str,                            \
        ID component_id;                                                         \
        ID GetComponentId() const { return component_id; } __VA_ARGS__           \
    )

#define DefineActionType(TypePath, ...)                \
    namespace Action {                                 \
    namespace TypePath {                               \
    inline static const fs::path _TypePath{#TypePath}; \
    __VA_ARGS__;                                       \
    }                                                  \
    }

#define DefineNestedActionType(ParentType, InnerType, ...)                      \
    namespace Action {                                                          \
    namespace ParentType {                                                      \
    namespace InnerType {                                                       \
    inline static const fs::path _TypePath{fs::path{#ParentType} / #InnerType}; \
    __VA_ARGS__;                                                                \
    }                                                                           \
    }                                                                           \
    }

#define DefineTemplatedActionType(ParentType, InnerType, TemplateType, ...)         \
    template<> struct ParentType<TemplateType> {                                    \
        inline static const fs::path _TypePath{fs::path{#ParentType} / #InnerType}; \
        __VA_ARGS__;                                                                \
    }
