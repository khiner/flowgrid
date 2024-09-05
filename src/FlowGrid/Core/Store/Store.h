#pragma once

#include "TypedStore.h"

#include "IdPairs.h"
#include "Project/TextEditor/TextBufferData.h"

struct Store : TypedStore<
                   bool, u32, s32, float, std::string, IdPairs, TextBufferData, immer::set<u32>,
                   immer::flex_vector<bool>, immer::flex_vector<s32>, immer::flex_vector<u32>, immer::flex_vector<float>, immer::flex_vector<std::string>> {};
