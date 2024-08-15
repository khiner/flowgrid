#include "TextBufferData.h"

#include <ranges>

#include "immer/flex_vector_transient.hpp"

using std::ranges::any_of, std::ranges::all_of, std::ranges::fold_left, std::ranges::subrange, std::views::filter, std::views::transform, std::ranges::to;

using TransientLine = immer::flex_vector_transient<char>;
using TransientLines = immer::flex_vector_transient<TextBufferLine>;

u32 TextBufferData::ToByteIndex(LineChar lc) const {
    if (lc.L >= Text.size()) return EndByteIndex();
    return fold_left(subrange(Text.begin(), Text.begin() + lc.L), 0u, [](u32 sum, const auto &line) { return sum + line.size() + 1; }) + lc.C;
}

std::string TextBufferData::GetText(LineChar start, LineChar end) const {
    if (end <= start) return "";

    const u32 start_li = start.L, end_li = std::min(u32(Text.size()) - 1, end.L);
    const u32 start_ci = start.C, end_ci = end.C;
    std::string result;
    for (u32 ci = start_ci, li = start_li; li < end_li || ci < end_ci;) {
        if (const auto &line = Text[li]; ci < line.size()) {
            result += line[ci++];
        } else {
            ++li;
            ci = 0;
            result += '\n';
        }
    }

    return result;
}

bool TextBufferData::AnyCursorsRanged() const {
    return any_of(Cursors, [](const auto &c) { return c.IsRange(); });
}
bool TextBufferData::AllCursorsRanged() const {
    return all_of(Cursors, [](const auto &c) { return c.IsRange(); });
}
bool TextBufferData::AnyCursorsMultiline() const {
    return any_of(Cursors, [](const auto &c) { return c.IsMultiline(); });
}

TextBufferData TextBufferData::SetText(const std::string &text) const {
    const u32 old_end_byte = EndByteIndex();

    TransientLines transient_lines{};
    TransientLine current_line{};
    for (auto ch : text) {
        if (ch == '\r') continue; // Ignore the carriage return character.
        if (ch == '\n') {
            transient_lines.push_back(current_line.persistent());
            current_line = {};
        } else {
            current_line.push_back(ch);
        }
    }
    transient_lines.push_back(current_line.persistent());

    TextBufferData b = *this;
    b.Text = transient_lines.persistent();
    b.Edits = b.Edits.push_back({0, old_end_byte, b.EndByteIndex()});
    return b;
}

std::string join_with(auto &&v, std::string_view delimiter = "") {
    std::ostringstream os;
    for (auto it = std::ranges::begin(v); it != std::ranges::end(v); ++it) {
        if (it != v.begin()) os << delimiter;
        os << *it;
    }
    return os.str();
}

std::string TextBufferData::GetSelectedText() const {
    return AnyCursorsRanged() ?
        join_with(Cursors | filter([](const auto &c) { return c.IsRange(); }) | transform([this](const auto &c) { return GetText(c); }), "\n") :
        Text[GetCursorPosition().L] | to<std::string>();
}
