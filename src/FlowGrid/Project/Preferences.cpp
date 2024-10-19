#include "Preferences.h"

#include "nlohmann/json.hpp"

#include "Helper/File.h"

using json = nlohmann::json;

Preferences::Preferences() {
    if (fs::exists(Path)) {
        const json js = json::parse(FileIO::read(Path));
        RecentlyOpenedPaths = js["RecentlyOpenedPaths"].get<std::list<fs::path>>();
    } else {
        Write();
    }
}

bool Preferences::Write() const {
    json js;
    js["RecentlyOpenedPaths"] = RecentlyOpenedPaths;

    return FileIO::write(Path, js.dump());
}

bool Preferences::Clear() {
    RecentlyOpenedPaths.clear();
    return Write();
}

void Preferences::OnProjectOpened(const fs::path &path) {
    RecentlyOpenedPaths.remove(path);
    RecentlyOpenedPaths.emplace_front(path);
    Write();
}
