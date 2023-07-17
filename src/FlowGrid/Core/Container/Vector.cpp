#include "Vector.h"

#include "imgui.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> void Vector<T>::Set(const std::vector<T> &value) const {
    u32 i = 0;
    while (i < value.size()) {
        Set(i, value[i]);
        i++;
    }
    Resize(i);
}

template<IsPrimitive T> void Vector<T>::Set(size_t i, const T &value) const {
    RootStore.Set(PathAt(i), value);
}

template<IsPrimitive T> void Vector<T>::Set(const std::vector<std::pair<int, T>> &values) const {
    for (const auto &[i, value] : values) Set(i, value);
}

template<IsPrimitive T> void Vector<T>::Resize(u32 size) const {
    u32 i = size;
    while (RootStore.CountAt(PathAt(i))) {
        RootStore.Erase(PathAt(i));
        i++;
    }
}

template<IsPrimitive T> void Vector<T>::RefreshValue() {
    u32 i = 0;
    while (RootStore.CountAt(PathAt(i))) {
        const T value = std::get<T>(RootStore.Get(PathAt(i)));
        if (Value.size() == i) Value.push_back(value);
        else Value[i] = value;
        i++;
    }
    Value.resize(i);
}

template<IsPrimitive T> void Vector<T>::SetJson(json &&j) const {
    std::vector<T> new_value = json::parse(std::string(std::move(j)));
    Set(std::move(new_value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<IsPrimitive T> json Vector<T>::ToJson() const { return json(Value).dump(); }

// Explicit instantiations.
using namespace ImGui;

template<IsPrimitive T> void Vector<T>::RenderValueTree(bool annotate, bool auto_select) const {
    Field::RenderValueTree(annotate, auto_select);

    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        for (u32 i = 0; i < Value.size(); i++) {
            T value = Value[i];
            TreeNode(to_string(i), false, to_string(value).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Vector<bool>;
template struct Vector<int>;
template struct Vector<u32>;
template struct Vector<float>;
