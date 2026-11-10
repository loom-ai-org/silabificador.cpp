// Stress assignment.
//
// Portuguese spelling encodes stress, so it is read, not guessed:
//   1. ACCENT  a syllable with an acute or circumflex is stressed: sá.bi.a
//   2. TILDE   failing that, a nasal ã/õ carries it: ir.mã, co.ra.ção
//   3. ENDING  failing both, the ending chooses the last or the penultimate
//              syllable; an unaccented word cannot be stressed further back.
//
// In a hyphenated compound each element keeps its own stress; the last carries
// the primary and earlier ones the secondary (guàr.da-CHU.va). Unstressed
// monosyllables (articles, prepositions, conjunctions, clitic pronouns) are not
// stress domains. A pre-1990 grave (sòmente) marks a secondary stress outright.
//
// A pronoun joined by a hyphen to the verb before it is an enclitic, not the
// last word of a compound (AO90 Base XVII 1: amá-lo, dá-se, deixa-o). It has no
// stress, so the verb keeps the primary: ven.DEU-.se, DEU-.te-.lo. In tmesis the
// verb ending comes last and keeps it: (dar-).te-.EI.
//
// References: Cunha & Cintra (1984), "Acentuação", and "Monossílabos átonos"
// (pp. 56-57, as quoted in Ciberdúvidas answer 35988); Acordo Ortográfico (1990),
// Bases VIII-XI, Base IX 9.º (pelo(s), pela(s) are proclitic), Base XIII 1.º and
// Base XVII 1.º (the hyphen of enclisis and tmesis).
#include <array>

#include "detail.hpp"

namespace silabificador {
namespace detail {

namespace {
constexpr std::u32string_view kStressAccents = U"áéíóúâêôîû";
constexpr std::u32string_view kTilde = U"ãõ";
// The pre-1990 grave; à is excluded because it is crasis and unstressed.
constexpr std::u32string_view kSecondaryAccents = U"èìòù";
// Characters that join words without belonging to one.
constexpr std::u32string_view kSeparators = U"- '’";
// The separators that end a word. An apostrophe does not: it marks an elided
// vowel (AO90 Base XVIII), so pau-d'alho has two elements, pau and dalho.
constexpr std::u32string_view kWordBoundaries = U"- ";

constexpr std::array<std::u32string_view, 18> kOxytoneEndings{
    U"r", U"l", U"z", U"x", U"n", U"ps", U"i", U"is", U"u", U"us",
    U"im", U"ins", U"um", U"uns", U"om", U"ons", U"y", U"w",
};
// Inflections do not move the stress: fa.lam, jo.vens.
constexpr std::array<std::u32string_view, 6> kParoxytoneEndings{
    U"as", U"es", U"os", U"am", U"em", U"ens",
};

// The clitic pronouns: Cunha & Cintra's "me, te, se, o, a, lhe, nos, vos, os,
// as, lhes e suas combinações: mo, to, lho"; lo(s), la(s) (AO90 Base VIII 1 b);
// no, na, nos, nas after a nasal (fazem-no); and the vo of vo-lo.
constexpr std::array<std::u32string_view, 31> kCliticPronouns{
    U"me", U"te", U"se", U"lhe", U"nos", U"vos", U"lhes", U"o", U"a", U"os", U"as",
    U"mo", U"ma", U"mos", U"mas", U"to", U"ta", U"tos", U"tas", U"lho", U"lha", U"lhos", U"lhas",
    U"lo", U"la", U"los", U"las", U"no", U"na", U"nas", U"vo",
};

// The monossílabos átonos: a closed class of function words with no stress,
// the clitic pronouns above included. pelo(s) and pela(s) are two syllables
// and AO90 Base IX 9.º names them proclitic. dom, são, seu and sob are left
// out: the stressed reading is common, or a prefix would lose its stress.
constexpr std::array<std::u32string_view, 41> kClitics{
    U"a", U"as", U"o", U"os", U"um", U"uns", U"uma", U"umas",
    U"de", U"do", U"da", U"dos", U"das", U"d", U"em", U"no", U"na",
    U"nos", U"nas", U"por", U"pelo", U"pela", U"com", U"sem",
    U"à", U"às", U"ao", U"aos", U"dum", U"duns", U"num", U"nuns", U"pelos", U"pelas",
    U"e", U"ou", U"que", U"se", U"mas", U"nem",
    U"frei",
};

template <size_t N>
bool one_of(std::u32string_view element, const std::array<std::u32string_view, N>& words) {
    for (auto word : words) {
        if (word == element) return true;
    }
    return false;
}

bool is_pronoun(std::u32string_view element) { return one_of(element, kCliticPronouns); }
bool is_clitic(std::u32string_view element) { return is_pronoun(element) || one_of(element, kClitics); }

template <size_t N>
bool ends_with_any(std::u32string_view word, const std::array<std::u32string_view, N>& endings) {
    for (auto e : endings) {
        if (ends_with(word, e)) return true;
    }
    return false;
}

bool has_any(std::u32string_view text, std::u32string_view set) {
    for (char32_t c : text) {
        if (contains(set, c)) return true;
    }
    return false;
}

// Python's `s.rstrip(APOSTROPHES)[-1:] in "- "`: a substring test, so an empty
// remainder matches. An apostrophe after the hyphen does not change the end of
// the element: bê-.á-'.bá.
bool ends_element(std::u32string_view syllable) {
    size_t end = syllable.size();
    while (end > 0 && contains(kApostrophes, syllable[end - 1])) --end;
    if (end == 0) return true;
    return contains(kWordBoundaries, syllable[end - 1]) || contains(syllable, U' ');
}

U32 rstrip_separators(std::u32string_view text) {
    size_t end = text.size();
    while (end > 0 && contains(kSeparators, text[end - 1])) --end;
    return U32(text.substr(0, end));
}

// The words inside a compound, in order: guarda-chuva is two, chapéu de chuva
// three. An element is spelled without its apostrophes, so d'os is dos.
std::vector<U32> elements(const U32& word) {
    std::vector<U32> parts;
    U32 current;
    for (char32_t c : word) {
        if (contains(kWordBoundaries, c)) {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else if (!contains(kApostrophes, c)) {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

std::vector<std::vector<int>> element_groups(const std::vector<U32>& syllables) {
    std::vector<std::vector<int>> groups(1);
    for (size_t index = 0; index < syllables.size(); ++index) {
        groups.back().push_back(static_cast<int>(index));
        if (ends_element(syllables[index])) groups.emplace_back();
    }
    std::vector<std::vector<int>> nonempty;
    for (auto& g : groups) {
        if (!g.empty()) nonempty.push_back(std::move(g));
    }
    return nonempty;
}

// Position of the element that carries the primary stress: the last one,
// unless it is a pronoun hyphenated to the word before it. An enclitic has no
// stress, so the primary falls on its host, and a chain of them is skipped
// whole (deu-te-lo). A form made only of pronouns (no-lo) keeps it on the last.
size_t primary_element(const std::vector<U32>& syllables, const std::vector<std::vector<int>>& groups,
                       const std::vector<U32>& words) {
    long host = static_cast<long>(words.size()) - 1;
    while (host != 0 && is_pronoun(py_at(words, host)) &&
           ends_with(syllables[groups[host - 1].back()], U"-")) {
        --host;
    }
    return is_pronoun(py_at(words, host)) ? words.size() - 1 : static_cast<size_t>(host);
}

// Index of the stressed syllable of a single word, not a compound.
int stressed_in(const std::vector<U32>& syllables, const U32& word) {
    const int n = static_cast<int>(syllables.size());
    if (n <= 1) return 0;

    for (int index = n - 1; index >= 0; --index) {
        if (has_any(syllables[index], kStressAccents)) return index;  // 1. ACCENT
    }
    for (int index = n - 1; index >= 0; --index) {
        if (has_any(syllables[index], kTilde)) return index;  // 2. TILDE
    }
    if (ends_with_any(word, kParoxytoneEndings)) return n - 2;  // 3. ENDING
    if (ends_with_any(word, kOxytoneEndings)) return n - 1;
    return n - 2;
}
}  // namespace

std::vector<Syllable> assign(const std::vector<Syllable>& syllables, const U32& word) {
    if (syllables.empty()) return syllables;

    std::vector<U32> surfaces;
    for (const auto& s : syllables) surfaces.push_back(from_utf8(s.surface));

    auto groups = element_groups(surfaces);
    auto words = elements(word);
    if (words.size() != groups.size()) {
        // The grouping and the spelling disagree: treat the whole as one word.
        std::vector<int> all;
        U32 joined;
        for (size_t i = 0; i < surfaces.size(); ++i) {
            all.push_back(static_cast<int>(i));
            joined += surfaces[i];
        }
        groups = {all};
        words = {joined};
    }

    std::set<int> primary, secondary;
    const size_t host = primary_element(surfaces, groups, words);
    for (size_t position = 0; position < groups.size(); ++position) {
        const auto& group = groups[position];
        const U32& element = words[position];
        if (position != host && (element.empty() || is_clitic(element))) continue;
        std::vector<U32> local;
        for (int i : group) local.push_back(rstrip_separators(surfaces[i]));
        const int target = group[stressed_in(local, element)];
        (position == host ? primary : secondary).insert(target);
    }

    // The pre-1990 grave states a secondary stress outright, wherever it falls.
    for (size_t index = 0; index < surfaces.size(); ++index) {
        if (has_any(surfaces[index], kSecondaryAccents) && !primary.count(static_cast<int>(index))) {
            secondary.insert(static_cast<int>(index));
        }
    }

    std::vector<Syllable> out = syllables;
    for (size_t index = 0; index < out.size(); ++index) {
        out[index].stressed = primary.count(static_cast<int>(index)) > 0;
        out[index].secondary = secondary.count(static_cast<int>(index)) > 0;
    }
    return out;
}

int stressed_index(const std::vector<U32>& syllables, const U32& word) {
    auto groups = element_groups(syllables);
    auto words = elements(word);
    if (words.size() != groups.size()) {
        std::vector<U32> stripped;
        for (const auto& s : syllables) stripped.push_back(rstrip_separators(s));
        return stressed_in(stripped, word);
    }
    // With no syllables and no words, primary_element raises as Python's
    // words[-1] does.
    const size_t host = primary_element(syllables, groups, words);
    const auto& group = groups[host];
    std::vector<U32> local;
    for (int i : group) local.push_back(rstrip_separators(syllables[i]));
    return group[stressed_in(local, words[host])];
}

}  // namespace detail

namespace stress {
std::vector<Syllable> assign(const std::vector<Syllable>& syllables, std::string_view word) {
    return detail::assign(syllables, detail::from_utf8(word));
}

int stressed_index(const std::vector<std::string>& syllables, std::string_view word) {
    std::vector<detail::U32> converted;
    for (const auto& s : syllables) converted.push_back(detail::from_utf8(s));
    return detail::stressed_index(converted, detail::from_utf8(word));
}
}  // namespace stress

}  // namespace silabificador
