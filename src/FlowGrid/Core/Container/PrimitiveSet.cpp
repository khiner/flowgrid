#include "PrimitiveSet.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/set_transient.hpp"

template<IsPrimitive T> PrimitiveSet<T>::ContainerT PrimitiveSet<T>::Get() const { return RootStore.Get<ContainerT>(Path);}

template<IsPrimitive T> void PrimitiveSet<T>::Set(const std::set<T> &value) const {
    immer::set_transient<T> val{};
    for (const auto &v : value) val.insert(v);
    RootStore.Set(Path, val.persistent());
}

template<IsPrimitive T> void PrimitiveSet<T>::Insert(const T &value) const {
    if (!RootStore.CountAt<ContainerT>(Path)) RootStore.Set<ContainerT>(Path, {});
    RootStore.Set(Path, RootStore.Get<ContainerT>(Path).insert(value));
}
template<IsPrimitive T> void PrimitiveSet<T>::Erase_(const T &value) const {
    if (RootStore.CountAt<ContainerT>(Path)) RootStore.Set(Path, RootStore.Get<ContainerT>(Path).erase(value));
}
template<IsPrimitive T> bool PrimitiveSet<T>::Contains(const T &value) const {
    return RootStore.CountAt<ContainerT>(Path) && RootStore.Get<ContainerT>(Path).count(value);
}
template<IsPrimitive T> bool PrimitiveSet<T>::Empty() const {
    return !RootStore.CountAt<ContainerT>(Path) || RootStore.Get<ContainerT>(Path).empty();
}

template<IsPrimitive T> void PrimitiveSet<T>::Refresh() {} // Not cached.

template<IsPrimitive T> void PrimitiveSet<T>::SetJson(json &&j) const {
    std::set<T> value = json::parse(std::string(std::move(j)));
    Set(std::move(value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<IsPrimitive T> json PrimitiveSet<T>::ToJson() const {
    auto value = RootStore.Get<ContainerT>(Path);
    std::set<T> val{};
    for (const auto &v : value) val.insert(v);
    return json(val).dump();
}

using namespace ImGui;

template<IsPrimitive T> void PrimitiveSet<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = RootStore.Get<ContainerT>(Path);
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 v : value) {
            FlashUpdateRecencyBackground(std::to_string(v));
            TextUnformatted(std::to_string(v).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct PrimitiveSet<u32>;
