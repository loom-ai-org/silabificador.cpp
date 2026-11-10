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
           pair == U"th" || pair == U"ph" || pair == U"zh";
}

// After q or g, a u followed by another vowel spells nothing of its own (quero,
// guerra) or rides in the onset as a glide (quadro, água): qu/gu is one unit.
// The pre-1990 trema marks the same u as pronounced (agüentar, qüinqüenal); it
// is still a glide in the onset, never a nucleus (AO90 Base XX 5).
bool u_is_mute(const U32& word, size_t index) {
    if (word[index] != U'u' && word[index] != U'ü') return false;
    size_t next = index + 1;
    return next < word.size() && contains(phonotactics::kVowels, word[next]);
}

// The loanword vowel spelling that starts at `index`, or "". Each is one vowel
// in its donor language, anchored on a context no Portuguese word has: French
// eau at the end of a word or before a final s/x (bu.reau, bor.deaux), French
// eu before a final r (bo.xeur), and ou in a word with a foreign mark
// (gour.met, coun.try).
std::u32string_view foreign_vowel(const U32& word, size_t index) {
    const std::u32string_view rest = std::u32string_view(word).substr(index);
    if (starts_with(rest, U"eau")) {
        const std::u32string_view tail = rest.substr(3);
        if (tail.empty() || tail == U"s" || tail == U"x") return U"eau";
    }
    if (starts_with(rest, U"eu")) {
        const std::u32string_view tail = rest.substr(2);
        if (tail == U"r" || tail == U"rs") return U"eu";
    }
    if (starts_with(rest, U"ou") && phonotactics::has_foreign_mark(word)) return U"ou";
    return U"";
}
}  // namespace

std::vector<Unit> tokenize(const U32& word) {
    std::vector<Unit> units;
    size_t i = 0;
    const size_t length = word.size();

    while (i < length) {
        char32_t c = word[i];
        std::u32string_view pair = std::u32string_view(word).substr(i, 2);
        const std::u32string_view vowel =
            contains(phonotactics::kVowels, c) ? foreign_vowel(word, i) : U"";

        if (contains(kSeparators, c)) {
            units.push_back({U32(1, c), graphemes::Kind::Separator});
            i += 1;
        } else if (std::u32string_view(word).substr(i, 4) == U"tsch") {
            units.push_back({U32(word, i, 4), graphemes::Kind::Consonant});
            i += 4;
        } else if (!vowel.empty()) {
            units.push_back({U32(vowel), graphemes::Kind::Vowel});
            i += vowel.size();
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
