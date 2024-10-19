#pragma once

enum FontStyle_ {
    FontStyle_Regular = 0,
    FontStyle_Bold = 1 << 1,
    FontStyle_Italic = 1 << 2,
};
using FontStyle = int;

enum class FontFamily {
    Main,
    Monospace,
};

struct ImFont;

struct Fonts {
    static constexpr float AtlasScale = 2; // We rasterize to a scaled-up texture and scale down the font size globally, for sharper text.

    inline static ImFont *Main{nullptr}, *MainBold{nullptr}, *MainItalic{nullptr}, *MainBoldItalic{nullptr};
    inline static ImFont *Monospace{nullptr}, *MonospaceBold{nullptr}, *MonospaceItalic{nullptr}, *MonospaceBoldItalic{nullptr};

    static void Init(); // Must be called after ImGui context is created.

    static ImFont *Get(FontFamily family, FontStyle style = FontStyle_Regular) {
        if (family == FontFamily::Main) {
            if (style == FontStyle_Regular) return Main;
            if (style == FontStyle_Bold) return MainBold;
            if (style == FontStyle_Italic) return MainItalic;
            if (style == (FontStyle_Bold | FontStyle_Italic)) return MainBoldItalic;
        }
        if (family == FontFamily::Monospace) {
            if (style == FontStyle_Regular) return Monospace;
            if (style == FontStyle_Bold) return MonospaceBold;
            if (style == FontStyle_Italic) return MonospaceItalic;
            if (style == (FontStyle_Bold | FontStyle_Italic)) return MonospaceBoldItalic;
        }
        return Main;
    }

    // Returns true if the font was changed.
    // **Only call `PopFont` if `PushFont` returned true.**
    static bool Push(FontFamily family, FontStyle style = FontStyle_Regular);
    static void Pop();
};
