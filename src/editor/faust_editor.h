#pragma once

#include "zep.h"

void zep_init(const Zep::NVec2f &pixelScale);
void zep_show();
void zep_destroy();
void zep_load(const Zep::ZepPath &file);
