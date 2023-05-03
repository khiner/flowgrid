#include "AppPreferences.h"

#include "nlohmann/json.hpp"

#include "Helper/File.h"

using namespace nlohmann;

AppPreferences::AppPreferences() {
    if (fs::exists(Path)) {
        const json js = json::parse(FileIO::read(Path));
        RecentlyOpenedPaths = js["RecentlyOpenedPaths"].get<std::list<fs::path>>();
    } else {
        Write();
    }
}

bool AppPreferences::Write() const {
    json js;
    js["RecentlyOpenedPaths"] = RecentlyOpenedPaths;

    return FileIO::write(Path, js.dump());
}

bool AppPreferences::Clear() {
    RecentlyOpenedPaths.clear();
    return Write();
}

void AppPreferences::SetCurrentProjectPath(const fs::path &path) {
    RecentlyOpenedPaths.remove(path);
    RecentlyOpenedPaths.emplace_front(path);
    Write();
}

AppPreferences Preferences{};
