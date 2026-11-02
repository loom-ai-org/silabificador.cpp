// Internal code-point layer. The Python package indexes `str` by code point, so
// every rule here runs on std::u32string and the public UTF-8 API converts at
// the boundary.
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "silabificador/silabificador.hpp"

namespace silabificador::detail {

using U32 = std::u32string;

U32 from_utf8(std::string_view text);
std::string to_utf8(std::u32string_view text);

// Python str.strip() with no argument: removes str.isspace() code points.
U32 py_strip(const U32& text);
// Python str.lower(): full Unicode lowercase mapping.
U32 py_lower(const U32& text);

bool contains(std::u32string_view set, char32_t c);
bool ends_with(std::u32string_view text, std::u32string_view suffix);
bool starts_with(std::u32string_view text, std::u32string_view prefix);

struct Unit {
    U32 text;
    graphemes::Kind kind;
    bool is_vowel() const { return kind == graphemes::Kind::Vowel; }
    bool is_consonant() const { return kind == graphemes::Kind::Consonant; }
    bool is_separator() const { return kind == graphemes::Kind::Separator; }
};

std::vector<Unit> tokenize(const U32& word);
std::vector<nucleus::Group> resolve(const std::vector<Unit>& units, int start, int end);
std::set<int> boundaries(const U32& word, const std::vector<Unit>& units);
Syllable syllable_of(const std::vector<Unit>& units, const nucleus::Group* nucleus);
std::vector<Syllable> parse(const U32& word);
std::vector<Syllable> assign(const std::vector<Syllable>& syllables, const U32& word);
int stressed_index(const std::vector<U32>& syllables, const U32& word);

std::vector<Unit> to_units(const std::vector<graphemes::Grapheme>& units);
std::vector<graphemes::Grapheme> from_units(const std::vector<Unit>& units);

}  // namespace silabificador::detail
