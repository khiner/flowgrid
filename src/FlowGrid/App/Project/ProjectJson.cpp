#include "ProjectJson.h"
#include "Core/Store/StoreHistory.h"

#include "immer/map.hpp"

nlohmann::json GetProjectJson(const ProjectJsonFormat format) {
    switch (format) {
        case StateFormat: return store::Get();
        case ActionFormat: return History.GetIndexedGestures();
    }
}
