#pragma once

#include <string>
#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/split.hpp>

using std::string;
namespace views = ranges::views;
using views::transform;
using ranges::to;

inline static string capitalize(const string &str) {
    if (str.empty()) return "";

    string copy = str;
    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

inline static string lowercase(const string &str) {
    if (str.empty()) return "";

    string copy = str;
    copy[0] = tolower(copy[0], std::locale());
    return copy;
}

// E.g. 'foo_bar_baz' => 'Foo bar baz'
inline static string SnakeCaseToSentenceCase(const string &snake_case) {
    auto sentence_case = snake_case | views::split('_') | views::join(' ') | to<string>;
    return capitalize(sentence_case);
}

constexpr inline static bool is_integer(const string &str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }

inline static string replace(string subject, const string &search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

// Same as above, but with single char search.
inline static string replace(string subject, const char search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
    return subject;
}
