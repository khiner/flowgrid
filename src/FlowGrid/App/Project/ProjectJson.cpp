#include "ProjectJson.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"

nlohmann::json GetProjectJson(const ProjectJsonFormat format) {
    switch (format) {
        case StateFormat: return store::GetJson();
        case ActionFormat: return History.GetIndexedGestures();
    }
}
