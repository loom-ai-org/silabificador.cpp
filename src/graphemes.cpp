// Layer 1: orthography to grapheme units.
//
// Each digraph is decided in context, so every later layer can assume one unit
// is one segment: the gu of guerra is one unit, the gu of agudo is two.
#include "detail.hpp"
#include "phonotactics.hpp"

namespace silabificador {
namespace detail {

namespace {
// Characters that hold a word together without being part of any syllable's
// sound: guarda-chuva, ajudante de campo, pau-d'água.
constexpr std::u32string_view kSeparators = U"- '’";

bool is_pair_digraph(std::u32string_view pair) {
    return pair == U"ch" || pair == U"lh" || pair == U"nh" || pair == U"sh" ||
           pair == U"th" || pair == U"ff" || pair == U"ph";
}

// After q or g, a u followed by another vowel spells nothing of its own (quero,
// guerra) or rides in the onset as a glide (quadro, água): qu/gu is one unit.
bool u_is_mute(const U32& word, size_t index) {
    if (word[index] != U'u') return false;
    size_t next = index + 1;
    return next < word.size() && contains(phonotactics::kVowels, word[next]);
}
}  // namespace

std::vector<Unit> tokenize(const U32& word) {
    std::vector<Unit> units;
    size_t i = 0;
    const size_t length = word.size();

    while (i < length) {
        char32_t c = word[i];
        std::u32string_view pair = std::u32string_view(word).substr(i, 2);

        if (contains(kSeparators, c)) {
            units.push_back({U32(1, c), graphemes::Kind::Separator});
            i += 1;
        } else if (is_pair_digraph(pair)) {
            units.push_back({U32(pair), graphemes::Kind::Consonant});
            i += 2;
        } else if ((c == U'q' || c == U'g') && i + 1 < length && u_is_mute(word, i + 1)) {
            units.push_back({U32(pair), graphemes::Kind::Consonant});
            i += 2;
        } else if (contains(phonotactics::kVowels, c)) {
            units.push_back({U32(1, c), graphemes::Kind::Vowel});
            i += 1;
        } else {
            units.push_back({U32(1, c), graphemes::Kind::Consonant});
            i += 1;
        }
    }
    return units;
}

std::vector<Unit> to_units(const std::vector<graphemes::Grapheme>& units) {
    std::vector<Unit> out;
    out.reserve(units.size());
    for (const auto& g : units) out.push_back({from_utf8(g.text), g.kind});
    return out;
}

std::vector<graphemes::Grapheme> from_units(const std::vector<Unit>& units) {
    std::vector<graphemes::Grapheme> out;
    out.reserve(units.size());
    for (const auto& u : units) out.push_back({to_utf8(u.text), u.kind});
    return out;
}

}  // namespace detail

namespace graphemes {

const char* kind_name(Kind kind) {
    switch (kind) {
        case Kind::Vowel:
            return "vowel";
        case Kind::Consonant:
            return "consonant";
        case Kind::Separator:
            return "separator";
    }
    return "";
}

std::vector<Grapheme> tokenize(std::string_view word) {
    return detail::from_units(detail::tokenize(detail::from_utf8(word)));
}

}  // namespace graphemes
}  // namespace silabificador
