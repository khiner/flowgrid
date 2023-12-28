#include "LanguageDefinition.h"

static bool TokenizeCStyleString(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    if (*p == '"') {
        p++;
        while (p < in_end) {
            // handle end of string
            if (*p == '"') {
                out_begin = in_begin;
                end_out = p + 1;
                return true;
            }

            // handle escape character for "
            if (*p == '\\' && p + 1 < in_end && p[1] == '"') p++;
            p++;
        }
    }

    return false;
}

static bool TokenizeCStyleCharacterLiteral(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    if (*p == '\'') {
        p++;
        // handle escape characters
        if (p < in_end && *p == '\\') p++;
        if (p < in_end) p++;

        // handle end of character literal
        if (p < in_end && *p == '\'') {
            out_begin = in_begin;
            end_out = p + 1;
            return true;
        }
    }

    return false;
}

static bool TokenizeCStyleIdentifier(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
        p++;
        while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) p++;

        out_begin = in_begin;
        end_out = p;
        return true;
    }

    return false;
}

static bool TokenizeCStyleNumber(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    const bool starts_with_number = *p >= '0' && *p <= '9';
    if (*p != '+' && *p != '-' && !starts_with_number) return false;

    p++;
    bool has_number = starts_with_number;
    while (p < in_end && (*p >= '0' && *p <= '9')) {
        has_number = true;
        p++;
    }
    if (!has_number) return false;

    bool is_float = false, is_hex = false, is_binary = false;
    if (p < in_end) {
        if (*p == '.') {
            is_float = true;
            p++;
            while (p < in_end && (*p >= '0' && *p <= '9')) p++;
        } else if (*p == 'x' || *p == 'X') {
            // hex formatted integer of the type 0xef80
            is_hex = true;
            p++;
            while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) p++;
        } else if (*p == 'b' || *p == 'B') {
            // binary formatted integer of the type 0b01011101
            is_binary = true;
            p++;
            while (p < in_end && (*p >= '0' && *p <= '1')) p++;
        }
    }

    if (!is_hex && !is_binary) {
        // floating point exponent
        if (p < in_end && (*p == 'e' || *p == 'E')) {
            is_float = true;
            p++;

            if (p < in_end && (*p == '+' || *p == '-')) p++;

            bool has_digits = false;
            while (p < in_end && (*p >= '0' && *p <= '9')) {
                has_digits = true;
                p++;
            }

            if (!has_digits) return false;
        }

        // single precision floating point type
        if (p < in_end && *p == 'f') p++;
    }

    if (!is_float) {
        // integer size type
        while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L')) p++;
    }

    out_begin = in_begin;
    end_out = p;
    return true;
}

static bool TokenizeCStylePunctuation(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    (void)in_end;

    switch (*in_begin) {
        case '[':
        case ']':
        case '{':
        case '}':
        case '!':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '~':
        case '|':
        case '<':
        case '>':
        case '?':
        case ':':
        case '/':
        case ';':
        case ',':
        case '.':
            out_begin = in_begin;
            end_out = in_begin + 1;
            return true;
    }

    return false;
}

static bool TokenizeLuaStyleString(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;

    bool is_single_quote = false, is_double_quotes = false, is_double_square_brackets = false;
    switch (*p) {
        case '\'':
            is_single_quote = true;
            break;
        case '"':
            is_double_quotes = true;
            break;
        case '[':
            p++;
            if (p < in_end && *(p) == '[') is_double_square_brackets = true;
            break;
    }

    if (is_single_quote || is_double_quotes || is_double_square_brackets) {
        p++;
        while (p < in_end) {
            // handle end of string
            if ((is_single_quote && *p == '\'') || (is_double_quotes && *p == '"') || (is_double_square_brackets && *p == ']' && p + 1 < in_end && *(p + 1) == ']')) {
                out_begin = in_begin;
                end_out = is_double_square_brackets ? p + 2 : p + 1;
                return true;
            }
            // handle escape character for "
            if (*p == '\\' && p + 1 < in_end && (is_single_quote || is_double_quotes)) p++;
            p++;
        }
    }

    return false;
}

static bool TokenizeLuaStyleIdentifier(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
        p++;
        while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) p++;

        out_begin = in_begin;
        end_out = p;
        return true;
    }

    return false;
}

static bool TokenizeLuaStyleNumber(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    const char *p = in_begin;
    const bool starts_with_number = *p >= '0' && *p <= '9';
    if (*p != '+' && *p != '-' && !starts_with_number) return false;

    p++;
    bool has_number = starts_with_number;
    while (p < in_end && (*p >= '0' && *p <= '9')) {
        has_number = true;
        p++;
    }
    if (!has_number) return false;

    if (p < in_end) {
        if (*p == '.') {
            p++;
            while (p < in_end && (*p >= '0' && *p <= '9')) p++;
        }

        // floating point exponent
        if (p < in_end && (*p == 'e' || *p == 'E')) {
            p++;
            if (p < in_end && (*p == '+' || *p == '-')) p++;

            bool has_digits = false;
            while (p < in_end && (*p >= '0' && *p <= '9')) {
                has_digits = true;
                p++;
            }

            if (!has_digits) return false;
        }
    }

    out_begin = in_begin;
    end_out = p;
    return true;
}

static bool TokenizeLuaStylePunctuation(const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out) {
    (void)in_end;

    switch (*in_begin) {
        case '[':
        case ']':
        case '{':
        case '}':
        case '!':
        case '%':
        case '#':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '~':
        case '|':
        case '<':
        case '>':
        case '?':
        case ':':
        case '/':
        case ';':
        case ',':
        case '.':
            out_begin = in_begin;
            end_out = in_begin + 1;
            return true;
    }

    return false;
}

const LanguageDefinition LanguageDefinition::Cpp = {
    .Name = "C++",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
        "compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
        "for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
        "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
    },
    .Identifiers = {"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper", "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"},
    .Tokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndex &palette_index) -> bool {
        while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin)) in_begin++;

        palette_index = PaletteIndex::Max;
        if (in_begin == in_end) {
            out_begin = in_end;
            end_out = in_end;
            palette_index = PaletteIndex::Default;
        } else if (TokenizeCStyleString(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::String;
        else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::CharLiteral;
        else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Identifier;
        else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Number;
        else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Punctuation;

        return palette_index != PaletteIndex::Max;
    },
};

const LanguageDefinition LanguageDefinition::Hlsl = {
    .Name = "HLSL",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer",
        "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment", "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer",
        "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else", "export", "extern",
        "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface",
        "line", "lineadj", "linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective",
        "NULL", "out", "OutputPatch", "packoffset", "pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView",
        "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer", "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D",
        "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state", "static", "string", "struct", "switch",
        "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray",
        "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector",
        "vertexfragment", "VertexShader", "void", "volatile", "while", "bool1", "bool2", "bool3", "bool4", "double1", "double2", "double3", "double4", "float1", "float2", "float3", "float4",
        "int1", "int2", "int3", "int4", "in", "out", "inout", "uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
        "float1x1", "float2x1", "float3x1", "float4x1", "float1x2", "float2x2", "float3x2", "float4x2", "float1x3", "float2x3", "float3x3", "float4x3", "float1x4", "float2x4", "float3x4", "float4x4",
        "half1x1", "half2x1", "half3x1", "half4x1", "half1x2", "half2x2", "half3x2", "half4x2", "half1x3", "half2x3", "half3x3", "half4x3", "half1x4", "half2x4", "half3x4", "half4x4"
    },
    .Identifiers = {"abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg", "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"},
    .TokenRegexStrings = {
        {R"##([ \t]*#[ \t]*[a-zA-Z_]+)##", PaletteIndex::Preprocessor},
        {R"##(L?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##(\'\\?[^\']\')##", PaletteIndex::CharLiteral},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::Glsl = {
    .Name = "GLSL",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
        "_Noreturn", "_Static_assert", "_Thread_local"
    },
    .Identifiers = {"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"},
    .TokenRegexStrings = {
        {R"##([ \t]*#[ \t]*[a-zA-Z_]+)##", PaletteIndex::Preprocessor},
        {R"##(L?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##(\'\\?[^\']\')##", PaletteIndex::CharLiteral},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::Python = {
    .Name = "Python",
    .CommentStart = "\"\"\"",
    .CommentEnd = "\"\"\"",
    .SingleLineComment = "#",
    .IsCaseSensitive = true,
    .Keywords = {
        "False", "await", "else", "import", "pass", "None", "break", "except", "in", "raise", "True", "class", "finally", "is", "return", "and", "continue",
        "for", "lambda", "try", "as", "def", "from", "nonlocal", "while", "assert", "del", "global", "not", "with", "async", "elif", "if", "or", "yield"
    },
    .Identifiers = {"abs", "aiter", "all", "any", "anext", "ascii", "bin", "bool", "breakpoint", "bytearray", "bytes", "callable", "chr", "classmethod", "compile", "complex", "delattr", "dict", "dir", "divmod", "enumerate", "eval", "exec", "filter", "float", "format", "frozenset", "getattr", "globals", "hasattr", "hash", "help", "hex", "id", "input", "int", "isinstance", "issubclass", "iter", "len", "list", "locals", "map", "max", "memoryview", "min", "next", "object", "oct", "open", "ord", "pow", "print", "property", "range", "repr", "reversed", "round", "set", "setattr", "slice", "sorted", "staticmethod", "str", "sum", "super", "tuple", "type", "vars", "zip", "__import__"},
    .TokenRegexStrings = {
        {R"##((b|u|f|r)?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##((b|u|f|r)?'(\\.|[^'])*')##", PaletteIndex::String},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.\:])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::C = {
    .Name = "C",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
        "_Noreturn", "_Static_assert", "_Thread_local"
    },
    .Identifiers = {"abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"},
    .Tokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndex &palette_index) -> bool {
        while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin)) in_begin++;

        palette_index = PaletteIndex::Max;
        if (in_begin == in_end) {
            out_begin = in_end;
            end_out = in_end;
            palette_index = PaletteIndex::Default;
        } else if (TokenizeCStyleString(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::String;
        else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::CharLiteral;
        else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Identifier;
        else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Number;
        else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Punctuation;

        return palette_index != PaletteIndex::Max;
    },
};

const LanguageDefinition LanguageDefinition::Sql = {
    .Name = "SQL",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "--",
    .IsCaseSensitive = false,
    .Keywords = {
        "ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
        "AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE",
        "BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE",
        "CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE",
        "COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER",
        "CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE",
        "CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION",
        "DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
        "DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING", "DROP", "OPENROWSET", "VIEW",
        "DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
    },
    .Identifiers = {"ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASCIISTR", "ASIN", "ATAN", "ATAN2", "AVG", "BFILENAME", "BIN_TO_NUM", "BITAND", "CARDINALITY", "CASE", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE", "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT", "COVAR_POP", "COVAR_SAMP", "CUME_DIST", "CURRENT_DATE", "CURRENT_TIMESTAMP", "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK", "DUMP", "EMPTY_BLOB", "EMPTY_CLOB", "EXP", "EXTRACT", "FIRST_VALUE", "FLOOR", "FROM_TZ", "GREATEST", "GROUP_ID", "HEXTORAW", "INITCAP", "INSTR", "INSTR2", "INSTR4", "INSTRB", "INSTRC", "LAG", "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH", "LENGTH2", "LENGTH4", "LENGTHB", "LENGTHC", "LISTAGG", "LN", "LNNVL", "LOCALTIMESTAMP", "LOG", "LOWER", "LPAD", "LTRIM", "MAX", "MEDIAN", "MIN", "MOD", "MONTHS_BETWEEN", "NANVL", "NCHR", "NEW_TIME", "NEXT_DAY", "NTH_VALUE", "NULLIF", "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2", "POWER", "RANK", "RAWTOHEX", "REGEXP_COUNT", "REGEXP_INSTR", "REGEXP_REPLACE", "REGEXP_SUBSTR", "REMAINDER", "REPLACE", "ROUND", "ROWNUM", "RPAD", "RTRIM", "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT", "STDDEV", "SUBSTR", "SUM", "SYS_CONTEXT", "SYSDATE", "SYSTIMESTAMP", "TAN", "TANH", "TO_CHAR", "TO_CLOB", "TO_DATE", "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE", "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE", "TRIM", "TRUNC", "TZ_OFFSET", "UID", "UPPER", "USER", "USERENV", "VAR_POP", "VAR_SAMP", "VARIANCE", "VSIZE"},
    .TokenRegexStrings = {
        {R"##(L?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##(\'[^\']*\')##", PaletteIndex::String},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::AngelScript = {
    .Name = "AngelScript",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for",
        "from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not",
        "null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
        "uint64", "void", "while", "xor"
    },
    .Identifiers = {"cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE", "complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"},
    .TokenRegexStrings = {
        {R"##(L?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##(\'\\?[^\']\')##", PaletteIndex::String},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::Lua = {
    .Name = "Lua",
    .CommentStart = "--[[",
    .CommentEnd = "]]",
    .SingleLineComment = "--",
    .IsCaseSensitive = true,
    .Keywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
    },
    .Identifiers = {"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset", "select", "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", "rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", "getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", "read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger", "floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh", "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock", "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep", "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern", "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"},
    .Tokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&end_out, PaletteIndex &palette_index) -> bool {
        while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin)) in_begin++;

        palette_index = PaletteIndex::Max;
        if (in_begin == in_end) {
            out_begin = in_end;
            end_out = in_end;
            palette_index = PaletteIndex::Default;
        } else if (TokenizeLuaStyleString(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::String;
        else if (TokenizeLuaStyleIdentifier(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Identifier;
        else if (TokenizeLuaStyleNumber(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Number;
        else if (TokenizeLuaStylePunctuation(in_begin, in_end, out_begin, end_out))
            palette_index = PaletteIndex::Punctuation;

        return palette_index != PaletteIndex::Max;
    },
};

const LanguageDefinition LanguageDefinition::Cs = {
    .Name = "C#",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {
        "abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "checked", "class", "const", "continue", "decimal", "default",
        "delegate", "do", "double", "else", "enum", "event", "explicit", "extern", "false", "finally", "fixed", "float", "for", "foreach", "goto",
        "if", "implicit", "in", "in (generic modifier)", "int", "interface", "internal", "is", "lock", "long", "namespace", "new", "null", "object",
        "operator", "out", "out (generic modifier)", "override", "params", "private", "protected", "public", "readonly", "ref", "return", "sbyte",
        "sealed", "short", "sizeof", "stackalloc", "static", "string", "struct", "switch", "this", "throw", "true", "try", "typeof", "uint", "ulong",
        "unchecked", "unsafe", "ushort", "using", "using static", "void", "volatile", "while"
    },
    .Identifiers = {"add", "alias", "ascending", "async", "await", "descending", "dynamic", "from", "get", "global", "group", "into", "join", "let", "orderby", "partial", "remove", "select", "set", "value", "var", "when", "where", "yield"},
    .TokenRegexStrings = {
        {R"##(($|@)?\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?)##", PaletteIndex::Number},
        {R"##([+-]?[0-9]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[0-7]+[Uu]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##(0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?)##", PaletteIndex::Number},
        {R"##([a-zA-Z_][a-zA-Z0-9_]*)##", PaletteIndex::Identifier},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.])##", PaletteIndex::Punctuation},
    },
};

const LanguageDefinition LanguageDefinition::Jsn = {
    .Name = "Json",
    .CommentStart = "/*",
    .CommentEnd = "*/",
    .SingleLineComment = "//",
    .IsCaseSensitive = true,
    .Keywords = {},
    .Identifiers = {},
    .TokenRegexStrings = {
        {R"##(\"(\\.|[^\"])*\")##", PaletteIndex::String},
        {R"##([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?)##", PaletteIndex::Number},
        {R"##([\[\]\{\}\!\%\^\&\*\(\)\-\+\=\~\|\<\>\?\/\;\,\.\:])##", PaletteIndex::Punctuation},
        {R"##(false|true)##", PaletteIndex::Keyword},
    },
};