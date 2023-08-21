#pragma once

#include <string>
#include <vector>

struct NamesAndValues {
    size_t Size() const { return names.size(); }

    std::vector<std::string> names{};
    std::vector<double> values{};
};
