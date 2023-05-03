#include "AppPreferences.h"

#include "Helper/File.h"
#include "StateJson.h"

AppPreferences Preferences{};

AppPreferences::AppPreferences() {
    static bool loading_from_json = false;
    if (loading_from_json) return;

    if (fs::exists(Path)) {
        loading_from_json = true;
        *this = json::parse(FileIO::read(Path));
        loading_from_json = false;
    } else {
        Write();
    }
}

bool AppPreferences::Write() const {
    return FileIO::write(Path, json(Preferences).dump());
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
