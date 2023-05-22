#include "Actionable.h"

#include <regex>

#include "../Helper/String.h"

namespace Actionable {
std::pair<string, string> ParseMetadata(string_view meta_str) {
    static const std::regex pattern("(~([^@]*))?(@(.*))?");
    string meta_str_std(meta_str);
    static std::smatch matches;
    if (!meta_str_std.empty() && std::regex_search(meta_str_std, matches, pattern)) {
        return {matches[2].str(), matches[4].str()};
    }
    return {"", ""};
}

// `meta_str` is of the format: "~{menu label}@{shortcut}" (order-independent, prefixes required)
Metadata::Metadata(string_view name, string_view meta_str)
    : Metadata(name, ParseMetadata(meta_str)) {}

Metadata::Metadata(string_view name, std::pair<string, string> menu_label_shortcut)
    : Name(StringHelper::PascalToSentenceCase(name)),
      MenuLabel(menu_label_shortcut.first.empty() ? Name : menu_label_shortcut.first),
      Shortcut(menu_label_shortcut.second) {}
} // namespace Actionable
