#include "Actions.h"

SavableActionMoments MergeActions(const SavableActionMoments &actions) {
    SavableActionMoments merged_actions; // Mutable return value.

    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const SavableActionMoment> active;
    for (u32 i = 0; i < actions.size(); i++) {
        if (!active) active.emplace(actions[i]);
        const auto &a = *active;
        const auto &b = actions[i + 1];
        const auto merge_result = a.Action.Merge(b.Action);
        Visit(
            merge_result,
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_actions.emplace_back(a); //
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const Action::Savable &merged_action) {
                // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
                active.emplace(merged_action, b.QueueTime);
            },
        );
    }
    if (active) merged_actions.emplace_back(*active);

    return merged_actions;
}
