#include "Primitive.h"

#include "Core/Store/Store.h"
#include "PrimitiveActionQueuer.h"

template<typename T> bool Primitive<T>::Exists() const { return RootStore.Count<T>(Id); }
template<typename T> T Primitive<T>::Get() const { return RootStore.Get<T>(Id); }

template<typename T> json Primitive<T>::ToJson() const { return Value; }
template<typename T> void Primitive<T>::SetJson(json &&j) const { Set(std::move(j)); }

template<typename T> void Primitive<T>::Set(const T &value) const { RootStore.Set(Id, value); }
template<typename T> void Primitive<T>::Set(T &&value) const { RootStore.Set(Id, std::move(value)); }
template<typename T> void Primitive<T>::Erase() const { RootStore.Erase<T>(Id); }

template<typename T> void Primitive<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

template<typename T> void Primitive<T>::IssueSet(const T &value) const { PrimitiveQ.QueueSet(Id, value); };

// Explicit instantiations.
template struct Primitive<bool>;
template struct Primitive<int>;
template struct Primitive<u32>;
template struct Primitive<float>;
template struct Primitive<std::string>;
