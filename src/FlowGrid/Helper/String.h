#pragma once

#include <algorithm>
#include <locale>
#include <ranges>
#include <string>
#include <vector>

using std::string, std::string_view;
using namespace std::string_literals;

namespace StringHelper {
constexpr string Capitalize(string_view str) {
    string copy{str};
    if (copy.empty()) return "";

    copy[0] = toupper(copy[0], std::locale());
    return copy;
}

constexpr void Replace(string &subject, const char search, string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
}

std::vector<string> Split(const string_view &text, const char *delims);

// Doesn't change contiguous capital ranges like "ID".
// Doesn't modify the first occurrences of any words in the provided `skip_words` list.
// Uppercases the first occurrences of all words in the provided `all_caps_words`.
// E.g. if "FlowGrid" is in `skip_words`: 'FooBarFlowGridId' => 'Foo bar FlowGrid ID'
string PascalToSentenceCase(string_view str, const std::vector<string> &skip_words, const std::vector<string> &all_caps_words);

// Use the default `skip_word`s and `all_caps_words`.
inline string PascalToSentenceCase(string_view str) {
    static const std::vector<string> SkipWords{"FlowGrid", "ImGui", "ImPlot", "Faust"};
    static const std::vector<string> AllCapsWords{"Id", "Svg", "Dsp"};
    return PascalToSentenceCase(str, SkipWords, AllCapsWords);
}
} // namespace StringHelper
