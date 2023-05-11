#include "StoreJson.h"

#include "Store.h"
#include "StoreHistory.h"

#include "../Action/ActionJson.h"
#include "../PrimitiveJson.h"

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

namespace nlohmann {
inline void to_json(json &j, const Store &store) {
    for (const auto &[key, value] : store) {
        j[json::json_pointer(key.string())] = value;
    }
}
} // namespace nlohmann

using namespace nlohmann;

// Not using `nlohmann::from_json` pattern to avoid getting a reference to a default-constructed, non-transient `Store` instance.
Store JsonToStore(const json &j) {
    const auto &flattened = j.flatten();
    StoreEntries entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value] : flattened.items()) entries[item_index++] = {StorePath(key), Primitive(value)};

    TransientStore store;
    for (const auto &[path, value] : entries) store.set(path, value);
    return store.persistent();
}

GesturesProject JsonToGestures(const nlohmann::json &j) {
    return {j["gestures"], j["index"]};
}

json GetStoreJson(const StoreJsonFormat format) {
    switch (format) {
        case StateFormat: return AppStore;
        case ActionFormat: return {{"gestures", History.Gestures()}, {"index", History.Index}};
    }
}
