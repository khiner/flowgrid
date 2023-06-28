#pragma once

#include <functional>

using IdPair = std::pair<unsigned int, unsigned int>;

struct IdPairHash {
    auto operator()(const IdPair &p) const noexcept {
        // Common shift trick, which can be found in e.g. https://en.cppreference.com/w/cpp/utility/hash
        return std::hash<unsigned int>()(p.first) ^ (std::hash<unsigned int>()(p.second) << 1);
    }
};

IdPair DeserializeIdPair(const std::string &);
std::string SerializeIdPair(const IdPair &);
