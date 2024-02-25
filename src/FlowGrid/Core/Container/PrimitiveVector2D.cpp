#include "PrimitiveVector2D.h"

#include "imgui.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> void PrimitiveVector2D<T>::Set(const std::vector<std::vector<T>> &value) const {
    u32 i = 0;
    while (i < value.size()) {
        u32 j = 0;
        while (j < value[i].size()) {
            Set(i, j, value[i][j]);
            j++;
        }
        while (RootStore.CountAt<PrimitiveVariant>(PathAt(i, j))) RootStore.Erase<PrimitiveVariant>(PathAt(i, j++));
        i++;
    }

    Resize(i);
}

template<IsPrimitive T> void PrimitiveVector2D<T>::Set(u32 i, u32 j, const T &value) const {
    RootStore.Set(PathAt(i, j), PrimitiveVariant{value});
}

template<IsPrimitive T> void PrimitiveVector2D<T>::Resize(u32 size) const {
    u32 i = size;
    while (RootStore.CountAt<PrimitiveVariant>(PathAt(i, 0))) {
        Resize(i, 0);
        i++;
    }
}

template<IsPrimitive T> void PrimitiveVector2D<T>::Resize(u32 i, u32 size) const {
    u32 j = size;
    while (RootStore.CountAt<PrimitiveVariant>(PathAt(i, j))) RootStore.Erase<PrimitiveVariant>(PathAt(i, j++));
}

template<IsPrimitive T> void PrimitiveVector2D<T>::Erase() const { Resize(0); }

template<IsPrimitive T> void PrimitiveVector2D<T>::Refresh() {
    u32 i = 0;
    while (RootStore.CountAt<PrimitiveVariant>(PathAt(i, 0))) {
        if (Value.size() == i) Value.push_back({});
        u32 j = 0;
        while (RootStore.CountAt<PrimitiveVariant>(PathAt(i, j))) {
            const T value = std::get<T>(RootStore.Get<PrimitiveVariant>(PathAt(i, j)));
            if (Value[i].size() == j) Value[i].push_back(value);
            else Value[i][j] = value;
            j++;
        }
        Value[i].resize(j);
        i++;
    }
    Value.resize(i);
}

template<IsPrimitive T> void PrimitiveVector2D<T>::SetJson(json &&j) const {
    std::vector<std::vector<T>> new_value = json::parse(std::string(std::move(j)));
    Set(std::move(new_value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<IsPrimitive T> json PrimitiveVector2D<T>::ToJson() const { return json(Value).dump(); }

using namespace ImGui;

template<IsPrimitive T> void PrimitiveVector2D<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        for (u32 i = 0; i < Value.size(); i++) {
            if (TreeNode(to_string(i))) {
                for (u32 j = 0; j < Value.size(); j++) {
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
template struct PrimitiveVector2D<bool>;
template struct PrimitiveVector2D<int>;
template struct PrimitiveVector2D<u32>;
template struct PrimitiveVector2D<float>;
