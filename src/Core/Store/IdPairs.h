#pragma once

#include <functional> // std::hash
#include <string>
#include <string_view>

#include "immer/set.hpp"

using ID = unsigned int;
using IdPair = std::pair<ID, ID>;

IdPair DeserializeIdPair(const std::string &);
std::string SerializeIdPair(const IdPair &);

struct IdPairHash {
    // Common hash shift trick: https://en.cppreference.com/w/cpp/utility/hash
    auto operator()(const IdPair &p) const noexcept { return std::hash<ID>()(p.first) ^ (std::hash<ID>()(p.second) << 1); }
};

using IdPairs = immer::set<IdPair, IdPairHash>;
