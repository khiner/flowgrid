#pragma once

#include <locale>
#include <string>
#include <vector>

using std::string, std::string_view;
using namespace std::string_literals;

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

static constexpr bool IsInteger(string_view str) { return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit); }

static constexpr string Replace(string subject, string_view search, string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

// Same as above, but with single char search.
static constexpr string Replace(string subject, const char search, string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != string::npos) {
        subject.replace(pos, 1, replace);
        pos += replace.length();
    }
    return subject;
}

std::vector<string> Split(const string &text, const char *delims);

// Doesn't change contiguous capital ranges like "ID".
// Doesn't modify the first occurrences of any words in the provided `skip_words` list.
// Uppercases the first occurrences of all words in the provided `all_caps_words`.
// E.g. 'FooBarFlowGridId' => 'Foo bar FlowGrid ID'
inline static const std::vector<string> SkipWords{"FlowGrid", "ImGui", "ImPlot", "Faust"};
inline static const std::vector<string> AllCapsWords{"Id", "Svg", "Dsp"};
string PascalToSentenceCase(string_view str, const std::vector<string> &skip_words = SkipWords, const std::vector<string> &all_caps_words = AllCapsWords);

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
std::pair<string_view, string_view> ParseHelpText(string_view str);
} // namespace StringHelper
