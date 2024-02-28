#include "PrimitiveVector.h"

#include "imgui.h"

#include "Core/Store/Store.h"

template<typename T> void PrimitiveVector<T>::Set(const std::vector<T> &value) const {
    u32 i = 0;
    while (i < value.size()) {
        Set(i, value[i]);
        i++;
    }
    Resize(i);
}

template<typename T> void PrimitiveVector<T>::Set(size_t i, const T &value) const {
    RootStore.Set(PathAt(i), value);
}

template<typename T> void PrimitiveVector<T>::Set(const std::vector<std::pair<int, T>> &values) const {
    for (const auto &[i, value] : values) Set(i, value);
}

template<typename T> void PrimitiveVector<T>::PushBack(const T &value) const {
    RootStore.Set(PathAt(Value.size()), value);
}

template<typename T> void PrimitiveVector<T>::PopBack() const {
    if (Value.empty()) return;
    RootStore.Erase<T>(PathAt(Value.size() - 1));
}

template<typename T> void PrimitiveVector<T>::Resize(u32 size) const {
    u32 i = size;
    while (RootStore.Count<T>(PathAt(i))) {
        RootStore.Erase<T>(PathAt(i));
        i++;
    }
}

template<typename T> void PrimitiveVector<T>::Erase() const { Resize(0); }

template<typename T> void PrimitiveVector<T>::Erase(const T &value) const {
    if (auto it = std::find(Value.begin(), Value.end(), value); it != Value.end()) {
        const u32 index = it - Value.begin();
        for (u32 relocate_index = Value.size() - 1; relocate_index > index; relocate_index--) {
            Set(relocate_index - 1, Value.at(relocate_index));
        }
        Resize(Value.size() - 1);
    }
}

template<typename T> void PrimitiveVector<T>::Refresh() {
    u32 i = 0;
    while (RootStore.Count<T>(PathAt(i))) {
        const T value = RootStore.Get<T>(PathAt(i));
        if (Value.size() == i) Value.push_back(value);
        else Value[i] = value;
        i++;
    }
    Value.resize(i);
}

template<typename T> void PrimitiveVector<T>::SetJson(json &&j) const {
    std::vector<T> new_value = json::parse(std::string(std::move(j)));
    Set(std::move(new_value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveVector<T>::ToJson() const { return json(Value).dump(); }

using namespace ImGui;

template<typename T> void PrimitiveVector<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 i = 0; i < Value.size(); i++) {
            FlashUpdateRecencyBackground(std::to_string(i));
            T value = Value[i];
            TreeNode(std::to_string(i), false, std::format("{}", value).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct PrimitiveVector<bool>;
template struct PrimitiveVector<int>;
template struct PrimitiveVector<u32>;
template struct PrimitiveVector<float>;
template struct PrimitiveVector<std::string>;
