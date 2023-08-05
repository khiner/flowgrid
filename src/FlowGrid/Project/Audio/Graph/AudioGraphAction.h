#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AudioGraph,
    DefineAction(DeleteNode, NoMerge, "", ID id;);
    DefineAction(SetDeviceDataFormat, Merge, "", ID id; int sample_format; u32 channels; u32 sample_rate;);

    Json(DeleteNode, id);
    Json(SetDeviceDataFormat, id, sample_format, channels, sample_rate);

    using Any = ActionVariant<DeleteNode, SetDeviceDataFormat>;
);
