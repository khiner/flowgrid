#include <string_view>

class CTree;
typedef CTree *Box;
typedef CTree *Tree;

void OnBoxChange(Box);
void SaveBoxSvg(const fs::path &dir_path);

bool IsBoxHovered(unsigned int imgui_id);
std::string GetBoxInfo(unsigned int imgui_id);
