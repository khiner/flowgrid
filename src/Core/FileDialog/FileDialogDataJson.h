#pragma once

#include "FileDialogData.h"

#include "nlohmann/json.hpp"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileDialogData, OwnerId, Title, Filters, FilePath, DefaultFileName, SaveMode, MaxNumSelections, Flags);
