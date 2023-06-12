#include "Action.h"

#include <regex>

#include "Helper/String.h"

namespace Action {
Metadata::Parsed Metadata::Parse(string_view meta_str) {
    static const std::regex pattern("(~([^@]*))?(@(.*))?");
    string meta_str_std(meta_str);
    static std::smatch matches;
    if (!meta_str_std.empty() && std::regex_search(meta_str_std, matches, pattern)) {
        return {matches[2].str(), matches[4].str()};
    }
    return {"", ""};
}
Metadata::Metadata(string_view path_leaf, Metadata::Parsed parsed)
    : PathLeaf(path_leaf),
      Name(StringHelper::PascalToSentenceCase(path_leaf)),
      MenuLabel(parsed.MenuLabel.empty() ? Name : parsed.MenuLabel),
      Shortcut(parsed.Shortcut) {}

Metadata::Metadata(string_view path_leaf, string_view meta_str)
    : Metadata(path_leaf, Parse(meta_str)) {}
} // namespace Action
