#pragma once

#include <map>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <set>

namespace views = ranges::views;
using ranges::to;

inline static const unordered_map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
inline static const auto ProjectFormatForExtension = ExtensionForProjectFormat | transform([](const auto &p) { return pair(p.second, p.first); }) | to<std::map>();
inline static const auto AllProjectExtensions = views::keys(ProjectFormatForExtension) | to<std::set>;
inline static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
inline static const string PreferencesFileExtension = ".flp";
inline static const string FaustDspFileExtension = ".dsp";

inline static const fs::path InternalPath = ".flowgrid";
inline static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
inline static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(ActionFormat));
inline static const fs::path PreferencesPath = InternalPath / ("Preferences" + PreferencesFileExtension);
