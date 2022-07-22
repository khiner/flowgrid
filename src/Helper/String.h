#pragma once

#include <string>
#include "range/v3/view.hpp"

using std::string;

inline string snake_case_to_sentence_case(const string &snake_case) {
    using namespace ranges;
    auto spaced = snake_case | views::split('_') | views::join(' ') | to<std::string>();
    spaced[0] = toupper(spaced[0]);
    return spaced;
}

inline string path_variable_name(const string &path) {
    using namespace ranges;
    const auto res = path | views::split('/') | to<std::vector<string>>();
    return res.back();
}

inline string path_label(const JsonPath &path) { return snake_case_to_sentence_case(path_variable_name(path)); }
