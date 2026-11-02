// The examples from the Python package's docstrings, as checks.
#include <iostream>
#include <string>
#include <vector>

#include "silabificador/silabificador.hpp"

namespace sb = silabificador;

static int failures = 0;

template <typename T>
static void expect(const T& got, const T& want, const char* what) {
    if (!(got == want)) {
        ++failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

using Strings = std::vector<std::string>;

int main() {
    const auto casa = sb::analyze("casa");
    expect(casa.size(), size_t{2}, "analyze(casa) has two syllables");
    expect(casa[0].onset + "|" + casa[0].nucleus, std::string("c|a"), "casa first syllable");
    expect(casa[0].stressed, true, "casa first syllable stressed");
    expect(casa[1].stressed, false, "casa second syllable unstressed");

    expect(sb::syllabify("computador"), Strings{"com", "pu", "ta", "dor"}, "computador");
    expect(sb::syllabify("guarda-chuva"), Strings{"guar", "da-", "chu", "va"}, "guarda-chuva");
    expect(sb::syllabify("  País "), Strings{"pa", "ís"}, "strip and lowercase");
    expect(sb::syllabify("pais"), Strings{"pais"}, "pais is one syllable");
    expect(sb::syllabify("subliminar"), Strings{"sub", "li", "mi", "nar"}, "prefix boundary");
    expect(sb::syllabify("sublime"), Strings{"su", "bli", "me"}, "no prefix boundary");
    expect(sb::syllabify("distribuidor"), Strings{"dis", "tri", "bu", "i", "dor"}, "stem seam");
    expect(sb::syllabify(""), Strings{}, "empty word");

    expect(sb::stressed_index("computador"), 3, "computador stress");
    expect(sb::stressed_index("casa"), 0, "casa stress");
    expect(sb::stressed_index("sílaba"), 0, "sílaba stress");

    const auto compound = sb::analyze("guarda-chuva");
    expect(compound[0].secondary, true, "guarda-chuva secondary stress");
    expect(compound[2].stressed, true, "guarda-chuva primary stress");

    const auto fe = sb::analyze("fé");
    expect(fe[0].length(), size_t{2}, "Syllable length counts code points");
    expect(sb::graphemes::tokenize("chá")[0].length(), size_t{2}, "Grapheme length of a digraph");
    expect(sb::graphemes::tokenize("chá")[1].str(), std::string("á"), "Grapheme str");
    expect(sb::graphemes::tokenize("chá")[1].length(), size_t{1}, "Grapheme length counts code points");
    expect(sb::code_points("\x80"), size_t{1}, "a lone continuation byte is one U+FFFD");
    expect(sb::code_points("\xED\xA0\x80"), size_t{3}, "a surrogate encoding is three U+FFFD");
    expect(sb::code_points("\xF0\x9F\x98"), size_t{1}, "a truncated sequence is one U+FFFD");

    const sb::Syllabifier syllabifier;
    expect(syllabifier.syllabify("rainha"), Strings{"ra", "i", "nha"}, "Syllabifier.syllabify");
    expect(syllabifier.stressed_index("rainha"), 1, "Syllabifier.stressed_index");

    if (failures) {
        std::cerr << failures << " example(s) failed\n";
        return 1;
    }
    std::cout << "examples: all passed\n";
    return 0;
}
