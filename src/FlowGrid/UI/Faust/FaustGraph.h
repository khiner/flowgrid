#include <string_view>

class CTree;
typedef CTree *Box;
void OnBoxChange(Box);
void SaveBoxSvg(std::string_view path);
