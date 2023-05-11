#pragma once

#include <__filesystem/path.h>
#include <map>
#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <set>
#include <string>

#include "Store/StoreJsonFormat.h"

namespace views = ranges::views;
using ranges::to;

inline static const std::filesystem::path InternalPath = ".flowgrid";
inline static const std::string FaustDspFileExtension = ".dsp";

inline static const std::map<StoreJsonFormat, std::string> ExtensionForStoreJsonFormat{{StoreJsonFormat::StateFormat, ".fls"}, {StoreJsonFormat::ActionFormat, ".fla"}};
inline static const auto StoreJsonFormatForExtension = ExtensionForStoreJsonFormat | views::transform([](const auto &p) { return std::pair(p.second, p.first); }) | to<std::map>();

inline static const std::set<std::string> AllProjectExtensions = views::keys(StoreJsonFormatForExtension) | to<std::set>;
inline static const std::string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<std::string>;

inline static const std::filesystem::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForStoreJsonFormat.at(StoreJsonFormat::StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
inline static const std::filesystem::path DefaultProjectPath = InternalPath / ("default" + ExtensionForStoreJsonFormat.at(StoreJsonFormat::ActionFormat));
