#include "Primitive.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> bool Primitive<T>::Exists() const { return RootStore.Exists(Path); }
template<IsPrimitive T> T Primitive<T>::Get() const { return std::get<T>(RootStore.Get(Path)); }

template<IsPrimitive T> json Primitive<T>::ToJson() const { return Value; }
template<IsPrimitive T> void Primitive<T>::SetJson(json &&j) const { Set(std::move(j)); }

template<IsPrimitive T> void Primitive<T>::Set(const T &value) const { RootStore.Set(Path, value); }
template<IsPrimitive T> void Primitive<T>::Set(T &&value) const { RootStore.Set(Path, std::move(value)); }
template<IsPrimitive T> void Primitive<T>::Erase() const { RootStore.Erase(Path); }

template<IsPrimitive T> void Primitive<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

// Explicit instantiations.
template struct Primitive<bool>;
template struct Primitive<int>;
template struct Primitive<u32>;
template struct Primitive<float>;
template struct Primitive<std::string>;
