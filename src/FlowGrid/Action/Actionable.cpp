#include "Actionable.h"

#include "../Helper/String.h"

namespace Actionable {
Metadata::Metadata(string_view name, string_view menu_label)
    : Name(StringHelper::PascalToSentenceCase(name)), MenuLabel(menu_label.empty() ? Name : menu_label) {}
} // namespace Actionable
