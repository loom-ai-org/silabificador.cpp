// Built against an installed silabificador to prove the CMake package works.
#include <silabificador/silabificador.hpp>

int main() {
    const auto syllables = silabificador::syllabify("computador");
    return syllables.size() == 4 && syllables[3] == "dor" ? 0 : 1;
}
