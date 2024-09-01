#include "PrimitiveVector.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/flex_vector_transient.hpp"

template<typename T> bool PrimitiveVector<T>::Exists() const { return RootStore.Count<ContainerT>(Id); }
template<typename T> void PrimitiveVector<T>::Erase() const { RootStore.Erase<ContainerT>(Id); }
template<typename T> void PrimitiveVector<T>::Clear() const { RootStore.Clear<ContainerT>(Id); }

template<typename T> PrimitiveVector<T>::ContainerT PrimitiveVector<T>::Get() const {
    return RootStore.Get<ContainerT>(Id);
}

template<typename T> void PrimitiveVector<T>::Set(PrimitiveVector<T>::ContainerT value) const {
    RootStore.Set(Id, std::move(value));
}

template<typename T> void PrimitiveVector<T>::Set(const std::vector<T> &value) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    Set(val.persistent());
}

template<typename T> void PrimitiveVector<T>::Set(size_t i, const T &value) const { Set(Get().set(i, value)); }
template<typename T> void PrimitiveVector<T>::PushBack(const T &value) const { RootStore.PushBack(Id, value); }
template<typename T> void PrimitiveVector<T>::PopBack() const { RootStore.PopBack<T>(Id); }

template<typename T> void PrimitiveVector<T>::Resize(size_t size) const {
    if (Size() == size) return;

    if (Exists()) Set(Get().take(size));
    else Set(ContainerT{});

    while (Size() < size) PushBack(T{});
}
template<typename T> void PrimitiveVector<T>::Erase(size_t i) const {
    if (!Exists() || i >= Size()) return;

    Set(Get().erase(i));
}

template<typename T> size_t PrimitiveVector<T>::IndexOf(const T &value) const {
    auto vec = Get();
    return std::ranges::find(vec, value) - vec.begin();
}

template<typename T> void PrimitiveVector<T>::SetJson(json &&j) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    Set(val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveVector<T>::ToJson() const { return json(Get()).dump(); }

using namespace ImGui;

template<typename T> void PrimitiveVector<T>::RenderValueTree(bool annotate, bool auto_select) const {
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
template struct PrimitiveVector<bool>;
template struct PrimitiveVector<s32>;
template struct PrimitiveVector<u32>;
template struct PrimitiveVector<float>;
template struct PrimitiveVector<std::string>;
