#pragma once

#include "FileDialogData.h"

#include "nlohmann/json.hpp"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileDialogData, title, filters, file_path, default_file_name, save_mode, max_num_selections, flags);
