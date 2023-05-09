#pragma once

#include <range/v3/core.hpp>
#include <string>
#include <vector>

using std::string, std::string_view, std::vector;
using namespace std::string_literals;
namespace views = ranges::views;
using ranges::to;

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

// Only matches first occurrence (assumes at most one match per match word).
static constexpr vector<std::pair<size_t, size_t>> FindRangesMatching(string_view str, const vector<string> &match_words) {
    vector<std::pair<size_t, size_t>> matching_ranges;
    for (const auto &match_word : match_words) {
        size_t found = str.find(match_word);
        if (found != string::npos) matching_ranges.emplace_back(found, found + match_word.size());
    }
    return matching_ranges;
}

// Doesn't change contiguous capital ranges like "ID".
// Doesn't modify the first occurrences of any words in the provided `skip_words` list.
// Uppercases the first occurrences of all words in the provided `all_caps_words`.
// E.g. 'FooBarFlowGridId' => 'Foo bar FlowGrid ID'
static const vector<string> SkipWords{"FlowGrid", "ImGui", "ImPlot"};
static const vector<string> AllCapsWords{"Id"};
static constexpr string PascalToSentenceCase(string_view str, const vector<string> &skip_words = SkipWords, const vector<string> &all_caps_words = AllCapsWords) {
    const auto skip_ranges = FindRangesMatching(str, skip_words);
    const auto all_caps_ranges = FindRangesMatching(str, all_caps_words);

    // Mutable vars:
    auto skip_range_it = skip_ranges.begin();
    auto caps_range_it = all_caps_ranges.begin();
    string sentence_case;

    for (size_t index = 0; index < str.size(); index++) {
        if (skip_range_it != skip_ranges.end() && index > (*skip_range_it).second) skip_range_it++;
        if (caps_range_it != all_caps_ranges.end() && index > (*caps_range_it).second) caps_range_it++;

        const char ch = str[index];
        if (isupper(ch) && islower(str[index - 1]) && (skip_range_it == skip_ranges.end() || index == (*skip_range_it).first)) sentence_case += ' ';

        const bool in_skip_range = skip_range_it != skip_ranges.end() && index >= (*skip_range_it).first && index < (*skip_range_it).second;
        const bool in_caps_range = caps_range_it != all_caps_ranges.end() && index >= (*caps_range_it).first && index < (*caps_range_it).second;
        sentence_case += in_caps_range ? toupper(ch) : ((index > 0 && !in_skip_range) ? tolower(ch) : ch);
    }
    return sentence_case;
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

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static inline std::pair<string_view, string_view> ParseHelpText(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

static inline vector<string> Split(const string &text, const char *delims) {
    vector<string> tokens;
    size_t start = text.find_first_not_of(delims), end;
    while ((end = text.find_first_of(delims, start)) != string::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(delims, end);
    }
    if (start != string::npos) {
        tokens.push_back(text.substr(start));
    }
    return tokens;
}

} // namespace StringHelper
