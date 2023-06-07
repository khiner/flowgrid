#include "Action.h"

#include <regex>

#include "Helper/String.h"

namespace Action {
Metadata::Parsed Metadata::ParseMetadata(string_view meta_str) {
    static const std::regex pattern("(~([^@]*))?(@(.*))?");
    string meta_str_std(meta_str);
    static std::smatch matches;
    if (!meta_str_std.empty() && std::regex_search(meta_str_std, matches, pattern)) {
        return {matches[2].str(), matches[4].str()};
    }
    return {"", ""};
}
Metadata::Metadata(fs::path type_path, string_view name, Metadata::Parsed parsed)
    : TypePath(type_path),
      Name(StringHelper::PascalToSentenceCase(name)),
      MenuLabel(parsed.MenuLabel.empty() ? Name : parsed.MenuLabel),
      Shortcut(parsed.Shortcut) {}
Metadata::Metadata(fs::path type_path, string_view name, string_view meta_str) : Metadata(type_path, name, ParseMetadata(meta_str)) {}
} // namespace Action
