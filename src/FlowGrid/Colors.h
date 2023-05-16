#pragma once

#include "Store/StoreFwd.h"
#include "WindowMember.h"

struct ImVec4;

struct Colors : UIStateMember {
    Colors(StateMember *parent, string_view path_segment, string_view name_help, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto = false);
    ~Colors();

    static U32 ConvertFloat4ToU32(const ImVec4 &value);
    static ImVec4 ConvertU32ToFloat4(const U32 value);

    Count Size() const;
    U32 operator[](Count) const;

    void Set(const std::vector<ImVec4> &, TransientStore &) const;
    void Set(const std::vector<std::pair<int, ImVec4>> &, TransientStore &) const;

protected:
    void Render() const override;

private:
    inline const UInt *At(Count) const;

    bool AllowAuto;
};
