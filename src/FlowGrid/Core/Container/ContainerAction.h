#pragma once

#include "AdjacencyListAction.h"
#include "NavigableAction.h"
#include "SetAction.h"
#include "Vec2Action.h"
#include "VectorAction.h"

namespace Action {
namespace Container {
using Any = Combine<
    AdjacencyList::Any, Navigable<u32>::Any, Vec2::Any,
    Set<u32>::Any,
    Vector<bool>::Any, Vector<int>::Any, Vector<u32>::Any, Vector<float>::Any, Vector<std::string>::Any>;
} // namespace Container
} // namespace Action
