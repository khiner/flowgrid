#include "FieldActionHandler.h"

#include "Core/Container/Matrix.h"
#include "Core/Container/Vec2.h"
#include "Core/Container/Vector.h"

void FieldActionHandler::Apply(const ActionType &action) const {
    const auto *field = Field::FindByPath(action.GetFieldPath());
    Visit(
        action,
        [&field](const PrimitiveField::ActionType &a) {
            if (auto *primitive_field = dynamic_cast<const PrimitiveField *>(field)) {
                primitive_field->Apply(a);
            }
        },
        [&field](const Vec2::ActionType &a) {
            if (auto *vec2_field = dynamic_cast<const Vec2 *>(field)) {
                vec2_field->Apply(a);
            }
        },
        [](const VectorBase::ActionHandler::ActionType &a) { VectorBase::ActionHandler.Apply(a); },
        [](const MatrixBase::ActionHandler::ActionType &a) { MatrixBase::ActionHandler.Apply(a); },
    );
}
