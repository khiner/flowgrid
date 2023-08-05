#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AudioGraph,
    DefineAction(CreateNode, NoMerge, "", std::string node_type_id;);
    DefineAction(DeleteNode, NoMerge, "", ID id;);
    DefineAction(SetDeviceDataFormat, Merge, "", ID id; int sample_format; u32 channels; u32 sample_rate;);

    Json(CreateNode, node_type_id);
    Json(DeleteNode, id);
    Json(SetDeviceDataFormat, id, sample_format, channels, sample_rate);

    using Any = ActionVariant<CreateNode, DeleteNode, SetDeviceDataFormat>;
);
