#include "PrimitiveField.h"

#include "Core/Store/Store.h"

template<IsPrimitive T> T PrimitiveField<T>::Get() const { return std::get<T>(store::Get(Path)); }

// Explicit instantiations for `Get`.
template bool PrimitiveField<bool>::Get() const;
template int PrimitiveField<int>::Get() const;
template U32 PrimitiveField<U32>::Get() const;
template float PrimitiveField<float>::Get() const;
template std::string PrimitiveField<std::string>::Get() const;

template<IsPrimitive T> void PrimitiveField<T>::Set(const T &value) const { store::Set(Path, value); }

// Explicit instantiations for `Set`.
template void PrimitiveField<bool>::Set(const bool &) const;
template void PrimitiveField<int>::Set(const int &) const;
template void PrimitiveField<U32>::Set(const U32 &) const;
template void PrimitiveField<float>::Set(const float &) const;
template void PrimitiveField<std::string>::Set(const std::string &) const;
