# Parity with the Python package

This library is held at byte parity with the Python `silabificador` release named in
`kPythonReference` (`include/silabificador/silabificador.hpp`). The test `parity` checks
that claim. It recomputes every line of `tests/goldens/syllables.jsonl.gz` and compares
the bytes.

## Public surface

The Python package defines 39 public names across its nine modules, counting each
re-export once and leaving out the version constants. Each one maps to C++ as shown
below.
The rule tables are data, so they are kept internal in C++. They are reached
through the predicates and through the functions that use them. The rule-coverage
corpus exercises every entry of every table in context.

| Python | C++ |
| --- | --- |
| `silabificador.analyze` / `syl.analyze` | `silabificador::analyze` |
| `silabificador.syllabify` / `syl.syllabify` | `silabificador::syllabify` |
| `silabificador.stressed_index` / `syl.stressed_index` | `silabificador::stressed_index` |
| `Syllabifier` and its `syllabify`, `analyze`, `stressed_index` | `silabificador::Syllabifier` |
| `Syllable` and its nine fields | `silabificador::Syllable` |
| `Syllable.of` | `silabificador::syllable_of` |
| `Syllable.__str__`, `__len__` | `Syllable::str()`, `Syllable::length()` (code points) |
| `phonotactics.is_vowel`, `is_complex_onset` | `phonotactics::is_vowel`, `is_complex_onset` |
| `phonotactics.VOWELS`, `GLIDE_CAPABLE`, `STRESS_MARKED`, `NASAL_VOWELS`, `CONSONANTS`, `CONSONANT_DIGRAPHS`, `FOREIGN_DIGRAPHS`, `HIATUS_BEFORE`, `HIATUS_CODAS` | the `phonotactics::is_*` predicates of the same names |
| `phonotactics.COMPLEX_ONSETS` | `phonotactics::is_complex_onset` |
| `graphemes.Grapheme` (`text`, `kind`, `is_vowel`, `is_consonant`, `is_separator`) | `graphemes::Grapheme` |
| `Grapheme.__str__`, `__len__` | `Grapheme::str()`, `Grapheme::length()` (code points) |
| `graphemes.VOWEL`, `CONSONANT`, `SEPARATOR` | `graphemes::Kind` and `graphemes::kind_name` |
| `graphemes.SEPARATORS` | internal table in `src/graphemes.cpp` |
| `graphemes.tokenize` | `graphemes::tokenize` |
| `nucleus.Group`, `nucleus.resolve` | `nucleus::Group`, `nucleus::resolve` |
| `morphology.LATINATE_PREFIX_STEMS`, `VOWEL_FINAL_STEMS` | internal tables in `src/morphology.cpp` |
| `morphology.boundaries` | `morphology::boundaries` |
| `parser.MAX_ONSET`, `parser.parse` | internal constant; `parser::parse` |
| `stress.STRESS_ACCENTS`, `TILDE`, `SECONDARY_ACCENTS`, `OXYTONE_ENDINGS`, `PAROXYTONE_ENDINGS`, `CLITICS`, `SEPARATORS` | internal tables in `src/stress.cpp` |
| `stress.assign`, `stress.stressed_index` | `stress::assign`, `stress::stressed_index` |
| `version.VERSION_*` | `kPythonReference` |

The Python package has no command-line interface, so this library ships none.

## Unicode

`analyze` strips and lowercases its input exactly as Python's `str.strip()` and
`str.lower()` do, including the Greek final-sigma rule. The tables in
`src/unicode_tables.inc` are generated from Python itself
(`tools/gen_unicode_tables.py`). The header of that file records the Python and
Unicode versions. Input is UTF-8. Ill-formed input decodes as Python's
`bytes.decode("utf-8", "replace")` does: each maximal subpart of an ill-formed
sequence becomes one U+FFFD.

Every function that takes indices or lists from the caller follows Python's
indexing. A negative index counts from the end, slices clamp, and an index out of
range throws `std::out_of_range` where Python raises `IndexError`.

## Goldens

`tools/dump_goldens.py` runs against the pinned Python release and calls every
public function on each input. It writes the result as one compact JSON line per
input.

| Corpus | Inputs | Source |
| --- | ---: | --- |
| gold_sample | 500 | silabificador `tests/gold_sample.json` |
| tests | 302 | every string literal in silabificador `tests/` |
| lexicon | 52,543 | every distinct word of tugalex `regional_dict.csv.gz` |
| gold_tsv | 1,204 | tugaphone `tests/data/gold/*.tsv` words not already listed |
| rules | 21,313 | every rule-table entry in context: clitics in compounds, each ending, each prefix and stem with inflections, each digraph and onset, every vowel pair with each coda |
| fuzz | 25,634 | seeded random strings over letters, accents, separators, digits, whitespace and Greek |
| unicode | 6,902 | every code point with a lowercase mapping, whitespace status, or category Mn/Lt, plus the final-sigma contexts |

Each line records the syllables with all nine fields, `stressed_index`, the
grapheme units, the morpheme boundaries, and `stress.stressed_index` over the
syllable strings. The test `parity` checks this file.

A second file, `tests/goldens/modules.jsonl.gz`, calls each sub-module function
with arguments that `analyze` never produces. The test `modules` checks it.

| Call | Cases | Arguments |
| --- | ---: | --- |
| `nucleus.resolve` | 40,004 | random units, including empty and multi-letter vowel texts; start and end from -10 to 10; four fixed cases whose nucleus test wraps a negative index |
| `Syllable.of` | 30,000 | random units; a nucleus that may be empty, negative, descending or absent |
| `stress.assign` | 20,000 | random syllables with empty and separator-only surfaces; compound words |
| `stress.stressed_index` | 20,000 | random syllable strings and words, IndexError cases included |
| `morphology.boundaries` | 20,000 | real and random words against units that do not match them |
| `parser.parse`, `graphemes.tokenize` | 10,000 each | mixed-case raw strings |
| `phonotactics.is_vowel`, `is_complex_onset` | 68 each | unit texts, every complex onset, and odd strings |
| UTF-8 decoding through `graphemes.tokenize` | 95,536 | every two-byte string, plus seeded strings over the lead and continuation boundary bytes |

To regenerate, install the reference release in a fresh virtual environment, then
run:

```
python -I tools/dump_goldens.py --out tests/goldens/syllables.jsonl.gz \
    --modules-out tests/goldens/modules.jsonl.gz \
    --gold-sample <silabificador>/tests/gold_sample.json \
    --tests-dir <silabificador>/tests \
    --lexicon <site-packages>/tugalex/data/regional_dict.csv.gz \
    --gold-tsv-dir <tugaphone>/tests/data/gold
```
