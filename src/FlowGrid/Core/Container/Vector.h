#pragma once

#include "Core/Container/PrimitiveVector.h"
#include "Helper/Hex.h"

inline static std::pair<std::string, std::string> Split(fs::path relative_path) {
    const auto it = relative_path.begin();
    return {it->string(), std::next(it)->string()};
}

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

Child order is tracked with a separate `ChildPrefixes` vector.
We need to store this in an auxiliary store member since child component members are stored in a persistent map without key ordering.
*/
template<typename ChildType> struct Vector : Container {
    using CreatorFunction = std::function<std::unique_ptr<ChildType>(ComponentArgs &&)>;
    using ChildInitializerFunction = std::function<void(ChildType *)>;

    inline static CreatorFunction DefaultCreator = [](ComponentArgs &&args) {
        return std::make_unique<ChildType>(std::move(args));
    };

    Vector(ComponentArgs &&args, Menu &&menu, CreatorFunction creator = DefaultCreator)
        : Container(std::move(args), std::move(menu)), Creator(std::move(creator)) {
        ContainerAuxiliaryIds.insert(ChildPrefixes.Id);
    }
    ~Vector() {
        ContainerAuxiliaryIds.erase(ChildPrefixes.Id);
    }

    Vector(ComponentArgs &&args, CreatorFunction creator = DefaultCreator)
        : Vector(std::move(args), Menu{{}}, std::move(creator)) {}

    bool Empty() const { return Value.empty(); }
    u32 Size() const { return Value.size(); }

    StorePath GetChildPrefix(const ChildType *child) const noexcept {
        if (child == nullptr) return {};

        const auto path = child->Path.lexically_relative(Path);
        const auto it = path.begin();
        return *it / *std::next(it);
    }

    inline std::string GetChildLabel(const ChildType *child, bool detailed = false) const noexcept {
        if (child == nullptr) return "";

        const std::string path_prefix = child->Path.parent_path().filename();
        const auto prefix_id = HexToU32(path_prefix);
        return std::format(
            "{}{}{}",
            child->Name,
            prefix_id == 0 ? "" : std::format(" {}", prefix_id),
            detailed && !child->GetLabelDetailSuffix().empty() ? std::format(" ({})", child->GetLabelDetailSuffix()) : ""
        );
    }

    std::string GenerateNextPrefix(string_view path_segment) const {
        std::vector<u32> existing_prefix_ids;
        for (const auto &child : Value) {
            const auto &[child_path_prefix, child_path_segment] = Split(child->Path.lexically_relative(Path));
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

    void EmplaceBack(string_view path_segment) const {
        ChildPrefixes.PushBack(StorePath(GenerateNextPrefix(path_segment)) / path_segment);
    }

    void EmplaceBack_(string_view path_segment, ChildInitializerFunction &&initializer = {}) {
        auto child = Creator({this, path_segment, "", GenerateNextPrefix(path_segment)});
        if (initializer) initializer(child.get());
        const auto child_prefix = GetChildPrefix(child.get());
        Value.emplace_back(std::move(child));
        ChildPrefixes.PushBack_(child_prefix);
    }

    struct Iterator : std::vector<std::unique_ptr<ChildType>>::const_iterator {
        using Base = std::vector<std::unique_ptr<ChildType>>::const_iterator;

        Iterator(Base it) : Base(it) {}
        const ChildType *operator*() const { return Base::operator*().get(); }
        ChildType *operator*() { return Base::operator*().get(); }
    };

    Iterator begin() const { return Value.begin(); }
    Iterator end() const { return Value.end(); }
    Iterator cbegin() const { return Value.cbegin(); }
    Iterator cend() const { return Value.cend(); }
    auto View() const { return std::views::all(Value); }

    ChildType *back() const { return Value.back().get(); }
    ChildType *front() const { return Value.front().get(); }
    ChildType *operator[](u32 i) const { return Value[i].get(); }

    ChildType *Find(ID id) const {
        const auto it = std::ranges::find_if(Value, [id](const auto &child) { return child->Id == id; });
        return it == Value.end() ? nullptr : it->get();
    }
    auto FindIt(const StorePath &child_prefix) const {
        return std::ranges::find_if(Value, [this, &child_prefix](const auto &child) { return GetChildPrefix(child.get()) == child_prefix; });
    }

    void Refresh() override {
        ChildPrefixes.Refresh();

        for (const StorePath prefix : ChildPrefixes) {
            if (const auto child_it = FindIt(prefix); child_it == Value.end()) {
                const auto &[path_prefix, path_segment] = Split(prefix);
                auto new_child = Creator({this, path_segment, "", path_prefix});
                u32 index = ChildPrefixes.IndexOf(GetChildPrefix(new_child.get()));
                Value.insert(Value.begin() + index, std::move(new_child));
            }
        }
        std::erase_if(Value, [this](const auto &child) { return !ChildPrefixes.Contains(GetChildPrefix(child.get())); });
        for (auto &child : Value) child->Refresh();
    }

    void EraseId(ID id) const {
        auto *child = Find(id);
        if (!child) return;

        child->Erase();
        ChildPrefixes.Erase(GetChildPrefix(child));
    }

    void EraseId_(ID id) {
        EraseId(id);
        Refresh();
    }

    void Clear() { Value.clear(); }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        if (Value.empty()) {
            TextUnformatted(std::format("{} (empty)", Name));
            return;
        }

        if (TreeNode(Name, false, nullptr, false, auto_select)) {
            for (u32 i = 0; i < Value.size(); i++) {
                if (Value.at(i)->TreeNode(std::to_string(i), false, nullptr, false, auto_select)) {
                    Value.at(i)->RenderValueTree(annotate, auto_select);
                    TreePop();
                }
            }
            TreePop();
        }
    }

private:
    // Keep track of child ordering.
    // Each prefix is a path containing two segments: The child's unique prefix and its `PathSegment`.
    Prop(PrimitiveVector<std::string>, ChildPrefixes);

    CreatorFunction Creator;
    std::vector<std::unique_ptr<ChildType>> Value;
};
