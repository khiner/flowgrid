#include <string_view>

class CTree;
typedef CTree *Box;
typedef CTree *Tree;

void OnBoxChange(Box);
void SaveBoxSvg(std::string_view path);

Box GetHoveredBox(unsigned int imgui_id);
string GetTreeInfo(Tree);
