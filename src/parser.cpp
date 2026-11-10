// Layer 3: syllable assembly.
//
// Layer 2 has fixed how many syllables there are; what remains is where each
// consonant goes. One principle decides it: onset maximization, bounded by the
// onset inventory. A cluster between two nuclei gives the following onset as
// much of its right edge as Portuguese licenses; the rest stays in the coda
// (a.tle.ta, pac.to, car.ro, abs.tra.ir). Morpheme boundaries and separators
// outrank it.

#include "detail.hpp"
#include "phonotactics.hpp"

namespace silabificador {
namespace detail {

namespace {
// Portuguese onsets hold at most two units, an obstruent and a liquid.
constexpr size_t kMaxOnset = 2;

size_t onset_size(const std::vector<const Unit*>& cluster) {
    if (cluster.empty()) return 0;
    // A single consonant between nuclei is always an onset: ca.sa.
    if (cluster.size() == 1) return 1;
    U32 tail = cluster[cluster.size() - 2]->text + cluster.back()->text;
    return phonotactics::is_complex_onset(tail) ? kMaxOnset : 1;
}

std::vector<std::vector<int>> split_runs(const std::vector<Unit>& units) {
    std::vector<std::vector<int>> runs;
    std::vector<int> current;
    for (size_t index = 0; index < units.size(); ++index) {
        if (units[index].is_vowel()) {
            current.push_back(static_cast<int>(index));
        } else if (!current.empty()) {
            runs.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) runs.push_back(std::move(current));
    return runs;
}

// Break a nucleus group wherever a morpheme boundary falls inside it.
std::vector<nucleus::Group> split_group(const nucleus::Group& group, const std::set<int>& hard) {
    std::vector<nucleus::Group> pieces;
    nucleus::Group current{group.front()};
    for (size_t k = 1; k < group.size(); ++k) {
        if (hard.count(group[k])) {
            pieces.push_back(std::move(current));
            current = {group[k]};
        } else {
            current.push_back(group[k]);
        }
    }
    pieces.push_back(std::move(current));
    return pieces;
}

// Place every consonant around the nuclei in `groups`.
std::vector<Syllable> assemble(const std::vector<Unit>& units, const std::vector<nucleus::Group>& groups,
                               const std::set<int>& hard) {
    const int last_unit = static_cast<int>(units.size()) - 1;
    std::vector<Syllable> syllables;
    int start = 0;  // first unit not yet placed
    for (size_t position = 0; position < groups.size(); ++position) {
        const nucleus::Group& group = groups[position];
        int end;
        if (position + 1 == groups.size()) {
            end = last_unit;  // the last syllable takes every remaining coda
        } else {
            const int next = groups[position + 1].front();
            std::vector<int> separators;
            std::vector<const Unit*> cluster;
            std::vector<int> cluster_index;
            for (int i = group.back() + 1; i < next; ++i) {
                // An apostrophe is not a boundary: it marks an elided vowel (AO90
                // Base XVIII 1), so the consonant before it is the onset of the
                // next vowel: pau-.d'a.lho, san.t'a.na.
                if (units[i].is_separator() && !in_apostrophes(units[i].text)) separators.push_back(i);
                if (units[i].is_consonant()) {
                    cluster.push_back(&units[i]);
                    cluster_index.push_back(i);
                }
            }
            int crossing = -1;
            for (int i : hard) {
                if (group.back() < i && i <= next) {
                    crossing = i;  // std::set iterates ascending: this is the minimum
                    break;
                }
            }

            if (crossing >= 0) {
                // A morpheme boundary outranks onset maximization: sub.li.mi.nar.
                end = crossing - 1;
            } else if (!separators.empty()) {
                // A hyphen or a space is a hard boundary: ab-.re.a.gir, not a.b-re.a.gir.
                end = separators.back();
            } else if (cluster.empty()) {
                end = group.back();
            } else {
                const size_t onset = onset_size(cluster);
                end = onset < cluster.size() ? cluster_index[cluster.size() - onset - 1] : group.back();
            }
        }

        // A hyphen, space or apostrophe belongs to the syllable it follows:
        // guar.da-.chu.va.
        while (end + 1 <= last_unit && units[end + 1].is_separator()) ++end;

        std::vector<Unit> slice(units.begin() + start, units.begin() + end + 1);
        syllables.push_back(syllable_of(slice, &group));
        start = end + 1;
    }
    return syllables;
}
}  // namespace

Syllable syllable_of(const std::vector<Unit>& units, const nucleus::Group* nucleus) {
    U32 text;
    for (const auto& unit : units) text += unit.text;
    Syllable syllable;
    if (nucleus == nullptr) {
        syllable.onset = to_utf8(text);
        syllable.surface = syllable.onset;
        return syllable;
    }

    // `nucleus` holds indices into the word; rebase them onto `units`. Indexing
    // and slicing follow Python, so a caller's negative or descending nucleus
    // wraps exactly as it does there.
    const long base = py_at(*nucleus, 0);
    long offset = 0;
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i].is_vowel()) {
            offset = static_cast<long>(i);
            break;
        }
    }
    std::vector<long> vowels;
    for (int index : *nucleus) vowels.push_back(offset + (index - base));

    const long head = vowels.front();
    const long tail = vowels.back();
    const size_t onset_end = py_bound(head, units.size());
    const size_t coda_start = py_bound(tail + 1, units.size());

    // The onset leaves out an apostrophe: the d of d'á is the onset.
    U32 onset, coda, glide_off, separator;
    const U32* last_onset = nullptr;
    for (size_t i = 0; i < onset_end; ++i) {
        if (in_apostrophes(units[i].text)) continue;
        onset += units[i].text;
        last_onset = &units[i].text;
    }
    for (size_t i = coda_start; i < units.size(); ++i) {
        if (!units[i].is_separator()) coda += units[i].text;
    }

    // The glide of qu/gu is spelled inside the onset consonant itself.
    if (last_onset != nullptr && (*last_onset == U"qu" || *last_onset == U"gu")) syllable.glide_on = "u";

    const U32& nucleus_text = py_at(units, head).text;
    for (size_t k = 1; k < vowels.size(); ++k) glide_off += py_at(units, vowels[k]).text;
    for (const auto& unit : units) {
        if (unit.is_separator()) separator += unit.text;
    }

    syllable.onset = to_utf8(onset);
    syllable.nucleus = to_utf8(nucleus_text);
    syllable.glide_off = to_utf8(glide_off);
    syllable.coda = to_utf8(coda);
    syllable.separator = to_utf8(separator);
    syllable.surface = to_utf8(text);
    return syllable;
}

std::vector<Syllable> parse(const U32& word) {
    const std::vector<Unit> units = tokenize(word);
    if (units.empty()) return {};

    // Indices that a morpheme boundary forces to begin a syllable.
    const std::set<int> hard = boundaries(word, units);

    std::vector<nucleus::Group> groups;
    for (const auto& run : split_runs(units)) {
        for (auto& group : resolve(units, run.front(), run.back())) groups.push_back(std::move(group));
    }
    if (!hard.empty()) {
        // A nucleus may not straddle a seam: the ui of distribuidor is two
        // nuclei because the boundary falls between them.
        std::vector<nucleus::Group> pieces;
        for (const auto& group : groups) {
            for (auto& piece : split_group(group, hard)) pieces.push_back(std::move(piece));
        }
        groups = std::move(pieces);
    }

    if (groups.empty()) {
        // No nucleus at all (an initialism, a stray consonant): nothing to find.
        return {syllable_of(units, nullptr)};
    }

    std::vector<Syllable> syllables = assemble(units, groups, hard);
    // The stress rules read the first pass, and the nucleus rules that depend on
    // stress regroup it. The regrouping never moves the stress: it only touches
    // a syllable that is not stressed, or a final one that stays stressed.
    std::vector<bool> stressed;
    for (const auto& syllable : assign(syllables, word)) stressed.push_back(syllable.stressed || syllable.secondary);
    const std::vector<nucleus::Group> regrouped = by_stress(units, groups, stressed, hard);
    if (regrouped == groups) return syllables;
    return assemble(units, regrouped, hard);
}

}  // namespace detail

Syllable syllable_of(const std::vector<graphemes::Grapheme>& units, const nucleus::Group* nucleus) {
    return detail::syllable_of(detail::to_units(units), nucleus);
}

namespace parser {
std::vector<Syllable> parse(std::string_view word) {
    return detail::parse(detail::from_utf8(word));
}
}  // namespace parser

}  // namespace silabificador
