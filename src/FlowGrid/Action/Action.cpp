#include "Action.h"

#include <range/v3/core.hpp>
#include <range/v3/view/concat.hpp>

using namespace action;
using ranges::to;
namespace views = ranges::views;

string to_string(PatchOp::Type patch_op_type) {
    switch (patch_op_type) {
        case AddOp: return "Add";
        case RemoveOp: return "Remove";
        case ReplaceOp: return "Replace";
    }
}

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    PatchOps merged = a;
    for (const auto &[path, op] : b) {
        if (merged.contains(path)) {
            const auto &old_op = merged.at(path);
            // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
            // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
            // For example, if the first patch removes a path, and the second one adds the same path,
            // we can't know from only looking at the pair whether the added value was the same as it was before the remove
            // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
            if (old_op.Op == AddOp) {
                if (op.Op == RemoveOp || ((op.Op == AddOp || op.Op == ReplaceOp) && old_op.Value == op.Value)) merged.erase(path); // Cancel out
                else merged[path] = {AddOp, op.Value, {}};
            } else if (old_op.Op == RemoveOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) {
                    if (old_op.Value == op.Value) merged.erase(path); // Cancel out
                    else merged[path] = {ReplaceOp, op.Value, old_op.Old};
                } else {
                    merged[path] = {RemoveOp, {}, old_op.Old};
                }
            } else if (old_op.Op == ReplaceOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) merged[path] = {ReplaceOp, op.Value, old_op.Old};
                else merged[path] = {RemoveOp, {}, old_op.Old};
            }
        } else {
            merged[path] = op;
        }
    }

    return merged;
}

/**
 Provided actions are assumed to be chronologically consecutive.

 Cases:
 * `b` can be merged into `a`: return the merged action
 * `b` cancels out `a` (e.g. two consecutive boolean toggles on the same value): return `true`
 * `b` cannot be merged into `a`: return `false`

 Only handling cases where merges can be determined from two consecutive actions.
 One could imagine cases where an idempotent cycle could be determined only from > 2 actions.
 For example, incrementing modulo N would require N consecutive increments to determine that they could all be cancelled out.
*/
std::variant<StatefulAction, bool> Merge(const StatefulAction &a, const StatefulAction &b) {
    const ID a_id = GetId(a);
    const ID b_id = GetId(b);
    const bool same_type = a_id == b_id;

    switch (a_id) {
        case id<OpenFileDialog>:
        case id<CloseFileDialog>:
        case id<ShowOpenProjectDialog>:
        case id<ShowSaveProjectDialog>:
        case id<CloseApplication>:
        case id<SetImGuiColorStyle>:
        case id<SetImPlotColorStyle>:
        case id<SetFlowGridColorStyle>:
        case id<SetGraphColorStyle>:
        case id<SetGraphLayoutStyle>:
        case id<ShowOpenFaustFileDialog>:
        case id<ShowSaveFaustFileDialog>: {
            if (same_type) return b;
            return false;
        }
        case id<OpenFaustFile>:
        case id<SetValue>: {
            if (same_type && std::get<SetValue>(a).path == std::get<SetValue>(b).path) return b;
            return false;
        }
        case id<SetValues>: {
            if (same_type) return SetValues{views::concat(std::get<SetValues>(a).values, std::get<SetValues>(b).values) | to<std::vector>};
            return false;
        }
        case id<SetVector>: {
            if (same_type && std::get<SetVector>(a).path == std::get<SetVector>(a).path) return b;
            return false;
        }
        case id<SetMatrix>: {
            if (same_type && std::get<SetMatrix>(a).path == std::get<SetMatrix>(a).path) return b;
            return false;
        }
        case id<ToggleValue>: return same_type && std::get<ToggleValue>(a).path == std::get<ToggleValue>(b).path;
        case id<ApplyPatch>: {
            if (same_type) {
                const auto &_a = std::get<ApplyPatch>(a);
                const auto &_b = std::get<ApplyPatch>(b);
                // Keep patch actions affecting different base state-paths separate,
                // since actions affecting different state bases are likely semantically different.
                const auto &ops = Merge(_a.patch.Ops, _b.patch.Ops);
                if (ops.empty()) return true;
                if (_a.patch.BasePath == _b.patch.BasePath) return ApplyPatch{ops, _b.patch.BasePath};
                return false;
            }
            return false;
        }
        default: return false;
    }
}

namespace action {
Gesture MergeGesture(const Gesture &gesture) {
    Gesture merged_gesture; // Mutable return value
    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const StatefulActionMoment> active;
    for (Count i = 0; i < gesture.size(); i++) {
        if (!active) active.emplace(gesture[i]);
        const auto &a = *active;
        const auto &b = gesture[i + 1];
        std::variant<StatefulAction, bool> merge_result = Merge(a.first, b.first);
        Match(
            merge_result,
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_gesture.emplace_back(a); //
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const StatefulAction &merged_action) {
                active.emplace(merged_action, b.second); // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
            },
        );
    }
    if (active) merged_gesture.emplace_back(*active);

    return merged_gesture;
}

string GetName(const ProjectAction &action) {
    return std::visit([](auto &&a) { return GetName<std::decay_t<decltype(a)>>(); }, action);
}
string GetName(const StatefulAction &action) {
    return std::visit([](auto &&a) { return GetName<std::decay_t<decltype(a)>>(); }, action);
}

string GetShortcut(const EmptyAction &action) {
    const ID id = std::visit([](const Action &&a) { return GetId(a); }, action);
    return ShortcutForId.contains(id) ? ShortcutForId.at(id) : "";
}

// An action's menu label is its name, except for a few exceptions.
string GetMenuLabel(const EmptyAction &action) {
    return Match(
        action,
        [](const ShowOpenProjectDialog &) { return "Open project"s; },
        [](const OpenEmptyProject &) { return "New project"s; },
        [](const SaveCurrentProject &) { return "Save project"s; },
        [](const ShowSaveProjectDialog &) { return "Save project as..."s; },
        [](const ShowOpenFaustFileDialog &) { return "Open DSP file"s; },
        [](const ShowSaveFaustFileDialog &) { return "Save DSP as..."s; },
        [](const ShowSaveFaustSvgFileDialog &) { return "Export SVG"s; },
        [](const ProjectAction &a) { return GetName(a); },
        [](const StatefulAction &a) { return GetName(a); },
    );
}
} // namespace action
