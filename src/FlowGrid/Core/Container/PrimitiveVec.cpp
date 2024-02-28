#include "PrimitiveVec.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/vector_transient.hpp"

template<typename T> bool PrimitiveVec<T>::Exists() const { return RootStore.Count<ContainerT>(Path); }
template<typename T> void PrimitiveVec<T>::Erase() const { RootStore.Erase<ContainerT>(Path); }

template<typename T> PrimitiveVec<T>::ContainerT PrimitiveVec<T>::Get() const { return RootStore.Get<ContainerT>(Path); }

template<typename T> void PrimitiveVec<T>::Set(const std::vector<T> &value) const {
    immer::vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    RootStore.Set(Path, val.persistent());
}

template<typename T> void PrimitiveVec<T>::Set(size_t i, const T &value) const {
    if (Exists()) RootStore.Set(Path, Get().set(i, value));
}

template<typename T> void PrimitiveVec<T>::Set(const std::vector<std::pair<size_t, T>> &values) const {
    for (const auto &[i, value] : values) Set(i, value);
}

template<typename T> void PrimitiveVec<T>::PushBack(const T &value) const {
    if (!Exists()) RootStore.Set<ContainerT>(Path, {});
    RootStore.Set(Path, Get().push_back(value));
}

template<typename T> void PrimitiveVec<T>::PopBack() const {
    if (Exists()) RootStore.Set(Path, Get().take(Get().size() - 1));
}

template<typename T> void PrimitiveVec<T>::Resize(u32 size) const {
    if (Exists()) RootStore.Set(Path, Get().take(size));
}

template<typename T> void PrimitiveVec<T>::SetJson(json &&j) const {
    std::vector<T> new_value = json::parse(std::string(std::move(j)));
    Set(std::move(new_value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveVec<T>::ToJson() const {
    auto value = Get();
    std::vector<T> val{};
    for (const auto &v : value) val.push_back(v);
    return json(val).dump();
}

using namespace ImGui;

template<typename T> void PrimitiveVec<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 i = 0; i < value.size(); i++) {
            FlashUpdateRecencyBackground(std::to_string(i));
            TreeNode(std::to_string(i), false, std::format("{}", value[i]).c_str());
        }
        TreePop();
    }
}

template<typename T> bool PrimitiveVec<T>::Empty() const { return Get().empty(); }
template<typename T> T PrimitiveVec<T>::operator[](u32 i) const { return Get()[i]; }
template<typename T> u32 PrimitiveVec<T>::Size() const { return Get().size(); }

// Explicit instantiations.
template struct PrimitiveVec<u32>;
