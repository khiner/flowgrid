#include "PrimitiveField.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> T PrimitiveField<T>::Get() const { return std::get<T>(RootStore.Get(Path)); }

template<IsPrimitive T> void PrimitiveField<T>::Set(const T &value) const { RootStore.Set(Path, value); }
template<IsPrimitive T> void PrimitiveField<T>::Set(T &&value) const { RootStore.Set(Path, std::move(value)); }
template<IsPrimitive T> void PrimitiveField<T>::SetJson(json &&j) const { Set(std::move(j)); }
template<IsPrimitive T> void PrimitiveField<T>::Erase() const { RootStore.Erase(Path); }

template<IsPrimitive T> json PrimitiveField<T>::ToJson() const { return Value; }

template<IsPrimitive T> void PrimitiveField<T>::RenderValueTree(bool annotate, bool auto_select) const {
    Field::RenderValueTree(annotate, auto_select);
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

// Explicit instantiations.
template struct PrimitiveField<bool>;
template struct PrimitiveField<int>;
template struct PrimitiveField<u32>;
template struct PrimitiveField<float>;
template struct PrimitiveField<std::string>;
