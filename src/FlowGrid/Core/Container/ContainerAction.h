#pragma once

#include "AdjacencyListAction.h"
#include "NavigableAction.h"
#include "PrimitiveSetAction.h"
#include "PrimitiveVectorAction.h"
#include "Vec2Action.h"

namespace Action {
namespace Container {
using Any = Combine<
    AdjacencyList::Any, Navigable<u32>::Any, Vec2::Any,
    PrimitiveSet<u32>::Any,
    PrimitiveVector<bool>::Any, PrimitiveVector<int>::Any, PrimitiveVector<u32>::Any, PrimitiveVector<float>::Any, PrimitiveVector<std::string>::Any>;
} // namespace Container
} // namespace Action
