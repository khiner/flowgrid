#include "Actionable.h"

#include "../Helper/String.h"

namespace Actionable {
Metadata::Metadata(string_view name)
    : Name(StringHelper::PascalToSentenceCase(name)) {}
} // namespace Actionable
