#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AudioGraph,
    DefineAction(CreateNode, Saved, NoMerge, "", std::string node_type_id;);
    DefineAction(CreateFaustNode, Saved, NoMerge, "", ID dsp_id;);
    DefineAction(DeleteNode, Saved, NoMerge, "", ID id;);
    DefineAction(SetDeviceDataFormat, Saved, Merge, "", ID id; int sample_format; u32 channels; u32 sample_rate;);

    Json(CreateNode, node_type_id);
    Json(CreateFaustNode, dsp_id);
    Json(DeleteNode, id);
    Json(SetDeviceDataFormat, id, sample_format, channels, sample_rate);

    using Any = ActionVariant<CreateNode, CreateFaustNode, DeleteNode, SetDeviceDataFormat>;
);
