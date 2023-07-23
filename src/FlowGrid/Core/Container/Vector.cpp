#include "Vector.h"

#include "Core/Store/Store.h"

template<HasId ChildType> void Vector<ChildType>::Refresh() {
    // Only handling deletes so far...
    std::erase_if(Value, [this](const auto &child) {
        // The child is considered deleted if its first leaf is deleted.
        // This avoids needing to look through every path in the store to see if it's prefixed with the child's path.
        const auto *first_leaf = child->GetFirstLeaf();
        if (!first_leaf) throw std::runtime_error("Child has no first leaf.");
        return !RootStore.Exists(first_leaf->Path);
    });
}

// Explicit instantiations.
#include "Project/Audio/Graph/AudioGraphNode.h"

template struct Vector<AudioGraphNode>;
