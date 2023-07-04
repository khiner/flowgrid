#pragma once

#include <unordered_set>

#include "Path.h"
#include "Time.h"

using UniquePaths = std::unordered_set<StorePath, PathHash>;
using PathsMoment = std::pair<TimePoint, UniquePaths>;
