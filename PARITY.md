# Parity with the Python package

This library is held at byte parity with the Python `silabificador` release named in
`kPythonReference` (`include/silabificador/silabificador.hpp`). The test `parity` checks
that claim. It recomputes every line of `tests/goldens/syllables.jsonl.gz` and compares
the bytes.

## Public surface

The Python package 2.1.7a1 defines 43 public names across its nine modules, counting
each re-export once and leaving out the version constants. All 43 are ported: 43 public
symbols, 43 ported. Each one maps to C++ as shown below.
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
| `phonotactics.VOWELS`, `GLIDE_CAPABLE`, `STRESS_MARKED`, `NASAL_VOWELS`, `CONSONANTS`, `CONSONANT_DIGRAPHS`, `FOREIGN_DIGRAPHS`, `FOREIGN_TETRAGRAPHS`, `HIATUS_BEFORE`, `HIATUS_CODAS` | the `phonotactics::is_*` predicates of the same names |
| `phonotactics.FOREIGN_MARKS` (a compiled pattern) | `phonotactics::has_foreign_mark`, which answers `FOREIGN_MARKS.search(word)` |
| `phonotactics.COMPLEX_ONSETS` | `phonotactics::is_complex_onset` |
| `graphemes.Grapheme` (`text`, `kind`, `is_vowel`, `is_consonant`, `is_separator`) | `graphemes::Grapheme` |
| `Grapheme.__str__`, `__len__` | `Grapheme::str()`, `Grapheme::length()` (code points) |
| `graphemes.VOWEL`, `CONSONANT`, `SEPARATOR` | `graphemes::Kind` and `graphemes::kind_name` |
| `graphemes.SEPARATORS` | internal table in `src/graphemes.cpp` |
| `graphemes.tokenize` | `graphemes::tokenize` |
| `nucleus.Group`, `nucleus.resolve`, `nucleus.by_stress` | `nucleus::Group`, `nucleus::resolve`, `nucleus::by_stress` |
| `morphology.LATINATE_PREFIX_STEMS`, `VOWEL_FINAL_STEMS` | internal tables in `src/morphology.cpp` |
| `morphology.LISTED_STEMS` | internal table in `src/morphology.cpp`, read from `data/stems.csv` |
| `morphology.boundaries` | `morphology::boundaries` |
| `parser.MAX_ONSET`, `parser.parse` | internal constant; `parser::parse` |
| `stress.STRESS_ACCENTS`, `TILDE`, `SECONDARY_ACCENTS`, `OXYTONE_ENDINGS`, `PAROXYTONE_ENDINGS`, `CLITICS`, `SEPARATORS` | internal tables in `src/stress.cpp` |
| `stress.assign`, `stress.stressed_index` | `stress::assign`, `stress::stressed_index` |
| `version.VERSION_*` | `kPythonReference` |

The Python package has no command-line interface, so this library ships none.

## Data

`data/stems.csv` is the Python package's `silabificador/data/stems.csv`, copied byte for
byte from the 2.1.7a1 wheel. CMake embeds it in the library when the project is
configured, so an installed library reads no file at run time. The library parses it as
Python's `csv.DictReader` does and reads the `stem` and `split` columns. To change the
stems, replace the file with the one from the new Python release and configure again.

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
| tests | 377 | every string literal in silabificador `tests/` |
| edge_cases | 111 | every word of silabificador `tests/edge_cases.json` not already listed |
| lexicon | 52,654 | every distinct word of tugalex 2.3.0a1 `regional_dict.csv.gz` not already listed |
| gold_tsv | 1,200 | tugaphone `tests/data/gold/*.tsv` words not already listed |
| rules | 31,572 | every rule-table entry in context: clitics in compounds, each ending, each prefix and stem with inflections, each listed stem inside derivatives, each digraph, tetragraph and onset, every vowel pair with each coda, the loanword vowels with each foreign mark, the trema, the -idade nouns, and three-vowel runs ending in iu or ui |
| fuzz | 25,624 | seeded random strings over letters, accents, separators, digits, whitespace and Greek |
| unicode | 6,902 | every code point with a lowercase mapping, whitespace status, or category Mn/Lt, plus the final-sigma contexts |

An input that two corpora hold is written once, under the first. The header names the
release, the Python and Unicode versions, the line count, and the lines each corpus owns.
The test `parity` reports its mismatches per corpus, and it fails when the header names
a release other than `kPythonReference`, or when the file holds fewer or more lines than
the header states.

Each line records the syllables with all nine fields, `stressed_index`, the
grapheme units, the morpheme boundaries, and `stress.stressed_index` over the
syllable strings. The test `parity` checks this file.

A second file, `tests/goldens/modules.jsonl.gz`, calls each sub-module function
with arguments that `analyze` never produces. The test `modules` checks it.

| Call | Cases | Arguments |
| --- | ---: | --- |
| `nucleus.resolve` | 40,004 | random units, including empty and multi-letter vowel texts; start and end from -10 to 10; four fixed cases whose nucleus test wraps a negative index |
| `nucleus.by_stress` | 30,084 | random units, groups, stress flags and boundaries, IndexError cases included; the first-pass groups of 28 words with three stress patterns each |
| `Syllable.of` | 30,000 | random units; a nucleus that may be empty, negative, descending or absent |
| `stress.assign` | 20,000 | random syllables with empty and separator-only surfaces; compound words |
| `stress.stressed_index` | 20,000 | random syllable strings and words, IndexError cases included |
| `morphology.boundaries` | 40,152 | real and random words against units that do not match them; every listed stem and its derivatives; -idade nouns; words that end in a newline |
| `parser.parse`, `graphemes.tokenize` | 30,152 each | mixed-case raw strings; strings over the loanword letters and the trema; words that end in a newline |
| `phonotactics.is_vowel`, `is_complex_onset` | 68 each | unit texts, every complex onset, and odd strings |
| membership in each of the ten phonotactic tables | 1,410 | 141 texts per table: every table entry, unit texts, the corpus alphabet and odd strings |
| `phonotactics.FOREIGN_MARKS.search` | 632 | each mark alone and in context, the listed stems, and words that end in a newline |
| UTF-8 decoding through `graphemes.tokenize` | 95,536 | every two-byte string, plus seeded strings over the lead and continuation boundary bytes |

The calls added for 2.1.7a1 draw from a second seeded generator, so the earlier calls
keep their arguments. A newline at the end of a word reaches only these sub-module
calls, because `analyze` strips it. There, Python's `$` matches before it, and the C++
code does the same.

To regenerate, install the reference release in a fresh Python 3.12 virtual environment.
Python 3.12 has Unicode 15.0.0, the version of `src/unicode_tables.inc`. Then run:

```
python -I tools/dump_goldens.py --out tests/goldens/syllables.jsonl.gz \
    --modules-out tests/goldens/modules.jsonl.gz \
    --gold-sample <silabificador>/tests/gold_sample.json \
    --tests-dir <silabificador>/tests \
    --lexicon <site-packages>/tugalex/data/regional_dict.csv.gz \
    --gold-tsv-dir <tugaphone>/tests/data/gold
```
