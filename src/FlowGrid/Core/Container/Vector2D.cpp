#include "Vector2D.h"

#include "imgui.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> void Vector2D<T>::Set(const std::vector<std::vector<T>> &value) const {
    Count i = 0;
    while (i < value.size()) {
        Count j = 0;
        while (j < value[i].size()) {
            Set(i, j, value[i][j]);
            j++;
        }
        while (RootStore.CountAt(PathAt(i, j))) RootStore.Erase(PathAt(i, j++));
        i++;
    }

    Resize(i);
}

template<IsPrimitive T> void Vector2D<T>::Resize(Count size) const {
    Count i = size;
    while (RootStore.CountAt(PathAt(i, 0))) {
        Resize(i, 0);
        i++;
    }
}

template<IsPrimitive T> void Vector2D<T>::Resize(Count i, Count size) const {
    Count j = size;
    while (RootStore.CountAt(PathAt(i, j))) RootStore.Erase(PathAt(i, j++));
}

template<IsPrimitive T> void Vector2D<T>::Set(Count i, Count j, const T &value) const {
    RootStore.Set(PathAt(i, j), value);
}

template<IsPrimitive T> void Vector2D<T>::RefreshValue() {
    Count i = 0;
    while (RootStore.CountAt(PathAt(i, 0))) {
        if (Value.size() == i) Value.push_back({});
        Count j = 0;
        while (RootStore.CountAt(PathAt(i, j))) {
            const T value = std::get<T>(RootStore.Get(PathAt(i, j)));
            if (Value[i].size() == j) Value[i].push_back(value);
            else Value[i][j] = value;
            j++;
        }
        Value[i].resize(j);
        i++;
    }
    Value.resize(i);
}

using namespace ImGui;

template<IsPrimitive T> void Vector2D<T>::RenderValueTree(bool annotate, bool auto_select) const {
    Field::RenderValueTree(annotate, auto_select);

    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        for (Count i = 0; i < Value.size(); i++) {
            if (TreeNode(to_string(i))) {
                for (Count j = 0; j < Value.size(); j++) {
                    T value = Value[i][j];
                    TreeNode(to_string(j), false, to_string(value).c_str());
                }
                TreePop();
            }
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Vector2D<bool>;
template struct Vector2D<int>;
template struct Vector2D<U32>;
template struct Vector2D<float>;
