#include "Stateful.h"

#include "imgui_internal.h" // Only needed for `ImHashStr`.
#include <format>

#include "Helper/String.h"
#include "UI/Widgets.h"

std::pair<string_view, string_view> ParseHelpText(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

namespace Stateful {
Base::Base(Base *parent, string_view path_segment, std::pair<string_view, string_view> name_help)
    : Parent(parent),
      PathSegment(path_segment),
      Path(Parent && !PathSegment.empty() ? (Parent->Path / PathSegment) : (Parent ? Parent->Path : (!PathSegment.empty() ? StorePath(PathSegment) : RootPath))),
      Name(name_help.first.empty() ? PathSegment.empty() ? "" : StringHelper::PascalToSentenceCase(PathSegment) : name_help.first),
      Help(name_help.second),
      ImGuiLabel(Name.empty() ? "" : std::format("{}##{}", Name, PathSegment)),
      Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0)) {
    if (parent) parent->Children.emplace_back(this);
    WithId[Id] = this;
}

Base::Base(Base *parent, string_view path_segment, string_view name_help)
    : Base(parent, path_segment, ParseHelpText(name_help)) {}

Base::~Base() {
    WithId.erase(Id);
}

// Currently, `Draw` is not used for anything except wrapping around `Render`.
// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void Base::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) ImGui::SameLine();
    fg::HelpMarker(Help.c_str());
    if (!after) ImGui::SameLine();
}

namespace Field {
Base::Base(Stateful::Base *parent, string_view path_segment, string_view name_help)
    : Stateful::Base(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Base::~Base() {
    WithPath.erase(Path);
}
} // namespace Field
} // namespace Stateful

// Fields don't wrap their `Render` with a push/pop-id, ImGui widgets all push the provided label to the ID stack.
void Drawable::Draw() const {
    //    PushID(ImGuiLabel.c_str());
    Render();
    //    PopID();
}
