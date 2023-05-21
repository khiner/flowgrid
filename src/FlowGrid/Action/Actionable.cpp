#include "Actionable.h"

#include "../Helper/String.h"

namespace Action {
Meta::Meta(string_view name)
    : Name(StringHelper::PascalToSentenceCase(name)) {}
} // namespace Action
