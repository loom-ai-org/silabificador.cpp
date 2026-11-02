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
bool is_foreign_digraph(std::string_view unit) { return one_of(unit, {"sh", "th", "ff", "ph"}); }
bool is_hiatus_before(std::string_view unit) { return unit == "nh"; }
bool is_hiatus_coda(std::string_view unit) { return single_in(unit, kHiatusCodas); }

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
