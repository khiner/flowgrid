#pragma once

#include "Core/Store/Store.h"
#include "ProjectJsonFormat.h"

nlohmann::json GetProjectJson(const ProjectJsonFormat);
