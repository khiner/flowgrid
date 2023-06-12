#pragma once

#include "Field.h"

struct String : TypedField<string> {
    String(Stateful *parent, string_view path_leaf, string_view meta_str, string_view value = "");

    operator bool() const;
    operator string_view() const;

    void Render(const std::vector<string> &options) const;

private:
    void Render() const override;
};
