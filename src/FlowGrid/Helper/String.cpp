#include "String.h"

#include <range/v3/core.hpp>

using std::vector;

namespace views = ranges::views;
using ranges::to;

namespace StringHelper {
vector<string> Split(const string &text, const char *delims) {
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

// Only matches first occurrence (assumes at most one match per match word).
static constexpr vector<std::pair<size_t, size_t>> FindRangesMatching(string_view str, const vector<string> &match_words) {
    vector<std::pair<size_t, size_t>> matching_ranges;
    for (const auto &match_word : match_words) {
        const size_t pos = str.find(match_word);
        if (pos != string::npos) matching_ranges.emplace_back(pos, pos + match_word.size());
    }
    return matching_ranges;
}

string PascalToSentenceCase(string_view str, const vector<string> &skip_words, const vector<string> &all_caps_words) {
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

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
std::pair<string_view, string_view> ParseHelpText(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}
} // namespace StringHelper
