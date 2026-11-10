# silabificador.cpp

A C++17 port of [silabificador](https://github.com/TigreGotico/silabificador), the
rule-based Portuguese syllabifier. It gives the same output as the Python package,
byte for byte: see [PARITY.md](PARITY.md). It has no dependencies and loads no
model.

```cpp
#include <silabificador/silabificador.hpp>

silabificador::syllabify("guarda-chuva");   // {"guar", "da-", "chu", "va"}
silabificador::stressed_index("sílaba");    // 0
for (const auto& s : silabificador::analyze("quadro")) {
    // s.onset, s.glide_on, s.nucleus, s.glide_off, s.coda, s.stressed, ...
}
```

The engine works in layers:

- Spelling becomes grapheme units, and each digraph is decided in context.
- Vowel runs become nuclei and glides. The contractions ao and aos are one
  syllable; ao inside a word is two (a.or.ta).
- Consonants are placed by onset maximization, limited by the onset inventory.
- Prefix, suffix and stem boundaries override that placement. The stems whose
  boundaries no rule reaches are in `data/stems.csv`, which the library embeds.
- An apostrophe marks an elided vowel, so the consonant before it starts the
  next syllable (pau-.d'á.gua). A hyphen or a space ends a word.
- Stress is read from the accents and the ending. Unstressed words and enclitic
  pronouns carry none, so the verb keeps the stress (ven.DEU-.se).
- A second nucleus pass reads the stress: a stressed final iu or ui after a vowel
  is the diphthong (flu.iu), and an unstressed iu is two nuclei (mi.u.de.za).

Each source file under `src/` matches one Python module and carries its
references.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix /usr/local
```

The tests need zlib, which reads the compressed goldens. The library itself needs
nothing.

To use the library from another CMake project:

```cmake
find_package(silabificador REQUIRED)
target_link_libraries(app PRIVATE silabificador::silabificador)
```

## Licence

MIT, the same licence as the Python package.
