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
    Visit(
        action,
        [&field](const Bool::ActionType &a) {
            if (const auto *bool_field = dynamic_cast<const Bool *>(field)) {
                bool_field->Apply(a);
            }
        },
        [&field](const Int::ActionType &a) {
            if (const auto *int_field = dynamic_cast<const Int *>(field)) {
                int_field->Apply(a);
            }
        },
        [&field](const UInt::ActionType &a) {
            if (const auto *uint_field = dynamic_cast<const UInt *>(field)) {
                uint_field->Apply(a);
            }
        },
        [&field](const Float::ActionType &a) {
            if (const auto *float_field = dynamic_cast<const Float *>(field)) {
                float_field->Apply(a);
            }
        },
        [&field](const String::ActionType &a) {
            if (const auto *string_field = dynamic_cast<const String *>(field)) {
                string_field->Apply(a);
            }
        },
        [&field](const Enum::ActionType &a) {
            if (const auto *enum_field = dynamic_cast<const Enum *>(field)) {
                enum_field->Apply(a);
            }
        },
        [&field](const Flags::ActionType &a) {
            if (const auto *flags_field = dynamic_cast<const Flags *>(field)) {
                flags_field->Apply(a);
            }
        },
        [&field](const MultilineString::ActionType &a) {
            if (const auto *multiline_string_field = dynamic_cast<const MultilineString *>(field)) {
                multiline_string_field->Apply(a);
            }
        },
        [&field](const Vec2::ActionType &a) {
            if (const auto *vec2_field = dynamic_cast<const Vec2 *>(field)) {
                vec2_field->Apply(a);
            }
        },
        [](const VectorBase::ActionHandler::ActionType &a) { VectorBase::ActionHandler.Apply(a); },
        [](const MatrixBase::ActionHandler::ActionType &a) { MatrixBase::ActionHandler.Apply(a); },
    );
}
