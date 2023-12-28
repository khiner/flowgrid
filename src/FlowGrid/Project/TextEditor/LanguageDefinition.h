#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "PaletteIndex.h"

struct LanguageDefinition {
    using TokenRegexStringT = std::pair<std::string, PaletteIndex>;
    using TokenizeCallbackT = bool (*)(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndex &palette_index);

    std::string Name, CommentStart, CommentEnd, SingleLineComment;
    bool IsCaseSensitive{true};
    std::unordered_set<std::string> Keywords, Identifiers;
    std::vector<TokenRegexStringT> TokenRegexStrings;
    TokenizeCallbackT Tokenize{nullptr};
    char PreprocChar{'#'};

    static const LanguageDefinition Cpp, Hlsl, Glsl, Python, C, Sql, AngelScript, Lua, Cs, Jsn;
};
