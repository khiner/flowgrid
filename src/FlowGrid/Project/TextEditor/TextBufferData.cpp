#include "TextBufferData.h"

#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "immer/flex_vector_transient.hpp"

using std::ranges::any_of, std::ranges::all_of, std::ranges::find_if, std::ranges::fold_left, std::ranges::subrange, std::views::filter, std::views::transform, std::ranges::reverse_view, std::ranges::to;

using TransientLine = immer::flex_vector_transient<char>;
using TransientLines = immer::flex_vector_transient<TextBufferLine>;

using Cursor = LineCharRange;
using Lines = TextBufferLines;

constexpr bool IsWordChar(char ch) {
    return UTF8CharLength(ch) > 1 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}
constexpr bool IsUTFSequence(char c) { return (c & 0xC0) == 0x80; }

constexpr char ToLower(char ch, bool case_sensitive) { return (!case_sensitive && ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch; }

constexpr bool Equals(const auto &c1, const auto &c2, std::size_t c2_offset = 0) {
    if (c2.size() + c2_offset < c1.size()) return false;

    const auto begin = c2.begin() + c2_offset;
    return std::ranges::equal(c1, subrange(begin, begin + c1.size()));
}

// Depends on ImGui definition.
const char *ImTextCharToUtf8(char out_buf[5], unsigned int c);

struct LinesIter {
    LinesIter(const Lines &lines, LineChar lc, LineChar begin, LineChar end)
        : Text(lines), LC(std::move(lc)), Begin(std::move(begin)), End(std::move(end)) {}
    LinesIter(const LinesIter &) = default; // Needed since we have an assignment operator.

    LinesIter &operator=(const LinesIter &o) {
        LC = o.LC;
        Begin = o.Begin;
        End = o.End;
        return *this;
    }

    operator char() const {
        const auto &line = Text[LC.L];
        return LC.C < line.size() ? line[LC.C] : '\0';
    }

    LineChar operator*() const { return LC; }

    LinesIter &operator++() {
        MoveRight();
        return *this;
    }
    LinesIter &operator--() {
        MoveLeft();
        return *this;
    }

    bool operator==(const LinesIter &o) const { return LC == o.LC; }
    bool operator!=(const LinesIter &o) const { return LC != o.LC; }

    bool IsBegin() const { return LC == Begin; }
    bool IsEnd() const { return LC == End; }
    void Reset() { LC = Begin; }

private:
    const Lines &Text;
    LineChar LC, Begin, End;

    void MoveRight() {
        if (LC == End) return;

        if (LC.C == Text[LC.L].size()) {
            ++LC.L;
            LC.C = 0;
        } else {
            LC.C = std::min(LC.C + UTF8CharLength(Text[LC.L][LC.C]), u32(Text[LC.L].size()));
        }
    }

    void MoveLeft() {
        if (LC == Begin) return;

        if (LC.C == 0) {
            --LC.L;
            LC.C = Text[LC.L].size();
        } else {
            do { --LC.C; } while (LC.C > 0 && IsUTFSequence(Text[LC.L][LC.C]));
        }
    }
};

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

LineChar TextBufferData::FindWordBoundary(LineChar from, bool is_start) const {
    if (from.L >= Text.size()) return from;

    const auto &line = Text[from.L];
    u32 ci = from.C;
    if (ci >= line.size()) return from;

    const char init_char = line[ci];
    const bool init_is_word_char = IsWordChar(init_char), init_is_space = isspace(init_char);
    for (; is_start ? ci > 0 : ci < line.size(); is_start ? --ci : ++ci) {
        if (ci == line.size() ||
            (init_is_space && !isspace(line[ci])) ||
            (init_is_word_char && !IsWordChar(line[ci])) ||
            (!init_is_word_char && !init_is_space && init_char != line[ci])) {
            if (is_start) ++ci; // Undo one left step before returning line/char.
            break;
        }
    }
    return {from.L, ci};
}

// Returns a cursor containing the start/end positions of the next occurrence of `text` at or after `start`, or `std::nullopt` if not found.
std::optional<Cursor> TextBufferData::FindNextOccurrence(std::string_view text, LineChar start, bool case_sensitive) const {
    if (text.empty()) return {};
    auto find_lci = LinesIter{Text, start, BeginLC(), EndLC()};
    do {
        auto match_lci = find_lci;
        for (u32 i = 0; i < text.size(); ++i) {
            const auto &match_lc = *match_lci;
            if (match_lc.C == Text[match_lc.L].size()) {
                if (text[i] != '\n' || match_lc.L + 1 >= Text.size()) break;
            } else if (ToLower(match_lci, case_sensitive) != ToLower(text[i], case_sensitive)) {
                break;
            }

            ++match_lci;
            if (i == text.size() - 1) return Cursor{*find_lci, *match_lci};
        }

        ++find_lci;
        if (find_lci.IsEnd()) find_lci.Reset();
    } while (*find_lci != start);

    return {};
}

std::optional<Cursor> TextBufferData::FindMatchingBrackets(const Cursor &c) const {
    static const std::unordered_map<char, char>
        OpenToCloseChar{{'{', '}'}, {'(', ')'}, {'[', ']'}},
        CloseToOpenChar{{'}', '{'}, {')', '('}, {']', '['}};

    const u32 li = c.Line();
    const auto &line = Text[li];
    if (c.IsRange() || line.empty()) return {};

    u32 ci = c.CharIndex();
    // Considered on bracket if cursor is to the left or right of it.
    if (ci > 0 && (CloseToOpenChar.contains(line[ci - 1]) || OpenToCloseChar.contains(line[ci - 1]))) --ci;

    const char ch = line[ci];
    const bool is_close_char = CloseToOpenChar.contains(ch), is_open_char = OpenToCloseChar.contains(ch);
    if (!is_close_char && !is_open_char) return {};

    const LineChar lc{li, ci};
    const char other_ch = is_close_char ? CloseToOpenChar.at(ch) : OpenToCloseChar.at(ch);
    u32 match_count = 0;
    for (auto lci = LinesIter{Text, lc, BeginLC(), EndLC()};
         is_close_char ? !lci.IsBegin() : !lci.IsEnd(); is_close_char ? --lci : ++lci) {
        const char ch_inner = lci;
        if (ch_inner == ch) ++match_count;
        else if (ch_inner == other_ch && (match_count == 0 || --match_count == 0)) return Cursor{lc, *lci};
    }

    return {};
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

TextBufferData TextBufferData::MergeCursors() const {
    AssertCursorsSorted();

    // ColumnsForCursorIndex.clear();
    if (Cursors.size() <= 1) return *this;

    TextBufferData b = *this;
    const auto last_added_cursor_lc = b.LastAddedCursor().LC();

    // Merge overlapping cursors.
    immer::vector_transient<Cursor> merged;
    auto current = b.Cursors.front();
    for (size_t c = 1; c < b.Cursors.size(); ++c) {
        const auto &next = b.Cursors[c];
        if (current.Max() >= next.Min()) {
            // Overlap. Extend the current cursor to to include the next.
            current = {std::min(current.Min(), next.Min()), std::max(current.Max(), next.Max())};
        } else {
            // No overlap. Finalize the current cursor and start a new merge.
            merged.push_back(current);
            current = next;
        }
    }
    merged.push_back(current);
    b.Cursors = merged.persistent();

    // Update last added cursor index to be valid after sort/merge.
    const auto it = find_if(b.Cursors, [&last_added_cursor_lc](const auto &c) { return c.LC() == last_added_cursor_lc; });
    b.LastAddedCursorIndex = it != b.Cursors.end() ? std::distance(b.Cursors.begin(), it) : 0;

    return b;
}

TextBufferData TextBufferData::SetCursor(Cursor c, bool add) const {
    auto b = *this;
    if (!add) b.Cursors = {};
    // Insert into sorted position.
    b.LastAddedCursorIndex = std::distance(b.Cursors.begin(), std::ranges::upper_bound(b.Cursors, c));
    b.Cursors = b.LastAddedCursorIndex < b.Cursors.size() ? b.Cursors.set(b.LastAddedCursorIndex, std::move(c)) : b.Cursors.push_back(std::move(c));
    return b.MergeCursors();
}
TextBufferData TextBufferData::SetCursors(const immer::vector<Cursor> &cursors) const {
    auto b = *this;
    b.Cursors = cursors;
    b = b.MergeCursors();
    return b;
}

TextBufferData TextBufferData::EditCursor(u32 i, LineChar lc, bool select) const {
    auto b = *this;
    b.Cursors = b.Cursors.set(i, b.Cursors[i].To(lc, select));
    return b.MergeCursors();
}

TextBufferData TextBufferData::MoveCursorsLines(int amount, bool select, bool move_start, bool move_end) const {
    if (!move_start && !move_end) return *this;

    immer::vector_transient<Cursor> new_cursors;
    for (u32 i = 0; i < Cursors.size(); ++i) {
        const auto &c = Cursors[i];
        // Track the cursor's column to return back to it after moving to a line long enough.
        // (This is the only place we worry about this.)
        const auto [new_start_column, new_end_column] = GetColumns(c, i);
        const u32 new_end_li = std::clamp(int(c.End.L) + amount, 0, int(Text.size() - 1));
        const LineChar new_end{
            new_end_li,
            std::min(GetCharIndex({new_end_li, new_end_column}), GetLineMaxCharIndex(new_end_li)),
        };
        if (!select || !move_start) {
            new_cursors.push_back(c.To(new_end, select));
            continue;
        }

        const u32 new_start_li = std::clamp(int(c.Start.L) + amount, 0, int(Text.size() - 1));
        const LineChar new_start{
            new_start_li,
            std::min(GetCharIndex({new_start_li, new_start_column}), GetLineMaxCharIndex(new_start_li)),
        };
        new_cursors.push_back({new_start, new_end});
    }

    auto b = *this;
    b.Cursors = new_cursors.persistent();
    b.AssertCursorsSorted();
    return b;
}

TextBufferData TextBufferData::MoveCursorsChar(bool right, bool select, bool is_word_mode) const {
    const bool any_selections = AnyCursorsRanged();
    return EditCursors([this, right, select, is_word_mode, any_selections](const auto &c) {
        if (any_selections && !select && !is_word_mode) return c.To(right ? c.Max() : c.Min());
        if (auto lci = LinesIter{Text, c.LC(), BeginLC(), EndLC()};
            (!right && !lci.IsBegin()) || (right && !lci.IsEnd())) {
            if (right) ++lci;
            else --lci;
            return c.To(is_word_mode ? FindWordBoundary(*lci, !right) : *lci, select);
        }
        return c;
    });
}

TextBufferData TextBufferData::SwapLines(u32 li1, u32 li2) const {
    if (li1 == li2 || li1 >= Text.size() || li2 >= Text.size()) return *this;

    auto [b, _] = InsertText({Text[li2], {}}, {li1, 0}, false);
    // If the second line is the last line, we also need to delete the newline we just inserted.
    auto range = li2 + 1 < b.Text.size() ? LineCharRange{{li2 + 1, 0}, {li2 + 2, 0}} : LineCharRange{{li2, u32(b.Text[li2].size())}, b.EndLC()};
    return b.DeleteRange(range, false);
}

// Returns insertion end.
std::pair<TextBufferData, LineChar> TextBufferData::InsertText(Lines text, LineChar at, bool update_cursors) const {
    if (text.empty()) return {*this, at};

    auto t = Text;
    if (at.L < t.size()) {
        auto ln1 = t[at.L];
        t = t.set(at.L, ln1.take(at.C) + text[0]);
        t = t.take(at.L + 1) + text.drop(1) + t.drop(at.L + 1);
        auto ln2 = t[at.L + text.size() - 1];
        t = t.set(at.L + text.size() - 1, ln2 + ln1.drop(at.C));
    } else {
        t = t + text;
    }

    auto b = *this;
    b.Text = t;
    const u32 num_new_lines = text.size() - 1;
    if (update_cursors) {
        b = b.EditCursors(
            [num_new_lines](const auto &c) { return c.To({c.Line() + num_new_lines, c.CharIndex()}); },
            [at](const auto &c) { return c.Line() > at.L; }
        );
    }

    const u32 start_byte = b.ToByteIndex(at);
    const u32 text_byte_length = std::accumulate(text.begin(), text.end(), 0, [](u32 sum, const auto &line) { return sum + line.size(); }) + text.size() - 1;
    b.Edits = b.Edits.push_back({start_byte, start_byte, start_byte + text_byte_length});

    return {b, {at.L + num_new_lines, u32(text.size() == 1 ? at.C + text.front().size() : text.back().size())}};
}

TextBufferData TextBufferData::DeleteRange(LineCharRange lcr, bool update_cursors, std::optional<Cursor> exclude_cursor) const {
    const auto start = lcr.Min(), end = lcr.Max();
    if (end <= start) return *this;

    auto start_line = Text[start.L], end_line = Text[end.L];
    const u32 start_byte = ToByteIndex(start), old_end_byte = ToByteIndex(end);

    auto b = *this;
    if (start.L == end.L) {
        b.Text = b.Text.set(start.L, start_line.erase(start.C, end.C));

        if (update_cursors) {
            b = b.EditCursors(
                [start, end](const auto &c) { return c.To({c.Line(), u32(c.CharIndex() - (end.C - start.C))}); },
                [start](const auto &c) { return !c.IsRange() && c.IsRightOf(start); }
            );
        }
    } else {
        end_line = end_line.drop(end.C);
        b.Text = b.Text.set(end.L, end_line)
                     .set(start.L, start_line.take(start.C) + end_line)
                     .erase(start.L + 1, end.L + 1);

        if (update_cursors) {
            b = b.EditCursors(
                [start, end](const auto &c) { return c.To({c.Line() - (end.L - start.L), c.CharIndex()}); },
                [end, exclude_cursor](const auto &c) { return (!exclude_cursor || c != *exclude_cursor) && c.Line() >= end.L; }
            );
        }
    }

    b.Edits = b.Edits.push_back({start_byte, old_end_byte, start_byte});
    return b;
}

TextBufferData TextBufferData::DeleteSelection(u32 i) const {
    const auto c = Cursors[i];
    if (!c.IsRange()) return *this;

    // Exclude the cursor whose selection is currently being deleted from having its position changed in `DeleteRange`.
    return DeleteRange(c, true, c).EditCursor(i, c.Min());
}

TextBufferData TextBufferData::EnterChar(ImWchar ch, bool auto_indent) const {
    auto b = *this;
    for (int c = Cursors.size() - 1; c > -1; --c) b = b.DeleteSelection(c);

    // Order is important here when typing '\n' in the same line with multiple cursors.
    for (int i = b.Cursors.size() - 1; i > -1; --i) {
        const auto &c = b.Cursors[i];
        TransientLine insert_line_trans{};
        if (ch == '\n') {
            if (auto_indent && c.CharIndex() != 0) {
                // Match the indentation of the current or next line, whichever has more indentation.
                // todo use tree-sitter fold queries
                const u32 li = c.Line();
                const u32 indent_li = li < b.Text.size() - 1 && b.NumStartingSpaceColumns(li + 1) > b.NumStartingSpaceColumns(li) ? li + 1 : li;
                const auto &indent_line = b.Text[indent_li];
                for (u32 i = 0; i < insert_line_trans.size() && isblank(indent_line[i]); ++i) insert_line_trans.push_back(indent_line[i]);
            }
        } else {
            char buf[5];
            ImTextCharToUtf8(buf, ch);
            for (u32 i = 0; i < 5 && buf[i] != '\0'; ++i) insert_line_trans.push_back(buf[i]);
        }
        auto insert_line = insert_line_trans.persistent();
        b = b.InsertTextAtCursor(ch == '\n' ? Lines{{}, insert_line} : Lines{insert_line}, i);
    }

    return b;
}

TextBufferData TextBufferData::Backspace(bool is_word_mode) const {
    auto b = *this;
    if (!AnyCursorsRanged()) {
        b = b.MoveCursorsChar(false, true, is_word_mode);
        // Can't do backspace if any cursor is at {0,0}.
        if (!b.AllCursorsRanged()) {
            if (b.AnyCursorsRanged()) return b.MoveCursorsChar(true); // Restore cursors.
        }
        b = b.MergeCursors();
    }
    for (int c = b.Cursors.size() - 1; c > -1; --c) b = b.DeleteSelection(c);
    return b;
}

TextBufferData TextBufferData::Delete(bool is_word_mode) const {
    auto b = *this;
    if (!AnyCursorsRanged()) {
        b = b.MoveCursorsChar(true, true, is_word_mode);
        // Can't do delete if any cursor is at the end of the last line.
        if (!b.AllCursorsRanged()) {
            if (b.AnyCursorsRanged()) return b.MoveCursorsChar(false); // Restore cursors.
        }
        b = b.MergeCursors();
    }
    for (int c = b.Cursors.size() - 1; c > -1; --c) b = b.DeleteSelection(c);
    return b;
}

TextBufferData TextBufferData::MoveCurrentLines(bool up) const {
    std::set<u32> affected_lines;
    u32 min_li = std::numeric_limits<u32>::max(), max_li = std::numeric_limits<u32>::min();
    for (const auto &c : Cursors) {
        for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
            // Check if selection ends at line start.
            if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

            affected_lines.insert(li);
            min_li = std::min(li, min_li);
            max_li = std::max(li, max_li);
        }
    }
    if ((up && min_li == 0) || (!up && max_li == Text.size() - 1)) return *this; // Can't move up/down anymore.

    auto b = *this;
    if (up) {
        for (const u32 li : affected_lines) b = b.SwapLines(li - 1, li);
    } else {
        for (auto it = affected_lines.rbegin(); it != affected_lines.rend(); ++it) b = b.SwapLines(*it, *it + 1);
    }
    return b.MoveCursorsLines(up ? -1 : 1, true, true, true);
}

TextBufferData TextBufferData::ToggleLineComment(const std::string &comment) const {
    if (comment.empty()) return *this;

    static const auto FindFirstNonSpace = [](const Line &line) { return std::distance(line.begin(), std::ranges::find_if_not(line, isblank)); };

    std::unordered_set<u32> affected_lines;
    for (const auto &c : Cursors) {
        for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
            if (!(c.IsRange() && c.Max() == LineChar{li, 0}) && !Text[li].empty()) affected_lines.insert(li);
        }
    }

    const bool should_add_comment = any_of(affected_lines, [&](u32 li) {
        return !Equals(comment, Text[li], FindFirstNonSpace(Text[li]));
    });

    auto b = *this;
    for (u32 li : affected_lines) {
        if (should_add_comment) {
            b = b.InsertText({Line{comment.begin(), comment.end()} + Line{' '}}, {li, 0}).first;
        } else {
            const auto &line = b.Text[li];
            const u32 ci = FindFirstNonSpace(line);
            u32 comment_ci = ci + comment.length();
            if (comment_ci < line.size() && line[comment_ci] == ' ') ++comment_ci;

            b = b.DeleteRange({{li, ci}, {li, comment_ci}});
        }
    }
    return b;
}

TextBufferData TextBufferData::DeleteCurrentLines() const {
    auto b = *this;
    for (int c = b.Cursors.size() - 1; c > -1; --c) b = b.DeleteSelection(c);
    b = b.MoveCursorsStartLine().MergeCursors();

    for (const auto &c : reverse_view(b.Cursors)) {
        const u32 li = c.Line();
        b = b.DeleteRange({li == b.Text.size() - 1 && li > 0 ? b.LineMaxLC(li - 1) : LineChar{li, 0}, b.CheckedNextLineBegin(li)});
    }
    return b;
}

TextBufferData TextBufferData::ChangeCurrentLinesIndentation(bool increase) const {
    auto b = *this;
    for (const auto &c : reverse_view(b.Cursors)) {
        for (u32 li = c.Min().L; li <= c.Max().L; ++li) {
            // Check if selection ends at line start.
            if (c.IsRange() && c.Max() == LineChar{li, 0}) continue;

            const auto &line = b.Text[li];
            if (increase) {
                if (!line.empty()) b = b.InsertText({{'\t'}}, {li, 0}).first;
            } else {
                auto ci = int(b.GetCharIndex(line, GTextBufferStyle.NumTabSpaces)) - 1;
                while (ci > -1 && isblank(line[ci])) --ci;
                if (const bool only_space_chars_found = ci == -1; only_space_chars_found) {
                    b = b.DeleteRange({{li, 0}, {li, b.GetCharIndex(line, GTextBufferStyle.NumTabSpaces)}});
                }
            }
        }
    }
    return b;
}

TextBufferData TextBufferData::SelectNextOccurrence(bool case_sensitive) const {
    const auto &c = LastAddedCursor();
    if (const auto match_range = FindNextOccurrence(GetText(c), c.Max(), case_sensitive)) {
        return SetCursor({match_range->Start, match_range->End}, true);
    }
    return *this;
}
