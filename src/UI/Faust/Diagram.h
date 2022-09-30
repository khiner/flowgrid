#pragma once

#include <string>

using std::string;

class FaustUI;
class CTree;
typedef CTree *Box;

void on_box_change(Box);
void save_box_svg(const string &path);
