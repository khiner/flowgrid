#pragma once

#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nlohmann/json.hpp"
#include <tree_sitter/api.h>

#include "Helper/Hex.h"
#include "LanguageID.h"

using json = nlohmann::json;

using std::views::filter, std::ranges::reverse_view;

// Implemented by the grammar libraries in `lib/tree-sitter-grammars/`.
extern "C" TSLanguage *tree_sitter_cpp();
extern "C" TSLanguage *tree_sitter_faust();
extern "C" TSLanguage *tree_sitter_json();

/*
WIP Syntax highlighting strategy:
Manually convert lua vim themes to `tree-sitter/config.json` themes.

Starting with:
https://github.com/TomLebeda/chroma_code/blob/main/examples/config-example.json
since this is based on nvim-treesitter highlight groups.

Next, convert e.g. https://github.com/folke/tokyonight.nvim/blob/main/lua/tokyonight/theme.lua#L211-L323,
tracing the nvim tree-sitter highlights through the theme highlight names to the colors/styles.

Other themes: Lots of folks recommend https://github.com/sainnhe/sonokai
There's also this huge list: https://github.com/rockerBOO/awesome-neovim?tab=readme-ov-file#tree-sitter-supported-colorscheme
*/

struct LanguageDefinition {
    // todo recursively copy `queries` dir to build dir in CMake.
    inline static fs::path QueriesDir = fs::path("..") / "src" / "FlowGrid" / "Project" / "TextEditor" / "queries";

    TSQuery *GetQuery() const;

    LanguageID Id;
    std::string Name;
    std::string ShortName{""}; // e.g. "cpp" in "tree-sitter-cpp"
    TSLanguage *TsLanguage{nullptr};
    std::unordered_set<std::string> FileExtensions{};
    std::string SingleLineComment{""};
};

struct LanguageDefinitions {
    using ID = LanguageID;

    LanguageDefinitions();

    const LanguageDefinition &Get(ID id) const { return ById.at(id); }

    std::unordered_map<ID, LanguageDefinition> ById;
    std::unordered_map<std::string, LanguageID> ByFileExtension;
    std::string AllFileExtensionsFilter;
};

static const LanguageDefinitions Languages{};

TSQuery *LanguageDefinition::GetQuery() const {
    if (ShortName.empty()) return nullptr;

    const fs::path highlights_path = QueriesDir / ShortName / "highlights.scm";
    const std::string highlights = FileIO::read(highlights_path);

    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    auto *query = ts_query_new(
        TsLanguage,
        highlights.c_str(),
        highlights.size(),
        &error_offset,
        &error_type
    );
    // todo better error handling
    if (error_type != TSQueryErrorNone) {
        std::println("Error parsing tree-sitter query: {}", int(error_type));
    }
    return query;
}

LanguageDefinitions::LanguageDefinitions()
    : ById{
          {ID::None, {ID::None, "None"}},
          {ID::Cpp, {ID::Cpp, "C++", "cpp", tree_sitter_cpp(), {".h", ".hpp", ".cpp"}, "//"}},
          {ID::Faust, {ID::Faust, "Faust", "faust", tree_sitter_faust(), {".dsp"}, "//"}},
          {ID::Json, {ID::Json, "JSON", "json", tree_sitter_json(), {".json"}}},
      } {
    for (const auto &[id, lang] : ById) {
        for (const auto &ext : lang.FileExtensions) ByFileExtension[ext] = id;
    }
    for (const auto &ext : ByFileExtension | std::views::keys) AllFileExtensionsFilter += ext + ',';
}

constexpr ImU32 Col32(uint r, uint g, uint b, uint a = 255) { return IM_COL32(r, g, b, a); }

static ImU32 AnsiToRgb(uint code) {
    // Standard ANSI colors in hex, mapped directly to ImU32
    static const ImU32 StandardColors[16] = {
        Col32(0, 0, 0), // Black
        Col32(128, 0, 0), // Red
        Col32(0, 128, 0), // Green
        Col32(128, 128, 0), // Yellow
        Col32(0, 0, 128), // Blue
        Col32(128, 0, 128), // Magenta
        Col32(0, 128, 128), // Cyan
        Col32(192, 192, 192), // White
        Col32(128, 128, 128), // Black (bright)
        Col32(255, 0, 0), // Red (bright)
        Col32(0, 255, 0), // Green (bright)
        Col32(255, 255, 0), // Yellow (bright)
        Col32(0, 0, 255), // Blue (bright)
        Col32(255, 0, 255), // Magenta (bright)
        Col32(0, 255, 255), // Cyan (bright)
        Col32(255, 255, 255), // White (bright)
    };

    if (code < 16) return StandardColors[code];
    // All codes >= 16 are left up to the terminal implementation.
    // The following is a programmatic strategy to convert the >= 16 range to RGB.
    if (code < 232) {
        // 6x6x6 color cube
        static const uint step = 255 / 5;
        const uint red = (code - 16) / 36, green = (code - 16) / 6 % 6, blue = (code - 16) % 6;
        return IM_COL32(red * step, green * step, blue * step, 255);
    }
    if (code <= 255) {
        // Grayscale ramp, starts at 8 and increases by 10 up to 238.
        const uint shade = 8 + (code - 232) * 10;
        return IM_COL32(shade, shade, shade, 255);
    }
    return IM_COL32(0, 0, 0, 255); // Default to black if out of range.
}

ImU32 CharStyleColorValuetoU32(const json &j) {
    static const std::unordered_map<std::string, ImU32> ColorByName = {
        {"black", Col32(0, 0, 0)},
        {"blue", Col32(0, 0, 255)},
        {"cyan", Col32(0, 255, 255)},
        {"green", Col32(0, 255, 0)},
        {"purple", Col32(128, 0, 128)},
        {"red", Col32(255, 0, 0)},
        {"white", Col32(255, 255, 255)},
        {"yellow", Col32(255, 255, 0)},
    };

    if (j.is_string()) {
        const auto str_value = j.get<std::string>();
        if (IsHex(str_value)) return HexToU32(str_value);
        if (auto it = ColorByName.find(str_value); it != ColorByName.end()) return it->second;
        throw std::runtime_error("Unsupported color name in tree-sitter config JSON.");
    }
    if (j.is_number()) return AnsiToRgb(j.get<uint>());

    throw std::runtime_error("Invalid color type in tree-sitter config JSON.");
}

struct TSConfig {
    std::vector<std::string> ParserDirectories{};
    std::unordered_map<std::string, TextEditorStyle::CharStyle> StyleByHighlightName{};

    inline static const TextEditorStyle::CharStyle DefaultCharStyle{};

    TextEditorStyle::CharStyle FindStyleByCaptureName(const std::string &) const;
};

void from_json(const json &j, TextEditorStyle::CharStyle &style) {
    if (j.is_object()) {
        if (j.contains("color")) style.Color = CharStyleColorValuetoU32(j.at("color"));
        if (j.contains("bold")) j.at("bold").get_to(style.Bold);
        if (j.contains("italic")) j.at("italic").get_to(style.Italic);
        if (j.contains("underline")) j.at("italic").get_to(style.Underline);
    } else if (j.is_number() || j.is_string()) {
        style.Color = CharStyleColorValuetoU32(j);
    } else {
        throw std::runtime_error("Invalid theme style type in tree-sitter config JSON.");
    }
}
void from_json(const json &j, TSConfig &config) {
    j.at("parser-directories").get_to(config.ParserDirectories);
    const auto &theme = j.at("theme");
    for (const auto &[key, value] : theme.items()) {
        if (!value.is_null()) {
            config.StyleByHighlightName[key] = value.get<TextEditorStyle::CharStyle>();
        }
    }
}

template<typename ValueType> struct ByteTransitions {
    struct DeltaValue {
        uint Delta;
        ValueType Value;
    };

    struct Iterator {
        Iterator(const std::vector<DeltaValue> &deltas, ValueType default_value = {})
            : DeltaValues(deltas), DefaultValue(default_value) {
        }

        bool HasNext() const { return DeltaIndex < DeltaValues.size() - 1; }
        bool HasPrev() const { return DeltaIndex > 0; }
        bool IsEnd() const { return DeltaIndex == DeltaValues.size(); }

        Iterator &operator++() {
            MoveRight();
            return *this;
        }
        Iterator &operator--() {
            MoveLeft();
            return *this;
        }

        uint NextByteIndex() const { return HasNext() ? ByteIndex + DeltaValues[DeltaIndex + 1].Delta : ByteIndex; }

        /* Move ops all move until `ByteIndex` is _at or before_ the target byte. */
        void MoveTo(uint target_byte) {
            if (ByteIndex < target_byte) MoveForwardTo(target_byte);
            else MoveBackTo(target_byte);
        }
        void MoveForwardTo(uint target_byte) {
            while (ByteIndex < target_byte && !IsEnd() && NextByteIndex() <= target_byte) ++(*this);
        }
        void MoveBackTo(uint target_byte) {
            while ((ByteIndex > target_byte || (IsEnd() && ByteIndex == target_byte)) && HasPrev()) --(*this);
        }

        ValueType operator*() const { return !IsEnd() ? DeltaValues[DeltaIndex].Value : DefaultValue; }

        const std::vector<DeltaValue> &DeltaValues;
        uint DeltaIndex{0}, ByteIndex{0};
        ValueType DefaultValue{};

    private:
        void MoveRight() {
            if (IsEnd()) throw std::out_of_range("Iterator out of bounds");
            ++DeltaIndex;
            if (!IsEnd()) ByteIndex += DeltaValues[DeltaIndex].Delta;
        }
        void MoveLeft() {
            if (!HasPrev()) throw std::out_of_range("Iterator out of bounds");
            if (!IsEnd()) ByteIndex -= DeltaValues[DeltaIndex].Delta;
            --DeltaIndex;
        }
    };

    ByteTransitions(ValueType default_value = {}) : DefaultValue(default_value) {
        EnsureStartTransition();
    }

    void Insert(Iterator &it, uint byte_index, ValueType value) {
        it.MoveTo(byte_index);
        if (it.DeltaIndex > DeltaValues.size()) throw std::out_of_range("Insert iterator out of bounds");
        if (byte_index < it.ByteIndex) throw std::invalid_argument("Insert byte index is before iterator");
        if (byte_index == it.ByteIndex && !DeltaValues.empty()) {
            DeltaValues[it.DeltaIndex].Value = value;
            if (!it.IsEnd()) ++it;
            return;
        }

        const uint delta = byte_index - it.ByteIndex;
        if (it.IsEnd()) --it;
        DeltaValues.insert(DeltaValues.begin() + it.DeltaIndex + 1, {delta, value});
        ++it;
        if (it.HasNext()) DeltaValues[it.DeltaIndex + 1].Delta -= delta;
    }

    // Start is inclusive, end is exclusive.
    void Delete(Iterator &it, uint start_byte, uint end_byte) {
        it.MoveTo(start_byte);
        const uint start_index = start_byte <= it.ByteIndex ? it.DeltaIndex : it.DeltaIndex + 1;
        it.MoveTo(end_byte - 1);
        if (it.ByteIndex < start_byte) return;

        if (!it.IsEnd()) ++it;
        const uint end_index = it.DeltaIndex;

        // We're now one element after the last element to be deleted.
        // Merge the deltas of all elements to delete into this element (if we're not past the end).
        const uint deleted_delta = std::accumulate(DeltaValues.begin() + start_index, DeltaValues.begin() + end_index, 0u, [](uint sum, const DeltaValue &dv) { return sum + dv.Delta; });
        if (!it.IsEnd()) DeltaValues[it.DeltaIndex].Delta += deleted_delta;
        else it.ByteIndex -= deleted_delta;
        it.DeltaIndex -= end_index - start_index;
        DeltaValues.erase(DeltaValues.begin() + start_index, DeltaValues.begin() + end_index);
        if (EnsureStartTransition()) ++it.DeltaIndex;
    }

    void Increment(Iterator &it, int amount) {
        if (amount == 0 || it.IsEnd()) return;

        if (it.DeltaIndex == 0 && it.HasNext()) ++it;
        DeltaValues[it.DeltaIndex].Delta += amount;
        it.ByteIndex += amount;
    }

    auto begin() const { return Iterator(DeltaValues, DefaultValue); }
    uint size() const { return DeltaValues.size(); }
    void clear() {
        DeltaValues.clear();
        EnsureStartTransition();
    }

    std::vector<DeltaValue> DeltaValues;
    ValueType DefaultValue;

private:
    bool EnsureStartTransition() {
        // All documents start by "transitioning" to the default capture (showing the default style).
        if (DeltaValues.empty() || DeltaValues.front().Delta != 0) {
            DeltaValues.insert(DeltaValues.begin(), {0, DefaultValue});
            return true;
        }
        return false;
    }
};

struct ByteRange {
    uint Start{0}, End{0};

    auto operator<=>(const ByteRange &o) const {
        if (auto cmp = Start <=> o.Start; cmp != 0) return cmp;
        return End <=> o.End;
    }
    bool operator==(const ByteRange &) const = default;
    bool operator!=(const ByteRange &) const = default;
};

static ByteRange ToByteRange(const TSNode &node) { return {ts_node_start_byte(node), ts_node_end_byte(node)}; }

struct SyntaxTree {
    // todo take language
    SyntaxTree(TSInput input) : Input(input), Parser(ts_parser_new()) {}
    ~SyntaxTree() {
        if (QueryCursor) ts_query_cursor_delete(QueryCursor);
        if (Parser) ts_parser_delete(Parser);
        if (Tree) ts_tree_delete(Tree);
    }

    // Apply edits to the TS tree, re-parse, update highlight state.
    void ApplyEdits(const std::vector<TextInputEdit> &edits) {
        ChangedCaptureRanges.clear(); // For debugging
        if (edits.empty()) return;

        if (Tree != nullptr) {
            for (const auto &edit : edits) {
                const TSInputEdit ts_edit{.start_byte = edit.StartByte, .old_end_byte = edit.OldEndByte, .new_end_byte = edit.NewEndByte};
                ts_tree_edit(Tree, &ts_edit);
            }
        }

        auto *old_tree = Tree;
        Tree = ts_parser_parse(Parser, Tree, Input);
        if (!Query) return;

        /* Update capture ID transition points (used for highlighting) based on the query and the edits. */

        // Find the minimum range needed to span all nodes whose syntactic structure has changed.
        ByteRange changed_range = {UINT32_MAX, 0u};
        if (old_tree != nullptr) {
            uint num_changed_ranges;
            const TSRange *changed_ranges = ts_tree_get_changed_ranges(old_tree, Tree, &num_changed_ranges);
            for (uint i = 0; i < num_changed_ranges; ++i) {
                changed_range.Start = std::min(changed_range.Start, changed_ranges[i].start_byte);
                changed_range.End = std::max(changed_range.End, changed_ranges[i].end_byte);
            }
            free((void *)changed_ranges);
        }

        const bool any_changed_captures = changed_range.Start < changed_range.End;
        if (any_changed_captures) {
            // std::println("Changed capture range: [{},{}]", changed_range.Start, changed_range.End);
            ts_query_cursor_set_byte_range(QueryCursor, changed_range.Start, changed_range.End);

            // Note: We don't delete all transitions in this range, since it might include ancestor nodes
            // with transitions that are still valid. We delete within replaced terminal node ranges below.
        }

        auto transition_it = CaptureIdTransitions.begin();

        // Adjust transitions based on the edited ranges, from the end to the start.
        const auto ordered_edits = std::set<TextInputEdit>(edits.begin(), edits.end());
        if (CaptureIdTransitions.size() > 1) {
            for (const auto &edit : reverse_view(ordered_edits)) {
                const uint inc_after_byte = edit.OldEndByte;
                transition_it.MoveTo(inc_after_byte);
                if (!transition_it.IsEnd()) {
                    if (transition_it.ByteIndex != inc_after_byte) ++transition_it;
                    CaptureIdTransitions.Increment(transition_it, edit.NewEndByte - edit.OldEndByte);
                }
            }
        }

        // Delete all transitions in deleted ranges? Not right in all cases.
        // for (const auto &edit : reverse_view(ordered_edits) | filter([](const auto &edit) { return edit.IsDelete(); })) {
        //     CaptureIdTransitions.Delete(transition_it, edit.NewEndByte, edit.OldEndByte);
        // }

        if (old_tree == nullptr || any_changed_captures) {
            // Either this is the first parse, or the edit(s) affect existing node captures.
            // Execute the query and add all capture transitions.
            ts_query_cursor_exec(QueryCursor, Query, ts_tree_root_node(Tree));

            TSQueryMatch match;
            uint capture_index;
            while (ts_query_cursor_next_capture(QueryCursor, &match, &capture_index)) {
                const TSQueryCapture &capture = match.captures[capture_index];
                // We only store the points at which there is a _transition_ from one style to another.
                // This can happen either at the capture node's beginning or end.
                const TSNode node = capture.node;
                if (ts_node_child_count(node) > 0) continue; // Only highlight terminal nodes.

                // Delete invalidated transitions and insert new ones.
                const auto node_byte_range = ToByteRange(node);
                ChangedCaptureRanges.insert(node_byte_range); // For debugging.
                CaptureIdTransitions.Delete(transition_it, node_byte_range.Start, node_byte_range.End);
                if (*transition_it != capture.index) {
                    // uint length;
                    // const char *capture_name = ts_query_capture_name_for_id(Query, capture.index, &length);
                    // std::println("\t'{}'[{}:{}]: {}", ts_node_type(node), node_byte_range.Start, node_byte_range.End, string(capture_name, length));
                    CaptureIdTransitions.Insert(transition_it, node_byte_range.Start, capture.index);
                    if (node_byte_range.End != changed_range.End) {
                        CaptureIdTransitions.Insert(transition_it, node_byte_range.End, NoneCaptureId);
                    }
                }
            }
        }

        // Cleanup: Delete all transitions beyond the new text range.
        CaptureIdTransitions.Delete(transition_it, ts_node_end_byte(ts_tree_root_node(Tree)) - 1, UINT32_MAX);
    }

    void SetLanguage(LanguageID language_id) {
        if (language_id != LanguageID::None && !Preferences.TreeSitterConfigPath.empty()) {
            const auto config_json = json::parse(FileIO::read(Preferences.TreeSitterConfigPath));
            Config = config_json.get<TSConfig>();
        } else if (language_id == LanguageID::None) {
            Config = {};
        }
        const auto &language = Languages.Get(language_id);
        ts_parser_set_language(Parser, language.TsLanguage);
        Query = language.GetQuery();
        const uint capture_count = ts_query_capture_count(Query);
        StyleByCaptureId.clear();
        StyleByCaptureId.reserve(capture_count + 1);
        StyleByCaptureId[NoneCaptureId] = Config.DefaultCharStyle;
        for (uint i = 0; i < capture_count; ++i) {
            uint length;
            const char *capture_name = ts_query_capture_name_for_id(Query, i, &length);
            StyleByCaptureId[i] = Config.FindStyleByCaptureName(std::string(capture_name, length));
        }

        Tree = nullptr;
        QueryCursor = ts_query_cursor_new();
        CaptureIdTransitions.clear();
    }

    std::string GetSExp() const {
        char *c_string = ts_node_string(ts_tree_root_node(Tree));
        std::string s_expression(c_string);
        free(c_string);
        return s_expression;
    }

    inline static uint NoneCaptureId{uint(-1)}; // Corresponds to the default style.

    TSInput Input;
    TSConfig Config;
    TSTree *Tree{nullptr};
    TSParser *Parser{nullptr};
    TSQuery *Query{nullptr};
    TSQueryCursor *QueryCursor{nullptr};
    std::unordered_map<uint, TextEditorStyle::CharStyle> StyleByCaptureId{};
    ByteTransitions<uint> CaptureIdTransitions{NoneCaptureId};
    std::set<ByteRange> ChangedCaptureRanges{}; // For debugging.
};
