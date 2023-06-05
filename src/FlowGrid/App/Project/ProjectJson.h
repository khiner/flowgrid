#pragma once

#include "nlohmann/json.hpp"

#include "ProjectJsonFormat.h"

nlohmann::json GetProjectJson(const ProjectJsonFormat);
