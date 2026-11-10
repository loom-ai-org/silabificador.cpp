"""Dump the Python silabificador's output as the C++ port's parity goldens.

Every public function is called on every corpus word, and the result is written
as one compact JSON line per input. The C++ parity test recomputes each line and
compares the bytes.

Corpora (all optional except the rules, fuzz and unicode sets, which are generated):
  --gold-sample   silabificador's tests/gold_sample.json
  --tests-dir     silabificador's tests/ (every string literal is an input, and
                  every word of tests/edge_cases.json)
  --lexicon       tugalex regional_dict.csv.gz (every distinct word)
  --gold-tsv-dir  tugaphone tests/data/gold (every word column)

The generated sets are: rules (every rule-table entry in context), fuzz
(seeded random strings), unicode (case-mapping and whitespace probes), and the
phrase sets: clitics, enclitics, contractions, elisions, phrases and pairs. The
phrase sets join words with hyphens, spaces and apostrophes, and draw their base
words from the corpora above.

Usage:
  python -I tools/dump_goldens.py --out tests/goldens/syllables.jsonl.gz \\
      --modules-out tests/goldens/modules.jsonl.gz \\
      --gold-sample ... --tests-dir ... --lexicon ... --gold-tsv-dir ...
"""
import argparse
import ast
import csv
import gzip
import importlib.metadata
import io
import json
import pathlib
import random
import re
import sys
import unicodedata

import silabificador
from silabificador import graphemes, morphology, nucleus, parser, phonotactics, stress
from silabificador.version import VERSION_ALPHA, VERSION_BUILD, VERSION_MAJOR, VERSION_MINOR
from silabificador.graphemes import CONSONANT, SEPARATOR, VOWEL, Grapheme
from silabificador.syllable import Syllable


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def syllable_fields(s):
    return [s.onset, s.glide_on, s.nucleus, s.glide_off, s.coda, s.separator, s.surface,
            s.stressed, s.secondary]


def guarded(call):
    try:
        return call()
    except IndexError:
        return "IndexError"


UNIT_TEXTS = ["a", "e", "i", "o", "u", "y", "ã", "õ", "í", "ú", "iu", "eo", "oi", "",
              "n", "nh", "r", "rr", "s", "l", "z", "m", "x", "qu", "gu", "ch", "-", " ", "'"]
KINDS = [graphemes.VOWEL, graphemes.CONSONANT, graphemes.SEPARATOR]
SURFACES = ["ca", "sa", "da-", "chu", "va", "", "-", " ", "de ", "pé", "mó", "ção", "dòr",
            "xis", "a o", "vos-", "fa", "lam", "jo", "vens", "ír"]
WORD_PARTS = ["ca", "sa", "-", " ", "de", "vos", "a", "chu", "va", "pé", "", "lam", "ens", "'"]
BY_STRESS_TEXTS = ["i", "u", "a", "e", "o", "s", "ss", "l", "", "í", "x", "-", "iu"]
BY_STRESS_WORDS = ["fluiu", "distraiu", "pauis", "instruiu", "tuiuiú", "baiuca", "miudeza",
                   "ciumento", "vinícius", "cuidado", "atribuiu", "contribuiu", "possuiu",
                   "saiu", "caiu", "raiz", "ruinoso", "proibir", "fluido", "contiguidade",
                   "arduidade", "uiu", "iui", "aiu", "auis", "auiss", "eiu-s", "aiu s"]
STEM_PROBES = (sorted(morphology.LISTED_STEMS)
               + [stem + tail for stem in sorted(morphology.LISTED_STEMS)
                  for tail in ("a", "ar", "ado", "s")]
               + ["arruinado", "desenraizamento", "arduidade", "ingenuidade", "tenuidade",
                  "contiguidade", "ambiguidade", "obliquidade", "uidade", "auidades",
                  "suidade\n", "uidade\n", "sublinhar", "sublinear", "subluxação",
                  "obliquar"])
NEWLINE_PROBES = [head + "ou" + final + end
                  for head in ("b", "s", "gr")
                  for final in ("b", "c", "d", "g", "t", "ch", "n", "s", "chs", "bt")
                  for end in ("", "\n", "\n\n", "\nx")] + [
    head + "u" + suffix + end
    for head in ("ard", "contig", "obliq", "")
    for suffix in ("idade", "idades")
    for end in ("", "\n", "\n\n", " ")]
PHONOTACTIC_TABLES = ("VOWELS", "GLIDE_CAPABLE", "STRESS_MARKED", "NASAL_VOWELS", "CONSONANTS",
                      "CONSONANT_DIGRAPHS", "FOREIGN_DIGRAPHS", "FOREIGN_TETRAGRAPHS",
                      "HIATUS_BEFORE", "HIATUS_CODAS", "AO_DIPHTHONG_WORDS")
MARK_PROBES = [head + mark + tail
               for mark in ("k", "w", "y", "ä", "ö", "ü", "ë", "ck", "sch", "sh", "th", "ph", "tz",
                            "eau", "b", "c", "d", "g", "t", "ch", "K", "c h", "e au", "x", "")
               for head in ("", "a")
               for tail in ("", "a", "\n", "\n\n", "s", " ")]
TOKENIZE_ALPHABET = "aeiouüäöëykwbcdghnprstxzç- eauour" + "\n"
WORD_EDGE_TEXTS = ["a", "o", "ao", "s", "i", "u", "iu", "e", "eau", "d", "qu", "gu", "",
                   "-", " ", "'", "’", "'’", "x", "lh"]
WORD_EDGE_WORDS = ["ao", "aos", "ao pé", "aos pés", "ao-pé", "pé-ao", "d'ao", "d'aos", "ao'",
                   "'ao", "ao’s", "a o", "a os", "caos", "aorta", "x ao", "ao x", "ao-ao",
                   "saiu-se", "saiu se", "saiu's", "saiu’s", "saius-se", "saiu-s", "distraiu-o",
                   "contribuiu-lhe", "atribuiu-se-lhe", "pauis-x", "pauis x", "caiu d'água",
                   "caiu-o d'água", "abaiuca-se", "viu-se", "beaux-arts", "beaux arts",
                   "chauffeur-chefe", "bureau-x", "eau-de", "pau-d'água", "sant'ana",
                   "bê-á-'bá", "três pa'anga", "deu-te-lo", "dar-te-ei", "no-lo"]
CLITIC_SURFACES = ["dá-", "lo", "-", "te-", "lo-", "ei", "d'", "'á", "’", "'", "ven", "deu-", "se",
                   "pau-", "d'á", "gua", "a ", "ao ", "no-", "vo-", "", "s'", "-'", "á-'", "lhe",
                   "frei ", "às ", "mo", "ca", "sa", "pé-", " ", "m'o", "nos-", "nas"]
CLITIC_WORD_PARTS = ["dá", "lo", "-", " ", "'", "’", "te", "se", "ei", "deu", "no", "vo", "pau",
                     "d", "água", "a", "ao", "casa", "frei", "", "lhe", "mo", "nas", "pelos", "à",
                     "um"]
WORD_EDGE_ALPHABET = "aaooeiusdrlx- '’" + "\n"
DECODE_BYTES = [0x41, 0x61, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xAC, 0xBF, 0xC0, 0xC1, 0xC2,
                0xC3, 0xA7, 0xDF, 0xE0, 0xE2, 0x82, 0xED, 0xEF, 0xF0, 0xF4, 0xF5, 0xFF, 0x2D]


def module_records(seed):
    """Every sub-module function, called with arguments built here rather than
    produced by analyze(): odd units, empty texts, negative and descending
    indices, empty surfaces, and invalid UTF-8."""
    rng = random.Random(seed)

    def units(max_len=8):
        return [Grapheme(rng.choice(UNIT_TEXTS), rng.choice(KINDS))
                for _ in range(rng.randint(0, max_len))]

    def plain(us):
        return [[u.text, u.kind] for u in us]

    def word():
        return "".join(rng.choice(WORD_PARTS) for _ in range(rng.randint(0, 5)))

    # Negative start and end whose nucleus test must wrap the index past the
    # first unit; random draws rarely build this case.
    wrap = [Grapheme("ã", SEPARATOR), Grapheme("ç", CONSONANT), Grapheme("U", CONSONANT),
            Grapheme("", CONSONANT), Grapheme("eoiu", CONSONANT), Grapheme("i", VOWEL),
            Grapheme("z", CONSONANT)]
    for start, end in ((-7, -2), (-7, -1), (-6, -2), (-5, -1)):
        yield ["resolve", plain(wrap), start, end, guarded(lambda: nucleus.resolve(wrap, start, end))]
    for _ in range(40000):
        us = units()
        start, end = rng.randint(-10, 10), rng.randint(-10, 10)
        yield ["resolve", plain(us), start, end, guarded(lambda: nucleus.resolve(us, start, end))]
    for _ in range(30000):
        us = units()
        group = None if rng.random() < 0.1 else [rng.randint(-8, 8) for _ in range(rng.randint(0, 4))]
        yield ["syllable_of", plain(us), group,
               guarded(lambda: syllable_fields(Syllable.of(us, nucleus=group)))]
    for _ in range(20000):
        syllables = [Syllable(surface=rng.choice(SURFACES), onset=rng.choice(SURFACES),
                              stressed=rng.random() < 0.5, secondary=rng.random() < 0.5)
                     for _ in range(rng.randint(0, 5))]
        w = word()
        yield ["assign", [syllable_fields(s) for s in syllables], w,
               [syllable_fields(s) for s in stress.assign(syllables, w)]]
    for _ in range(20000):
        strings = [rng.choice(SURFACES) for _ in range(rng.randint(0, 5))]
        w = word()
        yield ["stressed_index", strings, w, guarded(lambda: stress.stressed_index(strings, w))]
    for _ in range(20000):
        us, w = units(), rng.choice(["subliminar", "distribuidor", "sublocar", "abrogar",
                                     "adligar", "juiz", "ajuizar", "possuir", word()])
        yield ["boundaries", w, plain(us), sorted(morphology.boundaries(w, us))]
    for _ in range(10000):
        w = "".join(rng.choice("AbcÇÉqgu- ’aeiouãõíúnhlrs") for _ in range(rng.randint(0, 10)))
        yield ["parse", w, [syllable_fields(s) for s in parser.parse(w)]]
        yield ["tokenize", w, plain(graphemes.tokenize(w))]
    for text in sorted(set(UNIT_TEXTS + SURFACES + sorted(phonotactics.COMPLEX_ONSETS)
                           + ["A", "Á", "pl", "plr", "aa", "ç"])):
        yield ["is_vowel", text, phonotactics.is_vowel(text)]
        yield ["is_complex_onset", text, phonotactics.is_complex_onset(text)]
    # Decoding: every byte pair, then seeded longer strings over the boundary bytes.
    for first in range(256):
        for second in range(256):
            data = bytes([first, second])
            yield ["decode", data.hex(), plain(graphemes.tokenize(data.decode("utf-8", "replace")))]
    for _ in range(30000):
        data = bytes(rng.choice(DECODE_BYTES) for _ in range(rng.randint(1, 10)))
        yield ["decode", data.hex(), plain(graphemes.tokenize(data.decode("utf-8", "replace")))]

    # Calls added with 2.1.7a1 draw from a second generator, so the calls above
    # keep their arguments from one release to the next.
    rng = random.Random(seed + 1)
    for _ in range(30000):
        us = [Grapheme(rng.choice(BY_STRESS_TEXTS), rng.choice(KINDS))
              for _ in range(rng.randint(0, 8))]
        groups = [[rng.randint(-9, 9) for _ in range(rng.choice((0, 1, 1, 1, 2, 2, 3)))]
                  for _ in range(rng.randint(0, 5))]
        stressed = [rng.random() < 0.5 for _ in range(rng.randint(0, 6))]
        hard = sorted({rng.randint(-9, 9) for _ in range(rng.choice((0, 0, 1, 2)))})
        yield ["by_stress", plain(us), groups, stressed, hard,
               guarded(lambda: nucleus.by_stress(us, groups, stressed, set(hard)))]
    # Well-formed by_stress calls: the units and groups of real words.
    for w in BY_STRESS_WORDS:
        us = graphemes.tokenize(w)
        hard = morphology.boundaries(w, us)
        groups = [g for run in parser._split_runs(us)
                  for g in nucleus.resolve(us, run[0], run[-1])]
        for stressed in ([True] * len(groups), [False] * len(groups),
                         [k == len(groups) - 1 for k in range(len(groups))]):
            yield ["by_stress", plain(us), groups, stressed, sorted(hard),
                   guarded(lambda: nucleus.by_stress(us, groups, stressed, hard))]
    for _ in range(20000):
        w = rng.choice(STEM_PROBES) if rng.random() < 0.7 else word()
        if rng.random() < 0.5:
            us = graphemes.tokenize(w)
        else:
            us = [Grapheme(rng.choice(UNIT_TEXTS), rng.choice(KINDS))
                  for _ in range(rng.randint(0, 8))]
        yield ["boundaries", w, plain(us), sorted(morphology.boundaries(w, us))]
    for _ in range(20000):
        w = "".join(rng.choice(TOKENIZE_ALPHABET) for _ in range(rng.randint(0, 12)))
        yield ["tokenize", w, plain(graphemes.tokenize(w))]
        yield ["parse", w, [syllable_fields(s) for s in parser.parse(w)]]
    # A final newline: Python's $ matches before it, in FOREIGN_MARKS and in the
    # -idade pattern. analyze() strips it, so only the sub-module calls see it.
    for w in NEWLINE_PROBES:
        yield ["tokenize", w, plain(graphemes.tokenize(w))]
        yield ["parse", w, [syllable_fields(s) for s in parser.parse(w)]]
        yield ["boundaries", w, plain(graphemes.tokenize(w)), sorted(morphology.boundaries(w, graphemes.tokenize(w)))]
    # Membership in every phonotactic table, each answered in C++ by a predicate.
    tables = {name: getattr(phonotactics, name) for name in PHONOTACTIC_TABLES}
    texts = set(UNIT_TEXTS + SURFACES + sorted(phonotactics.COMPLEX_ONSETS) + list(ALPHABET)
                + ["zh", "ff", "tsch", "tschx", "tsc", "Tsch", "ZH", "aa", "nhh"])
    for table in tables.values():
        texts.update(table)
    for name, table in tables.items():
        for text in sorted(texts):
            yield ["table", name, text, text in table]
    for w in sorted(set(STEM_PROBES + NEWLINE_PROBES + MARK_PROBES)):
        yield ["has_foreign_mark", w, phonotactics.FOREIGN_MARKS.search(w) is not None]

    # Separators that end a word and apostrophes that do not: the stressed iu scan,
    # the contraction ao, the elided onset and the enclitic host. These calls draw
    # from their own generator, so the calls above keep their arguments.
    rng = random.Random(seed + 2)
    for _ in range(20000):
        us = [Grapheme(rng.choice(WORD_EDGE_TEXTS), rng.choice(KINDS))
              for _ in range(rng.randint(0, 8))]
        start, end = rng.randint(-9, 9), rng.randint(-9, 9)
        yield ["resolve", plain(us), start, end, guarded(lambda: nucleus.resolve(us, start, end))]
    for w in WORD_EDGE_WORDS:
        us = graphemes.tokenize(w)
        for run in parser._split_runs(us):
            yield ["resolve", plain(us), run[0], run[-1],
                   guarded(lambda: nucleus.resolve(us, run[0], run[-1]))]
    for _ in range(20000):
        us = [Grapheme(rng.choice(BY_STRESS_TEXTS + ["'", "’", " "]), rng.choice(KINDS))
              for _ in range(rng.randint(0, 8))]
        groups = [[rng.randint(-9, 9) for _ in range(rng.choice((0, 1, 1, 1, 2, 2, 3)))]
                  for _ in range(rng.randint(0, 5))]
        stressed = [rng.random() < 0.5 for _ in range(rng.randint(0, 6))]
        hard = sorted({rng.randint(-9, 9) for _ in range(rng.choice((0, 0, 1, 2)))})
        yield ["by_stress", plain(us), groups, stressed, hard,
               guarded(lambda: nucleus.by_stress(us, groups, stressed, set(hard)))]
    for w in WORD_EDGE_WORDS:
        us = graphemes.tokenize(w)
        hard = morphology.boundaries(w, us)
        groups = [g for run in parser._split_runs(us)
                  for g in nucleus.resolve(us, run[0], run[-1])]
        for stressed in ([True] * len(groups), [False] * len(groups),
                         [k == len(groups) - 1 for k in range(len(groups))]):
            yield ["by_stress", plain(us), groups, stressed, sorted(hard),
                   guarded(lambda: nucleus.by_stress(us, groups, stressed, hard))]
    for _ in range(20000):
        us = [Grapheme(rng.choice(WORD_EDGE_TEXTS), rng.choice(KINDS))
              for _ in range(rng.randint(0, 8))]
        group = None if rng.random() < 0.1 else [rng.randint(-8, 8) for _ in range(rng.randint(0, 4))]
        yield ["syllable_of", plain(us), group,
               guarded(lambda: syllable_fields(Syllable.of(us, nucleus=group)))]
    for _ in range(20000):
        syllables = [Syllable(surface=rng.choice(CLITIC_SURFACES), onset=rng.choice(CLITIC_SURFACES),
                              stressed=rng.random() < 0.5, secondary=rng.random() < 0.5)
                     for _ in range(rng.randint(0, 6))]
        w = "".join(rng.choice(CLITIC_WORD_PARTS) for _ in range(rng.randint(0, 6)))
        yield ["assign", [syllable_fields(s) for s in syllables], w,
               [syllable_fields(s) for s in stress.assign(syllables, w)]]
    for _ in range(20000):
        strings = [rng.choice(CLITIC_SURFACES) for _ in range(rng.randint(0, 6))]
        w = "".join(rng.choice(CLITIC_WORD_PARTS) for _ in range(rng.randint(0, 6)))
        yield ["stressed_index", strings, w, guarded(lambda: stress.stressed_index(strings, w))]
    # Well-formed assign and stressed_index calls: the first-pass syllables of
    # real phrases against their own spelling.
    for w in WORD_EDGE_WORDS:
        syllables = parser.parse(w)
        yield ["assign", [syllable_fields(s) for s in syllables], w,
               [syllable_fields(s) for s in stress.assign(syllables, w)]]
        strings = [str(s) for s in syllables]
        yield ["stressed_index", strings, w, guarded(lambda: stress.stressed_index(strings, w))]
    for _ in range(20000):
        w = "".join(rng.choice(WORD_EDGE_ALPHABET) for _ in range(rng.randint(0, 12)))
        yield ["tokenize", w, plain(graphemes.tokenize(w))]
        yield ["parse", w, [syllable_fields(s) for s in parser.parse(w)]]


def write_gz(path, header, lines):
    buffer = io.StringIO()
    buffer.write(dumps(header) + "\n")
    for line in lines:
        buffer.write(line + "\n")
    with open(path, "wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as handle:
            handle.write(buffer.getvalue().encode("utf-8"))
    # Count the lines in the file on disk, not in the list that was written.
    with gzip.open(path, "rt", encoding="utf-8") as handle:
        return sum(1 for _ in handle) - 1


def record(word):
    cleaned = word.strip().lower()
    syllables = silabificador.analyze(word)
    units = graphemes.tokenize(cleaned)
    try:
        module_index = stress.stressed_index([str(s) for s in syllables], cleaned)
    except IndexError:
        module_index = "IndexError"
    return [
        word,
        [[s.onset, s.glide_on, s.nucleus, s.glide_off, s.coda, s.separator, s.surface,
          s.stressed, s.secondary] for s in syllables],
        silabificador.stressed_index(word),
        [[u.text, u.kind] for u in units],
        sorted(morphology.boundaries(cleaned, units)),
        module_index,
    ]


def string_literals(tests_dir):
    for path in sorted(pathlib.Path(tests_dir).glob("test_*.py")):
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                if len(node.value) <= 40 and "\n" not in node.value:
                    yield node.value


def lexicon_words(path):
    with gzip.open(path, "rt", encoding="utf-8") as handle:
        lines = (line for line in handle if not line.startswith("#"))
        for row in csv.DictReader(lines):
            yield row["word"]


def gold_tsv_words(directory):
    for path in sorted(pathlib.Path(directory).glob("*.tsv")):
        for line in path.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                yield line.split("\t", 1)[0]


ALPHABET = (
    "abcdefghijklmnopqrstuvwxyzçñ" "aeiou" "aeiou" "áéíóúâêîôûàèìòùãõäëïöü"
    "ABCÇÉÃÕÂ" "- '’" "lrnhsqgu" "\t  " "0123456789" ".,;!?" "Σσİ́̃"
)


def rule_words():
    # Every entry of every rule table, read from the Python package itself, in
    # the contexts that exercise it.
    for clitic in sorted(stress.CLITICS):
        for template in ("{}", "casa-{}-casa", "casa {} casa", "{}-chuva", "pé {} moleque"):
            yield template.format(clitic)
    for ending in stress.OXYTONE_ENDINGS + stress.PAROXYTONE_ENDINGS:
        for base in ("cas", "pal", "aguard", "fé", "x"):
            yield base + ending
            yield "guarda-" + base + ending
    for prefix, stems in morphology.LATINATE_PREFIX_STEMS.items():
        for stem in stems:
            for tail in ("", "ar", "ação", "ado"):
                yield prefix + stem + tail
        yield prefix + "lime"
        yield prefix + "rogar"
    for stem in morphology.VOWEL_FINAL_STEMS:
        for tail in ("", "i", "ir", "is", "ido", "idor", "ição", "imento", "a"):
            yield stem + tail
            yield "re" + stem + tail
    for digraph in sorted(phonotactics.CONSONANT_DIGRAPHS | phonotactics.FOREIGN_DIGRAPHS):
        for template in ("a{}a", "{}a", "ai{}a", "a{}", "ui{}o"):
            yield template.format(digraph)
    for onset in sorted(phonotactics.COMPLEX_ONSETS):
        yield "a" + onset + "a"
        yield "as" + onset + "a"
        yield "ai" + onset + "a"
    for vowel in sorted(phonotactics.VOWELS):
        for glide in sorted(phonotactics.VOWELS):
            for coda in ("", "s", "x", "l", "m", "n", "r", "z", "nh", "rr", "ss", "da", "nha"):
                yield "b" + vowel + glide + coda
                yield "qu" + vowel + glide + coda
    yield from rule_words_2_1_7()


def rule_words_2_1_7():
    """The tables and rules added between 2.1.1a2 and 2.1.7a1, in context."""
    # A doubled ff is no longer one consonant.
    for template in ("a{}a", "{}a", "ai{}a", "a{}", "ui{}o"):
        yield template.format("ff")
    for tetragraph in sorted(phonotactics.FOREIGN_TETRAGRAPHS):
        for template in ("a{}a", "{}a", "ai{}a", "a{}", "ku.bi{}ek", "pu{}ista", "a{}ra"):
            yield template.format(tetragraph).replace(".", "")
        yield tetragraph[:3] + "a"
    # Loanword vowels: each spelling, each context, with and without a foreign mark.
    marks = ("", "k", "w", "y", "ä", "ö", "ü", "ë", "ck", "sch", "sh", "th", "ph", "tz", "eau")
    finals = ("", "b", "c", "d", "g", "t", "ch", "a", "s", "\n")
    for spelling in ("eau", "eu", "ou", "eao", "oi"):
        for tail in ("", "s", "x", "r", "rs", "rt", "n", "nd", "sa", "xa", "ra", "ro"):
            for mark in marks:
                yield "b" + spelling + tail + mark
                yield mark + "b" + spelling + tail
            for final in finals:
                yield "g" + spelling + tail + "m" + final
                yield spelling + tail + final
    # The trema: a pronounced u after q or g is still in the onset.
    for consonant in "qg":
        for vowel in sorted(phonotactics.VOWELS):
            for template in ("{}ü{}", "a{}ü{}nta", "{}ü{}n", "{}u{}", "{}ü"):
                yield template.format(consonant, vowel)
    # Nouns in -idade after u, and the u of the onset that the rule leaves alone.
    for onset in ("", "a", "b", "d", "g", "q", "n", "t", "ss", "gu", "qu", "ü"):
        for suffix in ("idade", "idades", "idade\n", "idades\n", "idad", "idadez", "idade-x"):
            yield onset + "u" + suffix
            yield "ar" + onset + "u" + suffix
    # Every listed stem, alone and inside derivatives.
    for stem in sorted(morphology.LISTED_STEMS):
        for head in ("", "re", "des", "en", "ar", "a"):
            for tail in ("", "a", "o", "ar", "ado", "s", "inho", "mente", "-chuva"):
                yield head + stem + tail
    # Three-vowel runs ending in iu or ui, stressed and unstressed, final and not.
    for first in "aeiouáãy":
        for pair in ("iu", "ui", "íu", "uí", "iú", "uú", "ii", "uu"):
            for tail in ("", "s", "ss", "x", "r", "ns", "m", "ca", "s-chuva", "-sol", " de"):
                for head in ("b", "fl", "tr", "qu", "gu", "ób", "pá-b"):
                    yield head + first + pair + tail
    # iu and ui in two-vowel runs, with the stress before, on and after them.
    for pair in ("iu", "ui", "íu", "iú"):
        for head in ("m", "c", "v", "qu", "gu", "x", ""):
            for tail in ("deza", "mento", "s", "", "da", "dá", "nícius", "-chuva", "zinho"):
                yield head + pair + tail


PRONOUN_ENDINGS = ("ei", "ás", "á", "emos", "eis", "ão", "ia", "ias", "íamos", "íeis", "iam")
MESOCLITIC_PRONOUNS = ("me", "te", "se", "lhe", "nos", "vos", "lhes", "mo", "lho", "vo-lo")
PHRASES = (
    # Compounds and phrases with an unstressed word or a hyphenated pronoun.
    "guarda-chuva", "bem-te-vi", "arco-íris", "bem-me-quer", "mal-me-quer", "guarda-redes",
    "pé-de-meia", "chapéu de chuva", "pão de ló", "trás-os-montes", "tête-à-tête", "dia-a-dia",
    "cara-a-cara", "à-vontade", "à toa", "à-toa", "aqui del-rei", "aqui d'el-rei", "à-d'el-rei",
    "el-rei", "trinta-e-um", "vinte-e-um", "diz-que-diz-que", "nem-nem", "palmeira-dum",
    "no-lo", "vo-lo", "pelos campos", "pelas ruas", "pelo caminho", "pela estrada",
    "às vezes", "ao pé", "aos poucos", "num dia", "nuns dias", "dum lado", "duns tantos",
    "sob a ponte", "frei luís", "frei tomé", "dom pedro", "são paulo", "seu joão",
    "cada um", "mais uma", "capítulo um", "ei-lo", "eis-me", "eis-nos",
    "para-brisas", "para-choques", "para-raios", "para-sol", "mãe-d'água", "pau-d'alho",
    "revelou-m'o", "confiamos n'o que nos salvou", "pel'a que é nossa", "d'os lusíadas",
    "esse milagre revelou-m'o", "dá-se", "deixa-o", "partir-lhe", "amá-lo-ei",
    "enviar-lhe-emos", "fá-lo-ás", "habitá-la-iam", "trá-la-á", "atraí-lo", "atraí-lo-ia",
    "possuí-la", "possuí-la-ia", "fazem-no", "faze-lo", "fazemo-lo", "fazei-lo",
    "quere-o", "requere-o", "dar-vo-la-ia", "mostrou-vo-los", "limpei-lha",
    "entregar-mos", "diga-me o preço do livro", "vejo-te", "fala-se", "dá-me o livro",
    # The contractions ao and aos, and ao inside a word.
    "ao", "aos", "Ao", "AOS", "aos pés", "ao deus-dará", "ao passo que", "ao-pé",
    "vou ao cinema", "dei-o aos pais", "a os", "a o", "a Os Lusíadas", "a O que tudo pode",
    "d'aos", "'ao", "ao'", "ao  pé", "ao.", "aos,", "caos", "aorta", "maometano", "aonde",
    "macau", "cacau", "caetano", "são-caetano", "daos", "naos", "kaos", "tao", "macao",
    "ao-ao", "aos-aos", "ao ao", "pão ao forno", "àquele", "à", "às", "ao’", "d’aos",
    # The elided vowel of d', n', sant' and the U+2019 apostrophe.
    "pau-d'água", "estrela-d'alva", "lobo-d'alsácia", "copo d'água", "caiu d'água",
    "caiu-o d'água", "caiu n'água", "saiu d'aqui", "saiu-se-lhe d'aqui", "sant'ana",
    "nun'álvares", "castanha-d’água", "caiu d’água", "pau-d’água", "pa'anga", "bahá'í",
    "três pa'anga", "bê-á-'bá", "bê-á-’bá", "enguia-d’água-doce", "d’os lusíadas",
    "copo d' água", "d'", "'", "’", "d'-", "-d'", "x'-y", "a'-'a", "cd-rom",
    # The stressed iu or ui before a separator, and the loanword vowels before one.
    "saiu-se", "caiu-lhe", "saiu-lhes", "distraiu-se", "distraiu-o", "atraiu-nos",
    "instruiu-os", "possuiu-a", "contribuiu-lhe", "atribuiu-se-lhe", "saiu de casa",
    "abaiuca-se", "amiudá-lo", "viu-se", "partiu-se", "distraí-lo", "pauis-me", "pauis de",
    "beaux-arts", "beaux arts", "chauffeur-chefe", "bureau-chefe", "boxeur-x", "eau-de-vie",
    "bordeaux'x", "beaux’arts",
)


def phrase_corpora(corpora):
    """Words joined by hyphens, spaces and apostrophes. The base words are the
    plain words (no separator) of the corpora already read, so the phrase sets
    grow with the lexicon; a seeded generator picks the samples."""
    rng = random.Random(20261110)
    bases = sorted({w.lower() for name in ("gold_sample", "edge_cases", "lexicon", "gold_tsv")
                    for w in corpora.get(name, ())
                    if w and not any(c in w for c in "- '’\n\t\r")})
    pronouns = sorted(stress.CLITIC_PRONOUNS)

    def sample(words, count):
        return sorted(rng.sample(words, min(count, len(words))))

    def lo_stem(infinitive):
        stem, vowel = infinitive[:-2], infinitive[-2]
        if vowel == "a":
            return stem + "á"
        if vowel == "e":
            return stem + "ê"
        return stem + ("í" if stem[-1:] in "aeiou" else "i")

    clitics = []
    for clitic in sorted(stress.CLITICS):
        for template in ("{0} casa", "casa {0}", "{0} casa-chuva", "vende-{0}", "vendeu-{0}",
                         "fazem-{0}", "dar-{0}-ei", "vende-{0}-lo", "{0}-lo", "{0}-{0}", "{0}'casa",
                         "d'{0}", "casa d'{0}", "{0} {0} casa", "saiu-{0}", "pauis-{0}",
                         "casa-{0}", "{0}-casa", "dá-{0}", "deu-te-{0}"):
            clitics.append(template.format(clitic))

    infinitives = [w for w in bases if re.fullmatch(r"[a-zà-ü]+(ar|er|ir)", w)]
    preterites = [w for w in bases if re.fullmatch(r"[a-zà-ü]{2,}(ou|eu|iu)", w)]
    nasal = [w for w in bases if re.fullmatch(r"[a-zà-ü]{2,}(am|em|ão|õe)", w)]
    falling = [w for w in bases if re.search(r"[aeiouáéíóúâêôãõ](iu|ui|íu|uí)s?$", w)]
    enclitics = []
    for w in sample(infinitives, 300):
        enclitics += [f"{w}-{p}" for p in pronouns]
        enclitics += [f"{lo_stem(w)}-{p}" for p in ("lo", "la", "los", "las")]
    for w in sample(preterites, 300):
        enclitics += [f"{w}-{p}" for p in pronouns]
        enclitics += [f"{w}-me-lo", f"{w}-vo-lo", f"{w}-to-las"]
    for w in sample(nasal, 150):
        enclitics += [f"{w}-{p}" for p in ("no", "na", "nos", "nas")]
    for w in sample(infinitives, 100):
        for ending in PRONOUN_ENDINGS:
            enclitics += [f"{w}-{p}-{ending}" for p in MESOCLITIC_PRONOUNS]
            enclitics.append(f"{lo_stem(w)}-lo-{ending}")
    for w in falling:
        enclitics += [f"{w}{tail}" for tail in ("-se", "-lhe", "-o", "-se-lhe", " de casa", " se",
                                                "'s", "’s", "-x", " d'água", "s-se")]

    contractions = []
    for w in sample(bases, 1500):
        contractions += [f"ao {w}", f"aos {w}", f"{w} ao", f"{w}-ao", f"ao-{w}",
                         f"dei-o aos {w}", f"d'ao {w}", f"{w} ao {w}"]

    elisions = []
    for w in sample(bases, 1500):
        elisions += [f"{w} d'água", f"{w}-d'alho", f"{w}-se-lhe d'aqui", f"{w} d’água",
                     f"{w} d' água"]
        if w[:1] in "aeiouáéíóúâêôãõà":
            elisions += [f"pau-d'{w}", f"caiu n'{w}", f"sant'{w}", f"d'{w}", f"d’{w}",
                         f"{w}-d'{w}"]

    pairs, tails = [], bases + pronouns
    for _ in range(20000):
        pairs.append(rng.choice(bases) + rng.choice(("-", " ", "'", "’")) + rng.choice(bases))
    for _ in range(5000):
        pairs.append(rng.choice(bases) + rng.choice(("-", " ", "'")) + rng.choice(pronouns)
                     + rng.choice(("-", " ", "'")) + rng.choice(tails))

    return {"clitics": clitics, "enclitics": enclitics, "contractions": contractions,
            "elisions": elisions, "phrases": list(PHRASES), "pairs": pairs}


def fuzz_words(count, seed):
    rng = random.Random(seed)
    for _ in range(count):
        yield "".join(rng.choice(ALPHABET) for _ in range(rng.randint(0, 14)))


def unicode_words():
    # Every code point whose lowercase or whitespace status matters, in a
    # context that exercises it, plus the Greek final-sigma contexts.
    for cp in range(0x110000):
        if 0xD800 <= cp <= 0xDFFF:
            continue
        c = chr(cp)
        if c.lower() != c or c.isspace() or unicodedata.category(c) in ("Mn", "Lt"):
            yield "a" + c + "e"
            yield c + "ba"
    for text in ("ΣΑΣ", "ΑΣ", "Σ", "ΑΣΑ", "ΆΣ", "ΑΣ́", "Α.Σ", "ΑΣ'", "'Σ", "aΣ b", "ΑΣ1"):
        yield text


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument("--out", required=True)
    cli.add_argument("--gold-sample")
    cli.add_argument("--tests-dir")
    cli.add_argument("--lexicon")
    cli.add_argument("--gold-tsv-dir")
    cli.add_argument("--modules-out", required=True)
    cli.add_argument("--fuzz", type=int, default=30000)
    args = cli.parse_args()

    corpora = {}
    if args.gold_sample:
        data = json.loads(pathlib.Path(args.gold_sample).read_text(encoding="utf-8"))
        corpora["gold_sample"] = [entry["word"] for entry in data["entries"]]
    if args.tests_dir:
        corpora["tests"] = list(string_literals(args.tests_dir))
        edge_cases = pathlib.Path(args.tests_dir) / "edge_cases.json"
        if edge_cases.exists():
            data = json.loads(edge_cases.read_text(encoding="utf-8"))
            corpora["edge_cases"] = [entry["word"] for entry in data["entries"]]
    if args.lexicon:
        corpora["lexicon"] = list(lexicon_words(args.lexicon))
    if args.gold_tsv_dir:
        corpora["gold_tsv"] = list(gold_tsv_words(args.gold_tsv_dir))
    corpora["rules"] = list(rule_words())
    corpora["fuzz"] = list(fuzz_words(args.fuzz, seed=20261102))
    corpora["unicode"] = list(unicode_words())
    corpora.update(phrase_corpora(corpora))

    # Each input is written once, under the first corpus that holds it. The
    # header lists the corpora in file order with the number of lines each owns,
    # so the parity test can report its mismatches per corpus.
    seen, inputs, owned = set(), [], []
    for name, words in corpora.items():
        before = len(inputs)
        for word in words:
            if word not in seen:
                seen.add(word)
                inputs.append(word)
        owned.append([name, len(inputs) - before])
        if not words:
            sys.exit(f"{name}: empty corpus")

    # The installed package must be the release its version module names, so a
    # stale or mixed environment cannot produce goldens under the wrong header.
    installed = importlib.metadata.version("silabificador")
    declared = f"{VERSION_MAJOR}.{VERSION_MINOR}.{VERSION_BUILD}" + (
        f"a{VERSION_ALPHA}" if VERSION_ALPHA else "")
    if installed != declared:
        sys.exit(f"silabificador metadata {installed} != version.py {declared}")
    header = {"versions": {"silabificador": installed},
              "python": sys.version.split()[0], "unicode": unicodedata.unidata_version}
    lines = [dumps(record(word)) for word in inputs]
    written = write_gz(args.out, dict(header, count=len(lines), corpora=owned), lines)
    for (name, words), (_, new) in zip(corpora.items(), owned):
        print(f"{name}: {len(words)} in, {new} new", file=sys.stderr)
    print(f"total: {len(inputs)} in, {written} out -> {args.out}", file=sys.stderr)
    if written != len(inputs):
        sys.exit("short golden file")

    modules = [dumps(line) for line in module_records(seed=20261103)]
    calls = {}
    for line in modules:
        name = line[2:line.index('"', 2)]
        calls[name] = calls.get(name, 0) + 1
    written = write_gz(args.modules_out, dict(header, count=len(modules)), modules)
    for name, count in calls.items():
        print(f"modules {name}: {count} calls", file=sys.stderr)
    print(f"modules: {len(modules)} in, {written} out -> {args.modules_out}", file=sys.stderr)
    if written != len(modules):
        sys.exit("short modules golden file")


if __name__ == "__main__":
    main()
