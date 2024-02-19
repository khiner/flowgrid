#include "ID.h"

#include <string>

#include "imgui_internal.h"

using std::string, std::string_view;

ID GenerateId(ID parent_id, ID child_id) { return ImHashData(&child_id, sizeof(child_id), parent_id); }
ID GenerateId(ID parent_id, string_view child_id) { return ImHashStr(string(child_id).c_str(), 0, parent_id); }
