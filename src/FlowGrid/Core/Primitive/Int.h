#pragma once

#include "Primitive.h"

struct Int : Primitive<int> {
    Int(ComponentArgs &&, int value = 0, int min = 0, int max = 100);

    operator bool() const { return Value != 0; }
    operator char() const { return Value; };
    operator s8() const { return Value; };
    operator s16() const { return Value; };

    void Render(const std::vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};
