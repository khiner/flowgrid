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

constexpr inline bool is_integer(const string &str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }

inline string replace(string subject, const string &search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

// Same as above, but with single char search.
inline string replace(string subject, const char search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
    return subject;
}
