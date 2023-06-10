#pragma once

#include <map>
#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <set>
#include <string>

#include "Helper/Path.h"
#include "ProjectJsonFormat.h"

namespace views = ranges::views;

inline static const fs::path InternalPath = ".flowgrid";

inline static const std::map<ProjectJsonFormat, std::string> ExtensionForProjectJsonFormat{{ProjectJsonFormat::StateFormat, ".fls"}, {ProjectJsonFormat::ActionFormat, ".fla"}};
inline static const auto ProjectJsonFormatForExtension = ExtensionForProjectJsonFormat | views::transform([](const auto &p) { return std::pair(p.second, p.first); }) | ranges::to<std::map>();

inline static const std::set<std::string> AllProjectExtensions = views::keys(ProjectJsonFormatForExtension) | ranges::to<std::set>;
inline static const std::string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | ranges::to<std::string>;

inline static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectJsonFormat.at(ProjectJsonFormat::StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
inline static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectJsonFormat.at(ProjectJsonFormat::ActionFormat));
