#pragma once

#include "StoreBase.h"

#include "Core/TextEditor/TextBufferData.h"
#include "IdPairs.h"
#include "Patch/Patch.h"

struct Store : StoreBase<
                   bool, u32, s32, float, std::string, IdPairs, TextBufferData, immer::set<u32>,
                   immer::flex_vector<bool>, immer::flex_vector<s32>, immer::flex_vector<u32>, immer::flex_vector<float>, immer::flex_vector<std::string>> {
    // Create a patch comparing the provided stores.
    static Patch CreatePatch(const StoreMaps &, const StoreMaps &, ID base_component_id);

    // Create a patch comparing the provided store with the current persistent store.
    Patch CreatePatch(const Store &other, ID base_component_id) const { return CreatePatch(Maps, other.Maps, base_component_id); }

    // Create a patch comparing the current transient store with the current persistent store.
    // **Resets the transient store to the current persisent store.**
    Patch CreatePatchAndResetTransient(ID base_component_id) {
        const auto patch = CreatePatch(Maps, Persistent(), base_component_id);
        TransientMaps = Transient();
        return patch;
    }

    // Same as `Commit`, but returns the resulting patch.
    Patch CheckedCommit(ID base_component_id) {
        auto new_maps = Persistent();
        const auto patch = CreatePatch(Maps, new_maps, base_component_id);
        Commit(std::move(new_maps));
        return patch;
    }
};
