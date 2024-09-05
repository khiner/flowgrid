#include "PrimitiveVector.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/flex_vector_transient.hpp"

template<typename T> bool PrimitiveVector<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void PrimitiveVector<T>::Erase() const { S.Erase<ContainerT>(Id); }
template<typename T> void PrimitiveVector<T>::Clear() const { S.Clear<ContainerT>(Id); }

template<typename T> PrimitiveVector<T>::ContainerT PrimitiveVector<T>::Get() const { return S.Get<ContainerT>(Id); }

template<typename T> void PrimitiveVector<T>::Set(const std::vector<T> &value) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    S.Set(Id, val.persistent());
}

template<typename T> void PrimitiveVector<T>::Set(size_t i, const T &value) const { S.Set(Id, Get().set(i, value)); }
template<typename T> void PrimitiveVector<T>::PushBack(const T &value) const { S.Set(Id, S.Get<ContainerT>(Id).push_back(value)); }
template<typename T> void PrimitiveVector<T>::PopBack() const {
    const auto v = S.Get<ContainerT>(Id);
    S.Set(Id, v.take(v.size() - 1));
}

template<typename T> void PrimitiveVector<T>::Resize(size_t size) const {
    S.Set(Id, Get().take(size));
    while (Size() < size) PushBack(T{});
}
template<typename T> void PrimitiveVector<T>::Erase(size_t i) const { S.Set(Id, Get().erase(i)); }

template<typename T> size_t PrimitiveVector<T>::IndexOf(const T &value) const {
    auto vec = Get();
    return std::ranges::find(vec, value) - vec.begin();
}

template<typename T> void PrimitiveVector<T>::SetJson(json &&j) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    S.Set(Id, val.persistent());
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
