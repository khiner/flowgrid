#pragma once

#include "Core/Field/UInt.h"
#include "Core/Window.h"

struct ImVec4;

struct Colors : UIComponent {
    Colors(ComponentArgs &&, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto = false);
    ~Colors();

    static U32 ConvertFloat4ToU32(const ImVec4 &value);
    static ImVec4 ConvertU32ToFloat4(const U32 value);

    Count Size() const;
    U32 operator[](Count) const;

    void Set(const std::vector<ImVec4> &) const;
    void Set(const std::vector<std::pair<int, ImVec4>> &) const;

protected:
    void Render() const override;

private:
    inline const UInt *At(Count) const;

    bool AllowAuto;
};
