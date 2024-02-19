#include "HelpInfo.h"

using std::string, std::string_view;

HelpInfo HelpInfo::Parse(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? string(str.substr(0, help_split)) : string(str), found ? string(str.substr(help_split + 1)) : ""};
}
