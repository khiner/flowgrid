#include "Component.h"

#include "imgui_internal.h" // Only needed for `ImHashStr`.
#include <format>

#include "Helper/String.h"
#include "UI/HelpMarker.h"

std::pair<string_view, string_view> ParseHelpText(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

Component::Component(Component *parent, string_view path_leaf, std::pair<string_view, string_view> meta_str)
    : Parent(parent),
      PathLeaf(path_leaf),
      Path(Parent && !PathLeaf.empty() ? (Parent->Path / PathLeaf) : (Parent ? Parent->Path : (!PathLeaf.empty() ? StorePath(PathLeaf) : RootPath))),
      Name(meta_str.first.empty() ? PathLeaf.empty() ? "" : StringHelper::PascalToSentenceCase(PathLeaf) : meta_str.first),
      Help(meta_str.second),
      ImGuiLabel(Name.empty() ? "" : std::format("{}##{}", Name, PathLeaf)),
      Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0)) {
    if (parent) parent->Children.emplace_back(this);
    WithId[Id] = this;
}

Component::Component(Component *parent, string_view path_leaf, string_view meta_str)
    : Component(parent, path_leaf, ParseHelpText(meta_str)) {}

Component::~Component() {
    WithId.erase(Id);
}

// Currently, `Draw` is not used for anything except wrapping around `Render`.
// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void Component::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) ImGui::SameLine();
    fg::HelpMarker(Help.c_str());
    if (!after) ImGui::SameLine();
}

// Fields don't wrap their `Render` with a push/pop-id, ImGui widgets all push the provided label to the ID stack.
void Drawable::Draw() const {
    //    PushID(ImGuiLabel.c_str());
    Render();
    //    PopID();
}
