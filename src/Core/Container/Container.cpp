#include "Vector.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/flex_vector_transient.hpp"

template<typename T> Vector<T>::ContainerT Vector<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> bool Vector<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void Vector<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Vector<T>::Clear() const { _S.Clear<ContainerT>(Id); }

template<typename T> void Vector<T>::Set(const std::vector<T> &value) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Set(size_t i, const T &value) const { _S.Set(Id, Get().set(i, value)); }
template<typename T> void Vector<T>::Set(const std::unordered_map<size_t, T> &values) const {
    auto val = Get().transient();
    for (const auto &[i, value] : values) val.set(i, value);
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::PushBack(const T &value) const { _S.Set(Id, S.Get<ContainerT>(Id).push_back(value)); }
template<typename T> void Vector<T>::PopBack() const {
    const auto v = S.Get<ContainerT>(Id);
    _S.Set(Id, v.take(v.size() - 1));
}

template<typename T> void Vector<T>::Resize(size_t size) const {
    auto val = Get().take(size).transient();
    while (val.size() < size) val.push_back(T{});
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Erase(size_t i) const { _S.Set(Id, Get().erase(i)); }

template<typename T> size_t Vector<T>::IndexOf(const T &value) const {
    auto vec = Get();
    return std::ranges::find(vec, value) - vec.begin();
}

template<typename T> void Vector<T>::SetJson(json &&j) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Vector<T>::ToJson() const { return json(Get()).dump(); }

using namespace ImGui;

template<typename T> void Vector<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
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
template struct Vector<bool>;
template struct Vector<s32>;
template struct Vector<u32>;
template struct Vector<float>;
template struct Vector<std::string>;

#include "Set.h"

#include "immer/set_transient.hpp"

template<typename T> Set<T>::ContainerT Set<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> bool Set<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void Set<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Set<T>::Clear() const { _S.Clear<ContainerT>(Id); }
template<typename T> void Set<T>::Insert(const T &value) const { _S.Set(Id, Get().insert(value)); }
template<typename T> void Set<T>::Erase(const T &value) const { _S.Set(Id, Get().erase(value)); }

template<typename T> void Set<T>::SetJson(json &&j) const {
    immer::set_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.insert(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Set<T>::ToJson() const { return json(Get()).dump(); }

using namespace ImGui;

template<typename T> void Set<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 v : value) {
            FlashUpdateRecencyBackground(std::to_string(v));
            TextUnformatted(std::to_string(v));
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Set<u32>;
