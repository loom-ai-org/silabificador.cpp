// Morpheme boundaries, which outrank phonotactics, and which are lexical.
//
// The dictionaries write sub.li.mi.nar but su.bli.me, dis.tri.bu.i.dor but
// cui.da.do: same shapes, opposite answers. What separates them is whether the
// prefix or stem is still a live morpheme, a fact about the word's history that
// no string rule can see. Entries are morphemes, never syllabifications; each
// generalizes to its derivatives and is a falsifiable etymological claim.
//
// References: Cunha & Cintra (1984), "Prefixos"; Rio-Torto (2016), Gramática
// Derivacional do Português.
#include <array>
#include <utility>

#include "detail.hpp"

namespace silabificador {
namespace detail {

namespace {
// Consonant-final Latinate prefixes, with the Latin stems that keep them live.
// Only a boundary before a liquid is recorded: there a consonant-final prefix
// would otherwise form a false complex onset (su.bli.mi.nar). Ordered longest
// prefix first, the order the Python code tries them in.
struct PrefixStems {
    std::u32string_view prefix;
    std::initializer_list<std::u32string_view> stems;
};

const std::array<PrefixStems, 4>& latinate_prefix_stems() {
    static const std::array<PrefixStems, 4> table{{
        {U"sub", {U"limin", U"loc", U"lig", U"lunar", U"lacustre", U"lamelar", U"lenhoso",
                  U"licenc", U"licenç"}},
        {U"ab", {U"leg", U"loc", U"rog"}},
        {U"ad", {U"leg", U"lig"}},
        {U"ob", {U"liqu", U"rog"}},
    }};
    return table;
}

// Verb stems ending in a vowel before a suffix opening with i: the seam is a
// hiatus however much it looks like a diphthong (dis.tri.bu.i.dor,
// des.tru.i.ção, a.ju.i.zar). Matched anywhere, so prefixed derivatives are
// covered. Short stems that would seize unrelated words (us, flu) are excluded.
constexpr std::array<std::u32string_view, 26> kVowelFinalStems{
    // -uir verbs (Latin -uere) and everything derived from them.
    U"distribu", U"contribu", U"retribu", U"atribu", U"destru", U"constru", U"instru",
    U"substitu", U"constitu", U"restitu", U"institu", U"prostitu",
    U"polu", U"dilu", U"inclu", U"conclu", U"exclu", U"diminu", U"imbu", U"possu",
    U"continu",
    // -air verbs (Latin -ahere) and their nominalizations.
    U"abstra", U"distra", U"subtra", U"retra",
    // juízo, juiz and their family; safe only because the seam needs a following i.
    U"ju",
};

size_t prefix_boundary(const U32& word) {
    for (const auto& entry : latinate_prefix_stems()) {
        if (!starts_with(word, entry.prefix)) continue;
        std::u32string_view stem = std::u32string_view(word).substr(entry.prefix.size());
        if (!stem.empty() && (stem[0] == U'l' || stem[0] == U'r')) {
            for (auto s : entry.stems) {
                if (starts_with(stem, s)) return entry.prefix.size();
            }
        }
        return 0;
    }
    return 0;
}

size_t seam_boundary(const U32& word) {
    for (auto stem : kVowelFinalStems) {
        size_t start = word.find(stem);
        if (start == U32::npos) continue;
        size_t end = start + stem.size();
        if (end < word.size() && word[end] == U'i' && word.size() > end + 1) return end;
    }
    return 0;
}
}  // namespace

std::set<int> boundaries(const U32& word, const std::vector<Unit>& units) {
    std::set<size_t> offsets;
    for (size_t offset : {prefix_boundary(word), seam_boundary(word)}) {
        if (offset != 0) offsets.insert(offset);
    }
    std::set<int> found;
    if (offsets.empty()) return found;

    size_t position = 0;
    for (size_t index = 0; index < units.size(); ++index) {
        if (offsets.count(position)) found.insert(static_cast<int>(index));
        position += units[index].text.size();
    }
    return found;
}

}  // namespace detail

namespace morphology {
std::set<int> boundaries(std::string_view word, const std::vector<graphemes::Grapheme>& units) {
    return detail::boundaries(detail::from_utf8(word), detail::to_units(units));
}
}  // namespace morphology

}  // namespace silabificador
