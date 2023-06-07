#pragma once

#include "Field.h"

namespace Stateful::Field {
struct Bool : TypedBase<bool>, MenuItemDrawable {
    using TypedBase::TypedBase;

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

private:
    void Render() const override;
    void Toggle() const; // Used in draw methods.
};
} // namespace Stateful::Field
