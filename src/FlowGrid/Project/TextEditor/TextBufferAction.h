#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    TextBuffer,
    DefineUnsavedAction(Undo, NoMerge, "@cmd+z");
    DefineUnsavedAction(Redo, NoMerge, "@shift+cmd+z");

    using Any = ActionVariant<Undo, Redo>;
);
