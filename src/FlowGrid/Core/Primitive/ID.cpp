#include "ID.h"

#include <string>

#include "imgui_internal.h"

ID GenerateId(ID parent_id, ID child_id) { return ImHashData(&child_id, sizeof(child_id), parent_id); }
ID GenerateId(ID parent_id, std::string_view child_id) { return ImHashStr(child_id.data(), 0, parent_id); }
