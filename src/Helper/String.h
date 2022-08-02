#pragma once

#include <string>
#include "range/v3/view.hpp"

namespace views = ranges::views;
using ranges::to;

using std::string;

inline string snake_case_to_sentence_case(const string &snake_case) {
    auto spaced = snake_case | views::split('_') | views::join(' ') | to<std::string>;
    spaced[0] = toupper(spaced[0]);
    return spaced;
}

inline string path_variable_name(const string &path) {
    const auto res = path | views::split('/') | to<std::vector<string>>;
    return res.back();
}

inline string path_label(const string &path) { return snake_case_to_sentence_case(path_variable_name(path)); }

inline bool is_number(const string &str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}
