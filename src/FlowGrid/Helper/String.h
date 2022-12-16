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
static constexpr string Capitalize(string copy) {
    if (copy.empty()) return "";

    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

static constexpr string Lowercase(string copy) {
    if (copy.empty()) return "";

    copy[0] = tolower(copy[0], std::locale());
    return copy;
}

// E.g. 'foo_bar_baz' => 'Foo bar baz'
static constexpr string SnakeCaseToSentenceCase(std::string_view snake_case) {
    auto sentence_case = snake_case | views::split('_') | views::join(' ') | to<string>;
    return Capitalize(sentence_case);
}

static constexpr bool IsInteger(std::string_view str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }

static constexpr string Replace(string subject, std::string_view search, std::string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

// Same as above, but with single char search.
static constexpr string Replace(string subject, const char search, std::string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
    return subject;
}
} // namespace StringHelper
