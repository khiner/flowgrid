#include "IDs.h"

std::unordered_map<StorePath, ID, PathHash> IDs::ByPath{};
