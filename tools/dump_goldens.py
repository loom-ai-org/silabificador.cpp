"""Dump the Python silabificador's output as the C++ port's parity goldens.

Every public function is called on every corpus word, and the result is written
as one compact JSON line per input. The C++ parity test recomputes each line and
compares the bytes.

Corpora (all optional except the fuzz and unicode sets, which are generated):
  --gold-sample   silabificador's tests/gold_sample.json
  --tests-dir     silabificador's tests/ (every string literal is an input)
  --lexicon       tugalex regional_dict.csv.gz (every distinct word)
  --gold-tsv-dir  tugaphone tests/data/gold (every word column)

The generated sets are: rules (every rule-table entry in context), fuzz
(seeded random strings) and unicode (case-mapping and whitespace probes).

Usage:
  python -I tools/dump_goldens.py --out tests/goldens/syllables.jsonl.gz \\
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
import sys
import unicodedata

import silabificador
from silabificador import graphemes, morphology, nucleus, parser, phonotactics, stress
from silabificador.graphemes import Grapheme
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


def write_gz(path, header, lines):
    buffer = io.StringIO()
    buffer.write(dumps(header) + "\n")
    for line in lines:
        buffer.write(line + "\n")
    with open(path, "wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as handle:
            handle.write(buffer.getvalue().encode("utf-8"))


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
    if args.lexicon:
        corpora["lexicon"] = list(lexicon_words(args.lexicon))
    if args.gold_tsv_dir:
        corpora["gold_tsv"] = list(gold_tsv_words(args.gold_tsv_dir))
    corpora["rules"] = list(rule_words())
    corpora["fuzz"] = list(fuzz_words(args.fuzz, seed=20261102))
    corpora["unicode"] = list(unicode_words())

    seen, inputs = set(), []
    for name, words in corpora.items():
        before = len(inputs)
        for word in words:
            if word not in seen:
                seen.add(word)
                inputs.append(word)
        print(f"{name}: {len(words)} in, {len(inputs) - before} new", file=sys.stderr)

    header = {"versions": {"silabificador": importlib.metadata.version("silabificador")},
              "python": sys.version.split()[0], "unicode": unicodedata.unidata_version}
    write_gz(args.out, dict(header, count=len(inputs)), (dumps(record(word)) for word in inputs))
    print(f"total: {len(inputs)} in, {len(inputs)} out -> {args.out}", file=sys.stderr)

    modules = [dumps(line) for line in module_records(seed=20261103)]
    write_gz(args.modules_out, dict(header, count=len(modules)), modules)
    print(f"modules: {len(modules)} out -> {args.modules_out}", file=sys.stderr)


if __name__ == "__main__":
    main()
