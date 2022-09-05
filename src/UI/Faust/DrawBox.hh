#pragma once

#include <string>

using std::string;

class CTree;
typedef CTree *Box;

void on_box_change(Box box);
void save_box_svg(const string &path);
