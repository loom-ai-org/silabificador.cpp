// Portuguese phonotactic inventories. Data only: what Portuguese permits.
//
// References: Mateus & d'Andrade (2000), The Phonology of Portuguese, ch. 3;
// Cunha & Cintra (1984), "Divisão silábica"; Acordo Ortográfico (1990), Base XX.
#include "phonotactics.hpp"

#include <array>

#include "detail.hpp"

namespace silabificador::phonotactics {

namespace {
bool single_in(std::string_view unit, std::u32string_view set) {
    detail::U32 u = detail::from_utf8(unit);
    return u.size() == 1 && detail::contains(set, u[0]);
}

bool one_of(std::string_view unit, std::initializer_list<std::string_view> items) {
    for (auto item : items) {
        if (unit == item) return true;
    }
    return false;
}
}  // namespace

bool is_vowel(std::string_view unit) { return single_in(unit, kVowels); }
bool is_glide_capable(std::string_view unit) { return single_in(unit, kGlideCapable); }
bool is_stress_marked(std::string_view unit) { return single_in(unit, kStressMarked); }
bool is_nasal_vowel(std::string_view unit) { return single_in(unit, kNasalVowels); }
bool is_consonant(std::string_view unit) { return single_in(unit, kConsonants); }
bool is_consonant_digraph(std::string_view unit) { return one_of(unit, {"ch", "lh", "nh"}); }
// Loanword spellings that behave as single consonants: show, thriller, photo,
// and zh (pinyin zh, the romanization of Russian ж): gui.zhou, kru.zha.nov. A
// doubled ff is not one of them: a doubled consonant splits like rr and ss,
// graf.fi.ti (Portal da Língua Portuguesa; Cunha & Cintra, p. 83).
bool is_foreign_digraph(std::string_view unit) { return one_of(unit, {"sh", "th", "ph", "zh"}); }
// German tsch, one affricate [tʃ]: ku.bi.tschek, pu.tschis.ta.
bool is_foreign_tetragraph(std::string_view unit) { return unit == "tsch"; }
bool is_hiatus_before(std::string_view unit) { return unit == "nh"; }
bool is_hiatus_coda(std::string_view unit) { return single_in(unit, kHiatusCodas); }

bool has_foreign_mark(std::string_view word) { return has_foreign_mark(detail::from_utf8(word)); }

// AO90 Base I 2-5: k, w, y and the spellings "não peculiares à nossa escrita"
// survive only in foreign names and their derivatives (comtista, garrettiano,
// mülleriano, shakespeariano, Moloch, Jacob, David). This is the search of the
// Python pattern [kwyäöüë]|ck|sch|sh|th|ph|tz|eau|[bcdgt]$|ch$, where $ matches
// at the end of the word or before a newline that ends it.
bool has_foreign_mark(std::u32string_view word) {
    const size_t n = word.size();
    const auto at_end = [&](size_t p) { return p == n || (p + 1 == n && word[p] == U'\n'); };
    for (size_t p = 0; p < n; ++p) {
        const std::u32string_view rest = word.substr(p);
        if (detail::contains(U"kwyäöüë", word[p])) return true;
        for (std::u32string_view spelling : {U"ck", U"sch", U"sh", U"th", U"ph", U"tz", U"eau"}) {
            if (detail::starts_with(rest, spelling)) return true;
        }
        if (detail::contains(U"bcdgt", word[p]) && at_end(p + 1)) return true;
        if (detail::starts_with(rest, U"ch") && at_end(p + 2)) return true;
    }
    return false;
}

bool is_complex_onset(std::string_view cluster) {
    detail::U32 c = detail::from_utf8(cluster);
    return is_complex_onset(c);
}

bool is_complex_onset(std::u32string_view cluster) {
    // Obstruent + liquid, and nothing else: the sonority sequencing principle.
    return cluster.size() == 2 && detail::contains(kObstruents, cluster[0]) &&
           detail::contains(kLiquids, cluster[1]);
}

}  // namespace silabificador::phonotactics
