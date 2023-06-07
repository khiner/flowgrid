#pragma once

#include "Field.h"

namespace Stateful::Field {
struct String : TypedBase<string> {
    String(Stateful::Base *parent, string_view path_segment, string_view name_help, string_view value = "");

    operator bool() const;
    operator string_view() const;

    void Render(const std::vector<string> &options) const;

private:
    void Render() const override;
};
} // namespace Stateful::Field
