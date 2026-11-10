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

    expect(sb::syllabify("fluiu"), Strings{"flu", "iu"}, "STRESSED-IU-UI");
    expect(sb::syllabify("distraiu"), Strings{"dis", "tra", "iu"}, "STRESSED-IU-UI after a");
    expect(sb::syllabify("pauis"), Strings{"pa", "uis"}, "STRESSED-IU-UI before s");
    expect(sb::syllabify("baiuca"), Strings{"bai", "u", "ca"}, "a paroxytone keeps its diphthong");
    expect(sb::syllabify("miudeza"), Strings{"mi", "u", "de", "za"}, "UNSTRESSED-IU");
    expect(sb::syllabify("cuidado"), Strings{"cui", "da", "do"}, "UNSTRESSED-IU leaves ui alone");
    expect(sb::syllabify("mãinha"), Strings{"mã", "i", "nha"}, "PRE-PALATAL outranks NASAL-OFFGLIDE");
    expect(sb::syllabify("xiita"), Strings{"xi", "i", "ta"}, "IDENTICAL-VOWELS");
    expect(sb::syllabify("sublinhar"), Strings{"sub", "li", "nhar"}, "sub + linh");
    expect(sb::syllabify("arduidade"), Strings{"ar", "du", "i", "da", "de"}, "-uo adjective before -idade");
    expect(sb::syllabify("contiguidade"), Strings{"con", "ti", "gui", "da", "de"}, "gu before -idade");
    expect(sb::syllabify("arruinado"), Strings{"ar", "ru", "i", "na", "do"}, "listed stem ru.in");
    expect(sb::syllabify("bureau"), Strings{"bu", "reau"}, "French eau");
    expect(sb::syllabify("boxeur"), Strings{"bo", "xeur"}, "French eu before a final r");
    expect(sb::syllabify("gourmet"), Strings{"gour", "met"}, "ou in a foreign word");
    expect(sb::syllabify("agüentar"), Strings{"a", "güen", "tar"}, "the trema u is in the onset");
    expect(sb::syllabify("kubitschek"), Strings{"ku", "bi", "tschek"}, "German tsch");
    expect(sb::syllabify("graffiti"), Strings{"graf", "fi", "ti"}, "a doubled ff splits");
    expect(sb::syllabify("guizhou"), Strings{"gui", "zhou"}, "pinyin zh");
    expect(sb::phonotactics::has_foreign_mark("gourmet"), true, "has_foreign_mark(gourmet)");
    expect(sb::phonotactics::has_foreign_mark("casa"), false, "has_foreign_mark(casa)");
    expect(sb::phonotactics::is_foreign_tetragraph("tsch"), true, "is_foreign_tetragraph(tsch)");
    expect(sb::phonotactics::is_foreign_digraph("ff"), false, "ff is not a foreign digraph");
    expect(sb::phonotactics::is_ao_diphthong_word("aos"), true, "is_ao_diphthong_word(aos)");
    expect(sb::phonotactics::is_ao_diphthong_word("caos"), false, "is_ao_diphthong_word(caos)");

    expect(sb::syllabify("saiu-se"), Strings{"sa", "iu-", "se"}, "an enclitic ends the verb");
    expect(sb::syllabify("saiu de casa"), Strings{"sa", "iu ", "de ", "ca", "sa"}, "a space ends the verb");
    expect(sb::syllabify("beaux-arts"), Strings{"beaux-", "arts"}, "eau before a final x of an element");
    expect(sb::syllabify("pau-d'água"), Strings{"pau-", "d'á", "gua"}, "the elided d is an onset");
    expect(sb::syllabify("sant'ana"), Strings{"san", "t'a", "na"}, "the elided t is an onset");
    expect(sb::analyze("pau-d'água")[1].onset, std::string("d"), "the onset holds no apostrophe");
    expect(sb::syllabify("ao pé"), Strings{"ao ", "pé"}, "CONTRACTION-AO");
    expect(sb::syllabify("aos poucos"), Strings{"aos ", "pou", "cos"}, "CONTRACTION-AO plural");
    expect(sb::syllabify("aorta"), Strings{"a", "or", "ta"}, "ao inside a word is a hiatus");
    expect(sb::syllabify("d'aos"), Strings{"d'a", "os"}, "an apostrophe does not end the word");

    expect(sb::stressed_index(""), 0, "an empty word returns 0");
    expect(sb::stressed_index("computador"), 3, "computador stress");
    expect(sb::stressed_index("casa"), 0, "casa stress");
    expect(sb::stressed_index("sílaba"), 0, "sílaba stress");

    expect(sb::stressed_index("vendeu-se"), 1, "an enclitic leaves the stress on the verb");
    expect(sb::stressed_index("deu-te-lo"), 0, "a chain of enclitics is skipped");
    expect(sb::stressed_index("no-lo"), 1, "a form of pronouns only keeps the last");
    expect(sb::stressed_index("dar-te-ei"), 2, "in tmesis the verb ending keeps the primary");
    expect(sb::stressed_index("pelos campos"), 2, "pelos is unstressed");
    expect(sb::stressed_index("frei luís"), 2, "frei is unstressed");
    expect(sb::stress::stressed_index({"re", "ve", "lou-", "m'o"}, "revelou-m'o"), 2,
           "stress::stressed_index skips an elided pronoun");
    expect(sb::stress::stressed_index({"pé-", "d'a", "lho"}, "pé-d'alho"), 1,
           "an apostrophe does not split an element");

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
