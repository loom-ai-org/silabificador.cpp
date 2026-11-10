// Layer 2: nucleus and glide resolution.
//
// Rules, in order of application:
//   PRE-PALATAL                a close vowel before nh is a nucleus: ra.i.nha,
//                              mã.i.nha (AO90 Base X 2), so it outranks the next rule
//   NASAL-OFFGLIDE             a nasal nucleus takes an offglide: mãe, pão, põe
//   CONTRACTION-AO             the o of the contractions ao and aos is an offglide
//                              (AO90 Base VII 1 Obs.); a hyphen or a space ends
//                              the word, an apostrophe does not
//   ACCENT                     an accented close vowel is a nucleus: pa.ís
//   IDENTICAL-VOWELS           two identical vowels are two nuclei: xi.i.ta
//                              (AO90 Base XX 4, "mesmo que sejam iguais")
//   CLOSED-BY-LIQUID-OR-NASAL  a close vowel closed by l m n r z is a nucleus: a.in.da
//   FALLING-DIPHTHONG          otherwise an unaccented i/u after a nucleus is an
//                              offglide, and a nucleus takes at most one
//
// Two more rules need the stress, so by_stress runs them on the first pass once
// the stress module has read it:
//   STRESSED-IU-UI  a stressed final iu or ui after another vowel is the diphthong,
//                   and the vowel before it stands alone: flu.iu, dis.tra.iu, pa.uis
//                   (AO90 Base X 6, Base XX 4 "flu-iu"). "Final" means final in
//                   its own word: the scan stops at a separator, so the enclitic
//                   of saiu-se does not hide the end of the verb (Base XVII 1)
//   UNSTRESSED-IU   iu outside the stressed syllable is two nuclei: mi.u.de.za
//                   (Mateus & d'Andrade 1998, pp. 18-21); ui keeps its diphthong
#include <algorithm>

#include "detail.hpp"
#include "phonotactics.hpp"

namespace silabificador {
namespace detail {

namespace {
// The consonant that would close the syllable ending at `run_end`, if any. A
// geminate (rr, ss) closes nothing: it spells one consonant split by spelling.
// Indices follow Python list semantics, so a caller's negative index wraps.
U32 closes_syllable(const std::vector<Unit>& units, int run_end) {
    const long first = run_end + 1L;
    const long n = static_cast<long>(units.size());
    if (first >= n || !py_at(units, first).is_consonant()) return U"";

    const Unit* following = first + 1 < n ? &py_at(units, first + 1) : nullptr;
    if (following != nullptr && following->text == py_at(units, first).text) return U"";

    const bool is_coda = following == nullptr || !following->is_vowel();
    return is_coda ? py_at(units, first).text : U"";
}

U32 next_unit(const std::vector<Unit>& units, int run_end) {
    const long next = run_end + 1L;
    return next < static_cast<long>(units.size()) ? py_at(units, next).text : U"";
}

bool single_in(const U32& text, std::u32string_view set) {
    return text.size() == 1 && contains(set, text[0]);
}

// The word that holds units[index], ended by a hyphen or a space. The slices
// follow Python, so a negative index counts from the end.
U32 word_at(const std::vector<Unit>& units, int index) {
    const auto ends_word = [](const Unit& unit) {
        return unit.is_separator() && !in_apostrophes(unit.text);
    };
    const size_t at = py_bound(index, units.size());
    size_t first = at;
    while (first > 0 && !ends_word(units[first - 1])) --first;
    size_t last = at;
    while (last < units.size() && !ends_word(units[last])) ++last;
    U32 word;
    for (size_t k = first; k < last; ++k) word += units[k].text;
    return word;
}
}  // namespace

std::vector<nucleus::Group> resolve(const std::vector<Unit>& units, int start, int end) {
    std::vector<nucleus::Group> groups;
    int i = start;

    while (i <= end) {
        nucleus::Group group{i};
        const U32& head = py_at(units, i).text;

        if (i < end) {
            const U32& glide = py_at(units, i + 1L).text;
            const bool is_last = (i + 1) == end;
            const U32 after = is_last ? closes_syllable(units, end) : U"";

            if (is_last && single_in(glide, phonotactics::kGlideCapable) &&
                next_unit(units, end) == U"nh") {
                // PRE-PALATAL: ra.i.nha, mo.i.nho, mã.i.nha
            } else if (single_in(head, phonotactics::kNasalVowels) &&
                       // Python's `glide in "eoiu"` is a substring test, true for "" too.
                       std::u32string_view(U"eoiu").find(glide) != std::u32string_view::npos) {
                group.push_back(i + 1);  // NASAL-OFFGLIDE: mãe, pão, põe
            } else if (head + glide == U"ao" &&
                       phonotactics::is_ao_diphthong_word(to_utf8(word_at(units, i)))) {
                group.push_back(i + 1);  // CONTRACTION-AO: ao, aos
            } else if (!single_in(glide, phonotactics::kGlideCapable) ||
                       single_in(glide, phonotactics::kStressMarked)) {
                // ACCENT / no mid or low glides: pa.ís, co.e.lho, su.a
            } else if (glide == head) {
                // IDENTICAL-VOWELS: xi.i.ta, va.di.i.ce
            } else if (is_last && single_in(after, phonotactics::kHiatusCodas)) {
                // CLOSED-BY-LIQUID-OR-NASAL: a.in.da, ca.ir, ju.iz
            } else {
                group.push_back(i + 1);  // FALLING-DIPHTHONG: pai, sau.da.de
            }
        }

        i = group.back() + 1;
        groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<nucleus::Group> by_stress(const std::vector<Unit>& units,
                                      const std::vector<nucleus::Group>& groups,
                                      const std::vector<bool>& stressed, const std::set<int>& hard) {
    const auto is_close = [](const U32& text) { return text == U"i" || text == U"u"; };
    std::vector<nucleus::Group> regrouped;
    const size_t count = std::min(groups.size(), stressed.size());  // Python zip()
    for (size_t k = 0; k < count; ++k) {
        const nucleus::Group& group = groups[k];
        const bool is_stressed = stressed[k];
        const int head = py_at(group, 0);
        const bool has_previous = !regrouped.empty();
        const U32 glide = head != 0 ? py_at(units, head - 1L).text : U"";

        // Every unit after the head, up to a separator, is an s.
        const auto only_s_after = [&] {
            for (size_t i = py_bound(head + 1L, units.size());
                 i < units.size() && !units[i].is_separator(); ++i) {
                if (units[i].text != U"s") return false;
            }
            return true;
        };
        if (is_stressed && group.size() == 1 && is_close(py_at(units, head).text) && has_previous &&
            py_at(regrouped.back(), -1) == head - 1 && is_close(glide) &&
            glide != py_at(units, head).text && hard.count(head) == 0 && only_s_after()) {
            // STRESSED-IU-UI: flu.iu, dis.tra.iu, pa.uis
            if (regrouped.back().size() > 1) {
                regrouped.back().pop_back();
            } else {
                regrouped.pop_back();
            }
            regrouped.push_back({head - 1, head});
        } else if (!is_stressed && group.size() == 2 && py_at(units, head).text == U"i" &&
                   py_at(units, group[1]).text == U"u") {
            regrouped.push_back({head});  // UNSTRESSED-IU: mi.u.de.za
            regrouped.push_back({group[1]});
        } else {
            regrouped.push_back(group);
        }
    }
    return regrouped;
}

}  // namespace detail

namespace nucleus {
std::vector<Group> resolve(const std::vector<graphemes::Grapheme>& units, int start, int end) {
    return detail::resolve(detail::to_units(units), start, end);
}

std::vector<Group> by_stress(const std::vector<graphemes::Grapheme>& units,
                             const std::vector<Group>& groups, const std::vector<bool>& stressed,
                             const std::set<int>& hard) {
    return detail::by_stress(detail::to_units(units), groups, stressed, hard);
}
}  // namespace nucleus

}  // namespace silabificador
