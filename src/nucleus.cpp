// Layer 2: nucleus and glide resolution.
//
// Rules, in order of application:
//   NASAL-OFFGLIDE             a nasal nucleus takes an offglide: mãe, pão, põe
//   ACCENT                     an accented close vowel is a nucleus: pa.ís
//   CLOSED-BY-LIQUID-OR-NASAL  a close vowel closed by l m n r z is a nucleus: a.in.da
//   PRE-PALATAL                a close vowel before nh is a nucleus: ra.i.nha
//   FALLING-DIPHTHONG          otherwise an unaccented i/u after a nucleus is an
//                              offglide, and a nucleus takes at most one
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

            // Python's `glide in "eoiu"` is a substring test, true for "" too.
            if (single_in(head, phonotactics::kNasalVowels) &&
                std::u32string_view(U"eoiu").find(glide) != std::u32string_view::npos) {
                group.push_back(i + 1);  // NASAL-OFFGLIDE: mãe, pão, põe
            } else if (!single_in(glide, phonotactics::kGlideCapable) ||
                       single_in(glide, phonotactics::kStressMarked)) {
                // ACCENT / no mid or low glides: pa.ís, co.e.lho, su.a
            } else if (is_last && single_in(after, phonotactics::kHiatusCodas)) {
                // CLOSED-BY-LIQUID-OR-NASAL: a.in.da, ca.ir, ju.iz
            } else if (is_last && next_unit(units, end) == U"nh") {
                // PRE-PALATAL: ra.i.nha, mo.i.nho
            } else {
                group.push_back(i + 1);  // FALLING-DIPHTHONG: pai, sau.da.de
            }
        }

        i = group.back() + 1;
        groups.push_back(std::move(group));
    }
    return groups;
}

}  // namespace detail

namespace nucleus {
std::vector<Group> resolve(const std::vector<graphemes::Grapheme>& units, int start, int end) {
    return detail::resolve(detail::to_units(units), start, end);
}
}  // namespace nucleus

}  // namespace silabificador
