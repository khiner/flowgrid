#pragma once

using u32 = unsigned int;

struct LineChar {
    u32 L{0}, C{0};

    auto operator<=>(const LineChar &o) const {
        if (auto cmp = L <=> o.L; cmp != 0) return cmp;
        return C <=> o.C;
    }
    bool operator==(const LineChar &) const = default;
    bool operator!=(const LineChar &) const = default;
};

struct LineCharRange {
    // `Start` and `End` are the the first and second coordinate _set in an interaction_.
    // Use `Min()` and `Max()` for positional ordering.
    LineChar Start, End{Start};

    LineCharRange() = default;
    LineCharRange(LineChar start, LineChar end) : Start(std::move(start)), End(std::move(end)) {}
    LineCharRange(LineChar lc) : Start(lc), End(lc) {}

    bool operator==(const LineCharRange &) const = default;
    bool operator!=(const LineCharRange &) const = default;
    auto operator<=>(const LineCharRange &o) const { return Min() <=> o.Min(); }

    LineChar Min() const { return std::min(Start, End); }
    LineChar Max() const { return std::max(Start, End); }

    LineCharRange To(LineChar lc, bool extend = false) const { return {extend ? Start : lc, lc}; }

    u32 Line() const { return End.L; }
    u32 CharIndex() const { return End.C; }
    LineChar LC() const { return End; } // Be careful if this is a multiline cursor!

    bool IsRange() const { return Start != End; }
    bool IsMultiline() const { return Start.L != End.L; }
    bool IsRightOf(LineChar lc) const { return End.L == lc.L && End.C > lc.C; }
};
