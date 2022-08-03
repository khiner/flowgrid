#pragma once

#include <string>
#include "range/v3/view.hpp"

using std::string;
namespace views = ranges::views;
using views::transform;
using ranges::to;

// E.g. 'foo_bar_baz' => 'Foo bar baz'
inline string snake_case_to_sentence_case(const string &snake_case) {
    auto sentence_case = snake_case | views::split('_') | views::join(' ') | to<string>;
    sentence_case[0] = toupper(sentence_case[0], std::locale());
    return sentence_case;
}

// E.g. '/foo/bar/baz' => 'baz'
inline string path_variable_name(const string &path) {
    return (path | views::split('/') | to<std::vector<string>>).back();
}

inline string path_label(const string &path) { return snake_case_to_sentence_case(path_variable_name(path)); }

constexpr inline bool is_integer(const string &str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }
