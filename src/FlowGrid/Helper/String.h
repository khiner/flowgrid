#pragma once

#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <string>

using std::string;
namespace views = ranges::views;
using ranges::to;
using views::transform;

namespace StringHelper {
static constexpr string Capitalize(const string &str) {
    if (str.empty()) return "";

    string copy = str;
    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

static constexpr string Lowercase(const string &str) {
    if (str.empty()) return "";

    string copy = str;
    copy[0] = tolower(copy[0], std::locale());
    return copy;
}

// E.g. 'foo_bar_baz' => 'Foo bar baz'
static constexpr string SnakeCaseToSentenceCase(const string &snake_case) {
    auto sentence_case = snake_case | views::split('_') | views::join(' ') | to<string>;
    return Capitalize(sentence_case);
}

static constexpr bool IsInteger(const string &str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }

static constexpr string Replace(string subject, const string &search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

// Same as above, but with single char search.
static constexpr string Replace(string subject, const char search, const string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
    return subject;
}
} // namespace StringHelper
