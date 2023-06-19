#pragma once

#include "Core/Field/Field.h"

struct Int : TypedField<int> {
    Int(ComponentArgs &&, int value = 0, int min = 0, int max = 100);

    operator bool() const;
    operator short() const;
    operator char() const;
    operator S8() const;

    void Render(const std::vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};
