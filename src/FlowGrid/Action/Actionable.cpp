#include "Actionable.h"

#include <regex>

#include "../Helper/String.h"

namespace Actionable {

Metadata::Parsed Metadata::ParseMetadata(string_view meta_str) {
    static const std::regex pattern("(!)?(~([^@]*))?(@(.*))?");
    string meta_str_std(meta_str);
    static std::smatch matches;
    if (!meta_str_std.empty() && std::regex_search(meta_str_std, matches, pattern)) {
        return {matches[3].str(), matches[5].str(), matches[1].str().empty()};
    }
    return {"", "", true};
}

Metadata::Metadata(string_view name, string_view meta_str) : Metadata(name, ParseMetadata(meta_str)) {}

Metadata::Metadata(string_view name, Metadata::Parsed parsed)
    : Name(StringHelper::PascalToSentenceCase(name)),
      MenuLabel(parsed.MenuLabel.empty() ? Name : parsed.MenuLabel),
      Shortcut(parsed.Shortcut), Savable(parsed.Savable) {}
} // namespace Actionable
