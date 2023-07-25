#pragma once

#include <concepts>

#include "Core/Container/PrimitiveVector.h"
#include "Helper/Hex.h"

template<typename T>
concept HasId = requires(T t) {
    { t.Id } -> std::same_as<const ID &>;
};

/*
A component whose children are created/destroyed dynamically, with vector-ish semantics.
Like a `Field`, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
(It needs access to the definition of ths `ChildType` default destructor, though, since it uses `std::unique_ptr`.)

`Vector` uses a path prefix strategy chosen with the following constraints:
1) Deterministic. Inserting/erasing the same child types in the same order should produce the same store paths.
2) No collisions: Adding multiple instances of the same child type should produce different store paths.
3) Consistent component paths: Adding a child should not change the store paths of existing children.
4) Reduce the number of deletions/insertions when refreshing to the current store.
   - Favor updating an existing child to have the properties of a different child over deleting and inserting a new child.
5) We can't use raw int segments, since we rely on flattening JSON to deserialize, and flattening interprets int segments as array indices.

The path prefix strategy is as follows:
* If a child is added with a path segment different from any existing children, it gets a prefix of '0'.
* If a child is added with a path segment equal to an existing child, it gets a prefix equal to the minimum available prefix between '0' and max existing prefix + 1.

Child order is tracked with a separate ID vector.
We need to store this in an auxiliary store member since child component members are stored in a persistent map without key ordering.
*/
template<HasId ChildType> struct Vector : Field {
    using CreatorFunction = std::function<std::unique_ptr<ChildType>(Component *, string_view path_prefix_segment, string_view path_segment)>;

    Vector(ComponentArgs &&args, CreatorFunction creator)
        : Field(std::move(args)), Creator(std::move(creator)) {
        ComponentContainerFields.emplace_back(Id);
    }
    ~Vector() {
        std::erase_if(ComponentContainerFields, [this](const auto &id) { return id == Id; });
    }

    inline static std::pair<std::string, std::string> GetPathPrefixAndSegment(StorePath relative_path) {
        auto it = relative_path.begin();
        const auto path_prefix = *it;
        const auto path_segment = *std::next(it);
        return {path_prefix.string(), path_segment.string()};
    }

    std::string GenerateNextPrefix(string_view path_segment) const {
        std::vector<u32> existing_prefix_ids;
        for (const auto &child : Value) {
            const auto &child_path = child->Path;
            const auto relative_path = child_path.lexically_relative(Path);
            const auto &[child_path_prefix, child_path_segment] = GetPathPrefixAndSegment(relative_path);
            if (path_segment == child_path_segment) {
                existing_prefix_ids.push_back(HexToU32(child_path_prefix));
            }
        }
        std::sort(existing_prefix_ids.begin(), existing_prefix_ids.end());

        u32 prefix_id = 0;
        for (const auto existing_prefix_id : existing_prefix_ids) {
            if (prefix_id != existing_prefix_id) break;
            prefix_id++;
        }
        return U32ToHex(prefix_id);
    }

    void EmplaceBack(string_view path_segment) {
        Value.emplace_back(Creator(this, GenerateNextPrefix(path_segment), path_segment));
        Ids.PushBack_(Value.back()->Id);
    }

    struct Iterator : std::vector<std::unique_ptr<ChildType>>::const_iterator {
        using Base = std::vector<std::unique_ptr<ChildType>>::const_iterator;

        Iterator(Base it) : Base(it) {}
        const ChildType *operator*() const { return Base::operator*().get(); }
        ChildType *operator*() { return Base::operator*().get(); }
    };

    Iterator begin() const { return Value.cbegin(); }
    Iterator end() const { return Value.cend(); }

    ChildType *back() const { return Value.back().get(); }
    ChildType *operator[](u32 i) const { return Value[i].get(); }

    u32 Size() const { return Value.size(); }

    void Refresh() override;

    void EraseId(ID id) const {
        Ids.Erase(id);
        const auto it = std::find_if(Value.begin(), Value.end(), [id](const auto &child) { return child->Id == id; });
        if (it != Value.end()) {
            it->get()->Erase();
        }
    }

    void RefreshFromJson(const json &) override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

private:
    void RefreshFromChangedPathPairs(const std::unordered_set<StorePath, PathHash> &);

    // Returns an iterator.
    auto FindByPathPair(const StorePath &path_pair) {
        const auto &vector_path = Path;
        return std::find_if(Value.begin(), Value.end(), [&path_pair, &vector_path](const auto &child) {
            const auto &child_path = child->Path;
            const auto relative_path = child_path.lexically_relative(vector_path);
            return GetPathPrefixAndSegment(relative_path) == GetPathPrefixAndSegment(path_pair);
        });
    }

    CreatorFunction Creator;
    std::vector<std::unique_ptr<ChildType>> Value;

    Prop(PrimitiveVector<ID>, Ids); // Keep track of child ordering.
};
