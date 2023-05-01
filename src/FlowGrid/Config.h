#pragma once

#include <__filesystem/path.h>
#include <map>
#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <set>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;
namespace views = ranges::views;
using std::string, ranges::to, views::transform;

inline static const fs::path InternalPath = ".flowgrid";

enum ProjectFormat {
    StateFormat,
    ActionFormat
};

inline static const std::map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
inline static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
inline static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(ActionFormat));

inline static const auto ProjectFormatForExtension = ExtensionForProjectFormat | transform([](const auto &p) { return std::pair(p.second, p.first); }) | to<std::map>();
inline static const auto AllProjectExtensions = views::keys(ProjectFormatForExtension) | to<std::set>;
inline static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
inline static const string PreferencesFileExtension = ".flp";
inline static const string FaustDspFileExtension = ".dsp";
