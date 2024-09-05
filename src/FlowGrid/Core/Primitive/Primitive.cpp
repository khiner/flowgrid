#include "Primitive.h"

#include "Core/Store/Store.h"
#include "PrimitiveActionQueuer.h"

template<typename T> bool Primitive<T>::Exists() const { return S.Count<T>(Id); }
template<typename T> T Primitive<T>::Get() const { return S.Get<T>(Id); }

template<typename T> json Primitive<T>::ToJson() const { return Value; }
template<typename T> void Primitive<T>::SetJson(json &&j) const { Set(std::move(j)); }

template<typename T> void Primitive<T>::Set(const T &value) const { S.Set(Id, value); }
template<typename T> void Primitive<T>::Set(T &&value) const { S.Set(Id, std::move(value)); }
template<typename T> void Primitive<T>::Erase() const { S.Erase<T>(Id); }

template<typename T> void Primitive<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

template<> void Primitive<u32>::IssueSet(const u32 &value) const { PrimitiveQ(Action::Primitive::UInt::Set{Id, value}); };
template<> void Primitive<s32>::IssueSet(const s32 &value) const { PrimitiveQ(Action::Primitive::Int::Set{Id, value}); };
template<> void Primitive<float>::IssueSet(const float &value) const { PrimitiveQ(Action::Primitive::Float::Set{Id, value}); };
template<> void Primitive<std::string>::IssueSet(const std::string &value) const { PrimitiveQ(Action::Primitive::String::Set{Id, value}); };

// Explicit instantiations.
template struct Primitive<bool>;
template struct Primitive<int>;
template struct Primitive<u32>;
template struct Primitive<float>;
template struct Primitive<std::string>;
