#include "IdPair.h"

#include <format>
#include <sstream>

IdPair DeserializeIdPair(const std::string &serialized_id_pair) {
    IdPair id_pair;
    std::stringstream ss(serialized_id_pair);
    char comma;
    if (!(ss >> id_pair.first >> comma >> id_pair.second)) throw std::invalid_argument("Invalid string format for ID pair.");

    return id_pair;
}

std::string SerializeIdPair(const IdPair &p) {
    return std::format("{},{}", p.first, p.second);
}
