#include "PrimitiveField.h"

#include "Core/Store/Store.h"

#include "UI/Widgets.h"

template<IsPrimitive T> T PrimitiveField<T>::Get() const { return std::get<T>(store::Get(Path)); }
template<IsPrimitive T> void PrimitiveField<T>::Set(const T &value) const { store::Set(Path, value); }

template<IsPrimitive T> void PrimitiveField<T>::RenderValueTree(ValueTreeLabelMode mode, bool auto_select) const {
    Field::RenderValueTree(mode, auto_select);
    fg::TreeNode(Name, 0, nullptr, std::format("{}", Value).c_str());
}

// Explicit instantiations.
template struct PrimitiveField<bool>;
template struct PrimitiveField<int>;
template struct PrimitiveField<U32>;
template struct PrimitiveField<float>;
template struct PrimitiveField<std::string>;
