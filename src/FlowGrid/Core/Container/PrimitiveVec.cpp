#include "PrimitiveVec.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/vector_transient.hpp"

template<typename T> bool PrimitiveVec<T>::Exists() const { return RootStore.Count<ContainerT>(Path); }
template<typename T> void PrimitiveVec<T>::Erase() const { RootStore.Erase<ContainerT>(Path); }
template<typename T> void PrimitiveVec<T>::Clear() const { RootStore.Clear<ContainerT>(Path); }

template<typename T> PrimitiveVec<T>::ContainerT PrimitiveVec<T>::Get() const {
    if (!Exists()) return {};
    return RootStore.Get<ContainerT>(Path);
}

template<typename T> void PrimitiveVec<T>::Set(const std::vector<T> &value) const {
    immer::vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    RootStore.Set(Path, val.persistent());
}

template<typename T> void PrimitiveVec<T>::Set(size_t i, const T &value) const { RootStore.VectorSet(Path, i, value); }
template<typename T> void PrimitiveVec<T>::PushBack(const T &value) const { RootStore.PushBack(Path, value); }
template<typename T> void PrimitiveVec<T>::PopBack() const { RootStore.PopBack<T>(Path); }

template<typename T> void PrimitiveVec<T>::Resize(size_t size) const {
    if (Size() == size) return;

    if (Exists()) RootStore.Set(Path, Get().take(size));
    else RootStore.Set(Path, ContainerT{});

    while (Size() < size) PushBack(T{});
}
template<typename T> void PrimitiveVec<T>::Erase(size_t i) const {
    if (!Exists() || i >= Size()) return;

    // `immer::vector` does not have an `erase` or a `drop` like `flex_vector` does.
    auto val = Get();
    auto new_val = val.take(i).transient();
    for (size_t j = i + 1; j < val.size(); j++) new_val.push_back(val[j]);
    RootStore.Set(Path, new_val.persistent());
}

template<typename T> void PrimitiveVec<T>::SetJson(json &&j) const {
    immer::vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    RootStore.Set(Path, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveVec<T>::ToJson() const { return json(Get()).dump(); }

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

// Explicit instantiations.
template struct PrimitiveVec<bool>;
template struct PrimitiveVec<s32>;
template struct PrimitiveVec<u32>;
template struct PrimitiveVec<float>;
template struct PrimitiveVec<std::string>;
