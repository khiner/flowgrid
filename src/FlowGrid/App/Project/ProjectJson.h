#pragma once

#include "nlohmann/json.hpp"

#include "ProjectFormat.h"

nlohmann::json GetProjectJson(const ProjectFormat);
