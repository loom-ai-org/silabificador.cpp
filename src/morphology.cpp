// Morpheme boundaries, which outrank phonotactics, and which are lexical.
//
// The dictionaries write sub.li.mi.nar but su.bli.me, dis.tri.bu.i.dor but
// cui.da.do: same shapes, opposite answers. What separates them is whether the
// prefix or stem is still a live morpheme, a fact about the word's history that
// no string rule can see. Entries are morphemes, never syllabifications; each
// generalizes to its derivatives and is a falsifiable etymological claim. The
// one place that states syllable boundaries is data/stems.csv, and only inside a
// stem: the boundaries a stem inherits from its base.
//
// References: Cunha & Cintra (1984), "Prefixos"; Rio-Torto (2016), Gramática
// Derivacional do Português; Mateus & d'Andrade (1998), "The syllable structure
// in European Portuguese", pp. 20-27.
#include <array>
#include <map>
#include <stdexcept>
#include <utility>

#include "detail.hpp"
#include "stems_csv.inc"

namespace silabificador {
namespace detail {

namespace {
// Consonant-final Latinate prefixes, with the Latin stems that keep them live.
// Only a boundary before a liquid is recorded: there a consonant-final prefix
// would otherwise form a false complex onset (su.bli.mi.nar). Ordered longest
// prefix first, the order the Python code tries them in. Every stem has a source
// that writes the prefix boundary: AO90 Base XX 1 names the class (ab-legação,
// ad-ligar, sub-lunar), and the dictionaries give the members (sub.li.nhar,
// sub.li.ne.ar, sub.lu.xa.ção). obliquar is absent: every source writes o.bli.quar.
struct PrefixStems {
    std::u32string_view prefix;
    std::initializer_list<std::u32string_view> stems;
};

const std::array<PrefixStems, 4>& latinate_prefix_stems() {
    static const std::array<PrefixStems, 4> table{{
        {U"sub", {U"limin", U"loc", U"lig", U"lunar", U"lacustre", U"lamelar", U"lenhoso",
                  U"licenc", U"licenç", U"linh", U"linear", U"lux"}},
        {U"ab", {U"leg", U"loc", U"rog"}},
        {U"ad", {U"leg", U"lig"}},
        {U"ob", {U"rog"}},
    }};
    return table;
}

// Verb stems ending in a vowel before a suffix opening with i: the seam is a
// hiatus however much it looks like a diphthong (dis.tri.bu.i.dor,
// des.tru.i.ção, a.ju.i.zar). Matched anywhere, so prefixed derivatives are
// covered. Short stems that would seize unrelated words (us, flu) are excluded.
constexpr std::array<std::u32string_view, 26> kVowelFinalStems{
    // -uir verbs (Latin -uere) and everything derived from them.
    U"distribu", U"contribu", U"retribu", U"atribu", U"destru", U"constru", U"instru",
    U"substitu", U"constitu", U"restitu", U"institu", U"prostitu",
    U"polu", U"dilu", U"inclu", U"conclu", U"exclu", U"diminu", U"imbu", U"possu",
    U"continu",
    // -air verbs (Latin -ahere) and their nominalizations.
    U"abstra", U"distra", U"subtra", U"retra",
    // juízo, juiz and their family; safe only because the seam needs a following i.
    U"ju",
};

size_t prefix_boundary(const U32& word) {
    for (const auto& entry : latinate_prefix_stems()) {
        if (!starts_with(word, entry.prefix)) continue;
        std::u32string_view stem = std::u32string_view(word).substr(entry.prefix.size());
        if (!stem.empty() && (stem[0] == U'l' || stem[0] == U'r')) {
            for (auto s : entry.stems) {
                if (starts_with(stem, s)) return entry.prefix.size();
            }
        }
        return 0;
    }
    return 0;
}

// Nouns in -idade from adjectives in -uo and -ue keep the u of the base as a
// nucleus: ar.du.i.da.de, in.ge.nu.i.da.de, te.nu.i.da.de (AO90 Base XX 4,
// "flu-idez"). A u after g or q is in the onset, so con.ti.gui.da.de is left
// alone. This is the search of the Python pattern (?<=[^gq]u)idades?$, where $
// matches at the end of the word or before a newline that ends it.
size_t uo_idade(const U32& word) {
    for (std::u32string_view suffix : {U"idade", U"idades", U"idade\n", U"idades\n"}) {
        if (!ends_with(word, suffix)) continue;
        const size_t start = word.size() - suffix.size();
        if (start >= 2 && word[start - 1] == U'u' && word[start - 2] != U'g' && word[start - 2] != U'q') {
            return start;
        }
    }
    return 0;
}

size_t seam_boundary(const U32& word) {
    if (size_t suffix = uo_idade(word)) return suffix;
    for (auto stem : kVowelFinalStems) {
        size_t start = word.find(stem);
        if (start == U32::npos) continue;
        size_t end = start + stem.size();
        if (end < word.size() && word[end] == U'i' && word.size() > end + 1) return end;
    }
    return 0;
}

// One field of a CSV row, read as Python's csv module reads the excel dialect:
// a quoted field may hold commas and doubled quotes.
U32 csv_field(std::string_view text, size_t& pos) {
    std::string field;
    bool quoted = false;
    while (pos < text.size()) {
        const char c = text[pos];
        if (quoted) {
            if (c == '"' && pos + 1 < text.size() && text[pos + 1] == '"') {
                field.push_back('"');
                pos += 2;
                continue;
            }
            if (c == '"') quoted = false;
            else field.push_back(c);
        } else if (c == '"' && field.empty()) {
            quoted = true;
        } else if (c == ',' || c == '\n' || c == '\r') {
            break;
        } else {
            field.push_back(c);
        }
        ++pos;
    }
    return from_utf8(field);
}

std::vector<std::vector<U32>> csv_rows(std::string_view text) {
    std::vector<std::vector<U32>> rows;
    size_t pos = 0;
    while (pos < text.size()) {
        if (text[pos] == '\n' || text[pos] == '\r') {
            ++pos;  // a blank row, or the \n of \r\n
            continue;
        }
        std::vector<U32> row{csv_field(text, pos)};
        while (pos < text.size() && text[pos] == ',') row.push_back(csv_field(text, ++pos));
        rows.push_back(std::move(row));
    }
    return rows;
}

// Stems whose syllable boundaries no rule reaches, read from data/stems.csv,
// which the build embeds verbatim. Each stem maps to the code-point offsets where
// a syllable begins inside it: every dot of its split. Most rows are a hiatus a
// derivative inherits from a base with an accented hiatus (ruína: ar.ru.i.na.do);
// the glides of the falling diphthongs "are lexically marked" (Mateus &
// d'Andrade 1998, p. 27), so the hiatus is a fact about the stem.
const std::map<U32, std::vector<size_t>>& listed_stems() {
    static const std::map<U32, std::vector<size_t>> listed = [] {
        const auto rows = csv_rows(kStemsCsv);
        if (rows.empty()) throw std::logic_error("data/stems.csv has no header");
        size_t stem_column = rows[0].size(), split_column = rows[0].size();
        for (size_t c = 0; c < rows[0].size(); ++c) {
            if (rows[0][c] == U"stem") stem_column = c;
            if (rows[0][c] == U"split") split_column = c;
        }
        std::map<U32, std::vector<size_t>> table;
        for (size_t r = 1; r < rows.size(); ++r) {
            const auto& row = rows[r];
            if (stem_column >= row.size() || split_column >= row.size()) {
                throw std::logic_error("data/stems.csv row without a stem or a split");
            }
            std::vector<size_t> offsets;
            size_t length = 0;
            for (char32_t c : row[split_column]) {
                if (c == U'.') offsets.push_back(length);
                else ++length;
            }
            table[row[stem_column]] = std::move(offsets);
        }
        return table;
    }();
    return listed;
}

std::set<size_t> listed_boundaries(const U32& word) {
    std::set<size_t> found;
    for (const auto& [stem, offsets] : listed_stems()) {
        const size_t start = word.find(stem);
        if (start == U32::npos) continue;
        for (size_t offset : offsets) found.insert(start + offset);
    }
    return found;
}
}  // namespace

std::set<int> boundaries(const U32& word, const std::vector<Unit>& units) {
    std::set<size_t> offsets = listed_boundaries(word);
    for (size_t offset : {prefix_boundary(word), seam_boundary(word)}) {
        if (offset != 0) offsets.insert(offset);
    }
    std::set<int> found;
    if (offsets.empty()) return found;

    size_t position = 0;
    for (size_t index = 0; index < units.size(); ++index) {
        if (offsets.count(position)) found.insert(static_cast<int>(index));
        position += units[index].text.size();
    }
    return found;
}

}  // namespace detail

namespace morphology {
std::set<int> boundaries(std::string_view word, const std::vector<graphemes::Grapheme>& units) {
    return detail::boundaries(detail::from_utf8(word), detail::to_units(units));
}
}  // namespace morphology

}  // namespace silabificador
