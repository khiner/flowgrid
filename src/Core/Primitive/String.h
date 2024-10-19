#pragma once

#include "Primitive.h"

struct String : Primitive<std::string> {
    String(ComponentArgs &&, std::string_view value = "");
    String(ComponentArgs &&, fs::path value);

    operator bool() const { return !Value.empty(); }
    operator std::string_view() const { return Value; }
    operator fs::path() const { return Value; }

    void Render(const std::vector<std::string> &options) const;

private:
    void Render() const override;
};
