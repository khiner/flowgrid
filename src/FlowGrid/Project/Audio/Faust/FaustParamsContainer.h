#pragma once

#include "Project/Audio/Sample.h" // Must be included before any Faust includes

#include "FaustParamType.h"
#include "UI/NamesAndValues.h"

struct FaustParamsContainer {
    virtual void Add(FaustParamType, const char *label, std::string_view short_label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {}) = 0;
    virtual void PopGroup() = 0;
};
