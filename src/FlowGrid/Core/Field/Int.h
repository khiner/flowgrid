#pragma once

#include "Field.h"

struct Int : TypedField<int> {
    Int(Stateful *parent, string_view path_leaf, string_view meta_str, int value = 0, int min = 0, int max = 100);

    operator bool() const;
    operator short() const;
    operator char() const;
    operator S8() const;

    void Render(const std::vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};
