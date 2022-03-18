#pragma once

#include "blockingconcurrentqueue.h"
#include "action.h"

using namespace moodycamel;

int draw(BlockingConcurrentQueue<Action> &);
