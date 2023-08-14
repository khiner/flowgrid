#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AudioGraph,
    DefineUnmergableAction(CreateNode, std::string node_type_id;);
    DefineUnmergableAction(CreateFaustNode, ID dsp_id;);
    DefineUnmergableAction(DeleteNode, ID id;);
    DefineAction(SetDeviceDataFormat, Merge, "", ID id; int sample_format; u32 channels; u32 sample_rate;);

    Json(CreateNode, node_type_id);
    Json(CreateFaustNode, dsp_id);
    Json(DeleteNode, id);
    Json(SetDeviceDataFormat, id, sample_format, channels, sample_rate);

    using Any = ActionVariant<CreateNode, CreateFaustNode, DeleteNode, SetDeviceDataFormat>;
);
