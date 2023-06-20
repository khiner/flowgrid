#include "FieldActionHandler.h"

#include "Core/Container/Matrix.h"
#include "Core/Container/MultilineString.h"
#include "Core/Container/Vec2.h"
#include "Core/Container/Vector.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Flags.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/Int.h"
#include "Core/Primitive/String.h"
#include "Core/Primitive/UInt.h"

void FieldActionHandler::Apply(const ActionType &action) const {
    const auto *field = Field::FindByPath(action.GetFieldPath());
    if (field == nullptr) throw std::runtime_error(std::format("Field not found: {}", action.GetFieldPath().string()));

    // Note: If/when we support arbitrary json actions, we'll need to check field types.
    //   Maybe with a separate `FindByPath` for each type?
    //   Could also have each field primitive field type accept an `Action::Field::Any`,
    //   and do the best it can to convert it to something meaningful (e.g. convert string set to an int set).
    Visit(
        action,
        [&field](const Bool::ActionType &a) { static_cast<const Bool *>(field)->Apply(a); },
        [&field](const Int::ActionType &a) { static_cast<const Int *>(field)->Apply(a); },
        [&field](const UInt::ActionType &a) { static_cast<const UInt *>(field)->Apply(a); },
        [&field](const Float::ActionType &a) { static_cast<const Float *>(field)->Apply(a); },
        [&field](const String::ActionType &a) { static_cast<const String *>(field)->Apply(a); },
        [&field](const Enum::ActionType &a) { static_cast<const Enum *>(field)->Apply(a); },
        [&field](const Flags::ActionType &a) { static_cast<const Flags *>(field)->Apply(a); },
        [&field](const MultilineString::ActionType &a) { static_cast<const MultilineString *>(field)->Apply(a); },
        [&field](const Vec2::ActionType &a) { static_cast<const Vec2 *>(field)->Apply(a); },
        [&field](const Vector<bool>::ActionType &a) { static_cast<const Vector<bool> *>(field)->Apply(a); },
        [&field](const Vector<int>::ActionType &a) { static_cast<const Vector<int> *>(field)->Apply(a); },
        [&field](const Vector<U32>::ActionType &a) { static_cast<const Vector<U32> *>(field)->Apply(a); },
        [&field](const Vector<float>::ActionType &a) { static_cast<const Vector<float> *>(field)->Apply(a); },
        [](const MatrixBase::ActionHandler::ActionType &a) { MatrixBase::ActionHandler.Apply(a); },
    );
}
