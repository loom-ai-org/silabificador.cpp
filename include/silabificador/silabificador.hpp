// silabificador: rule-based Portuguese syllabification.
//
// C++ port of the Python package of the same name. Every public function takes
// and returns UTF-8; the rules work on Unicode code points internally, exactly
// as the Python code indexes `str`. Ill-formed UTF-8 decodes as Python's
// bytes.decode("utf-8", "replace"): one U+FFFD per maximal subpart.
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace silabificador {

// Number of code points the UTF-8 text decodes to: Python's len() of the str.
size_t code_points(std::string_view text);

// The Python release this port is held at byte parity with.
inline constexpr const char* kPythonReference = "2.1.7a1";

// One syllable, decomposed into onset, nucleus (with glides) and coda.
//
// `surface` is the syllable as written, kept verbatim: a separator (hyphen,
// space, apostrophe) can sit anywhere in it, so recomposing from the
// constituents would reorder the letters.
struct Syllable {
    std::string onset;
    std::string glide_on;   // the u of qu/gu: quadro, água
    std::string nucleus;
    std::string glide_off;  // the offglide of a falling diphthong: pai, saudade
    std::string coda;
    std::string separator;  // hyphen, space or apostrophe held by this syllable
    std::string surface;
    bool stressed = false;   // primary stress; exactly one syllable of a word
    bool secondary = false;  // secondary stress, only in compounds or with a grave

    const std::string& str() const { return surface; }        // Python __str__
    size_t length() const { return code_points(surface); }      // Python __len__
    bool operator==(const Syllable& other) const;
};

// Syllabify `word` into Syllable objects with stress marked.
// The input is stripped of surrounding whitespace and lowercased first.
std::vector<Syllable> analyze(std::string_view word);

// Split a word into syllables. The syllables concatenate back to the stripped,
// lowercased word: separators stay on the syllable they follow.
std::vector<std::string> syllabify(std::string_view word);

// Index of the syllable carrying the word's primary stress.
int stressed_index(std::string_view word);

// Object wrapper with the same three operations, mirroring the Python class.
class Syllabifier {
public:
    std::vector<std::string> syllabify(std::string_view word) const;
    std::vector<Syllable> analyze(std::string_view word) const;
    int stressed_index(std::string_view word) const;
};

// What Portuguese phonotactics licenses. Data only.
namespace phonotactics {
bool is_vowel(std::string_view unit);
bool is_glide_capable(std::string_view unit);
bool is_stress_marked(std::string_view unit);
bool is_nasal_vowel(std::string_view unit);
bool is_consonant(std::string_view unit);
bool is_consonant_digraph(std::string_view unit);
bool is_foreign_digraph(std::string_view unit);
bool is_foreign_tetragraph(std::string_view unit);
// Python's FOREIGN_MARKS.search(word): the word holds a spelling that marks it
// as foreign or derived from a foreign name (k, w, y, a trema, ck, sch, th, a
// final b c d g t or ch, ...).
bool has_foreign_mark(std::string_view word);
bool is_complex_onset(std::string_view cluster);
bool is_hiatus_before(std::string_view unit);
bool is_hiatus_coda(std::string_view unit);
}  // namespace phonotactics

// Layer 1: orthography to grapheme units.
namespace graphemes {
enum class Kind { Vowel, Consonant, Separator };

struct Grapheme {
    std::string text;  // one letter or a digraph
    Kind kind;

    bool is_vowel() const { return kind == Kind::Vowel; }
    bool is_consonant() const { return kind == Kind::Consonant; }
    bool is_separator() const { return kind == Kind::Separator; }
    const std::string& str() const { return text; }        // Python __str__
    size_t length() const { return code_points(text); }    // Python __len__
    bool operator==(const Grapheme& other) const {
        return text == other.text && kind == other.kind;
    }
};

const char* kind_name(Kind kind);  // "vowel", "consonant", "separator"

// Segment `word` (already lowercased) into grapheme units.
std::vector<Grapheme> tokenize(std::string_view word);
}  // namespace graphemes

// Layer 2: nucleus and glide resolution.
namespace nucleus {
using Group = std::vector<int>;  // one nucleus and its offglides, as unit indices

// Group the vowel run units[start..end] into syllable nuclei.
std::vector<Group> resolve(const std::vector<graphemes::Grapheme>& units, int start, int end);

// Regroup `groups` once stressed[k] says whether group k is stressed. `hard`
// holds the morpheme boundaries, which no group may straddle.
std::vector<Group> by_stress(const std::vector<graphemes::Grapheme>& units,
                             const std::vector<Group>& groups, const std::vector<bool>& stressed,
                             const std::set<int>& hard);
}  // namespace nucleus

// Build a syllable from its grapheme units. `nucleus` holds indices into the
// whole word, as nucleus::resolve returns them; without it the units are
// treated as unanalysable (the onset holds the whole text).
Syllable syllable_of(const std::vector<graphemes::Grapheme>& units,
                     const nucleus::Group* nucleus = nullptr);

// Morpheme boundaries, which outrank phonotactics.
namespace morphology {
// Grapheme indices that must begin a syllable, whatever phonotactics says.
std::set<int> boundaries(std::string_view word, const std::vector<graphemes::Grapheme>& units);
}  // namespace morphology

// Layer 3: syllable assembly.
namespace parser {
// Syllabify a lowercased word. Stress is not marked.
std::vector<Syllable> parse(std::string_view word);
}  // namespace parser

// Stress assignment.
namespace stress {
// Mark the primary (and in compounds, secondary) stressed syllables.
std::vector<Syllable> assign(const std::vector<Syllable>& syllables, std::string_view word);

// Index of the syllable carrying the primary stress of `word`, given its
// syllables as strings.
int stressed_index(const std::vector<std::string>& syllables, std::string_view word);
}  // namespace stress

}  // namespace silabificador
