/**
 * base-n, 1.0
 * Copyright (C) 2012 Andrzej Zawadzki (azawadzki@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/

// Implementation based on https://github.com/azawadzki/base-n/blob/master/include/basen.hpp

#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>

namespace bn {

const char Error = -1;

namespace {

static constexpr char extract_partial_bits(char value, size_t start_bit, size_t bits_count) {
    assert(start_bit + bits_count < 8);
    // shift extracted bits to the beginning of the byte
    char t1 = value >> (8 - bits_count - start_bit);
    // mask out bits on the left
    char t2 = t1 & ~(0xff << bits_count);
    return t2;
}

static constexpr char extract_overlapping_bits(char previous, char next, size_t start_bit, size_t bits_count) {
    assert(start_bit + bits_count < 16);
    size_t bits_count_in_previous = 8 - start_bit;
    size_t bits_count_in_next = bits_count - bits_count_in_previous;
    char t1 = static_cast<char>(previous << bits_count_in_next);
    char t2 = next >> (8 - bits_count_in_next) & ~(0xff << bits_count_in_next);
    return (t1 | t2) & ~(0xff << bits_count);
}

} // namespace

struct b16_conversion_traits {
    static constexpr size_t group_length() { return 4; }

    static constexpr char encode(unsigned int index) { return "0123456789ABCDEF"[index]; }

    static constexpr char decode(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return Error;
    }
};

struct b32_conversion_traits {
    static constexpr size_t group_length() { return 5; }

    static constexpr char encode(unsigned int index) { return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[index]; }

    static constexpr char decode(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= '2' && c <= '7') return c - '2' + 26;
        return Error;
    }
};

struct b64_conversion_traits {
    static constexpr size_t group_length() { return 6; }

    static constexpr char encode(unsigned int index) { return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[index]; }

    static constexpr char decode(char c) {
        constexpr int LetterCount = 26;
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + LetterCount * 1;
        if (c >= '0' && c <= '9') return c - '0' + LetterCount * 2;
        if (c == '+') return c - '+' + LetterCount * 2 + 10;
        if (c == '/') return c - '/' + LetterCount * 2 + 11;
        return Error;
    }
};

template<class ConversionTraits, class Iter1, class Iter2>
constexpr void decode(Iter1 start, Iter1 end, Iter2 out) {
    Iter1 iter = start;
    size_t output_current_bit = 0;
    char buffer = 0;

    while (iter != end) {
        if (std::isspace(*iter)) {
            ++iter;
            continue;
        }
        char value = ConversionTraits::decode(*iter);
        if (value == Error) {
            // malformed data, but let's go on...
            ++iter;
            continue;
        }
        size_t bits_in_current_byte = std::min<size_t>(output_current_bit + ConversionTraits::group_length(), 8) - output_current_bit;
        if (bits_in_current_byte == ConversionTraits::group_length()) {
            // the value fits within current byte, so we can extract it directly
            buffer |= value << (8 - output_current_bit - ConversionTraits::group_length());
            output_current_bit += ConversionTraits::group_length();
            // check if we filled up current byte completely; in such case we flush output and continue
            if (output_current_bit == 8) {
                *out++ = buffer;
                buffer = 0;
                output_current_bit = 0;
            }
        } else {
            // the value spans across the current and the next byte
            size_t bits_in_next_byte = ConversionTraits::group_length() - bits_in_current_byte;
            // fill the current byte and flush it to our output
            buffer |= value >> bits_in_next_byte;
            *out++ = buffer;
            buffer = 0;
            // save the remainder of our value in the buffer; it will be flushed during next iterations
            buffer |= value << (8 - bits_in_next_byte);
            output_current_bit = bits_in_next_byte;
        }
        ++iter;
    }
}

template<class ConversionTraits, class Iter1, class Iter2>
constexpr void encode(Iter1 start, Iter1 end, Iter2 out) {
    static_assert(sizeof(*start) == sizeof(char), "only char-size input supported");

    Iter1 iter = start;
    size_t start_bit = 0;
    bool has_backlog = false;
    char backlog = 0;
    while (has_backlog || iter != end) {
        if (!has_backlog) {
            if (start_bit + ConversionTraits::group_length() < 8) {
                // the value fits within single byte, so we can extract it directly
                char v = extract_partial_bits(*iter, start_bit, ConversionTraits::group_length());
                *out++ = ConversionTraits::encode(v);
                // since we know that start_bit + ConversionTraits::group_length() < 8 we don't need to go to the next byte
                start_bit += ConversionTraits::group_length();
            } else {
                // our bits are spanning across byte border; we need to keep the starting point and move over to next byte.
                backlog = *iter++;
                has_backlog = true;
            }
        } else {
            // encode value which is made from bits spanning across byte boundary
            char v = extract_overlapping_bits(backlog, iter == end ? 0 : *iter, start_bit, ConversionTraits::group_length());
            *out++ = ConversionTraits::encode(v);
            has_backlog = false;
            start_bit = (start_bit + ConversionTraits::group_length()) % 8;
        }
    }
}

template<class Iter1, class Iter2>
constexpr void encode_b16(Iter1 start, Iter1 end, Iter2 out) {
    encode<b16_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
constexpr void encode_b32(Iter1 start, Iter1 end, Iter2 out) {
    encode<b32_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
constexpr void encode_b64(Iter1 start, Iter1 end, Iter2 out) {
    encode<b64_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
constexpr void decode_b16(Iter1 start, Iter1 end, Iter2 out) {
    decode<b16_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
constexpr void decode_b32(Iter1 start, Iter1 end, Iter2 out) {
    decode<b32_conversion_traits>(start, end, out);
}

template<class Iter1, class Iter2>
constexpr void decode_b64(Iter1 start, Iter1 end, Iter2 out) {
    decode<b64_conversion_traits>(start, end, out);
}

} // namespace bn
