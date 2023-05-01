#include "StateMember.h"

#include "Helper/String.h"

#include "fmt/format.h"
#include "imgui_internal.h" // Only needed for `ImHashStr`.

using namespace StringHelper;

StateMember::StateMember(StateMember *parent, string_view path_segment, std::pair<string_view, string_view> name_help)
    : Parent(parent),
      PathSegment(path_segment),
      Path(Parent && !PathSegment.empty() ? (Parent->Path / PathSegment) : (Parent ? Parent->Path : (!PathSegment.empty() ? StatePath(PathSegment) : RootPath))),
      Name(name_help.first.empty() ? PathSegment.empty() ? "" : PascalToSentenceCase(PathSegment) : name_help.first),
      Help(name_help.second),
      ImGuiLabel(Name.empty() ? "" : fmt::format("{}##{}", Name, PathSegment)),
      Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0)) {
    if (parent) parent->Children.emplace_back(this);
    WithId[Id] = this;
}

StateMember::StateMember(StateMember *parent, string_view path_segment, string_view name_help) : StateMember(parent, path_segment, ParseHelpText(name_help)) {}

StateMember::~StateMember() {
    WithId.erase(Id);
}
