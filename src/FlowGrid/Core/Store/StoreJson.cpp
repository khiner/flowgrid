#include "StoreJson.h"
#include "Store.h"
#include "StoreHistory.h"

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

namespace nlohmann {
void to_json(json &j, const Store &store) {
    for (const auto &[key, value] : store) {
        j[json::json_pointer(key.string())] = value;
    }
}
} // namespace nlohmann

Store JsonToStore(const nlohmann::json &j) {
    const auto &flattened = j.flatten();
    StoreEntries entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value] : flattened.items()) entries[item_index++] = {StorePath(key), Primitive(value)};

    TransientStore store;
    for (const auto &[path, value] : entries) store.set(path, value);
    return store.persistent();
}

nlohmann::json GetStoreJson(const StoreJsonFormat format) {
    switch (format) {
        case StateFormat: return store::Get();
        case ActionFormat: return History.GetIndexedGestures();
    }
}
