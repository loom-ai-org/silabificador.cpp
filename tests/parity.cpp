// Byte parity with the Python silabificador. Each golden line holds an input
// and Python's output for every public function; this recomputes the line in
// C++ and compares the bytes.
#include <iostream>
#include <stdexcept>
#include <string>

#include "json_line.hpp"
#include "silabificador/silabificador.hpp"

namespace sb = silabificador;
using json_line::quote;

namespace {
const char* boolean(bool value) { return value ? "true" : "false"; }

std::string record(const std::string& word) {
    // The cleaned word, recovered the way analyze() sees it: the syllables of a
    // word always concatenate back to it.
    const auto syllables = sb::analyze(word);
    std::string cleaned;
    std::vector<std::string> surfaces;
    for (const auto& s : syllables) {
        cleaned += s.surface;
        surfaces.push_back(s.surface);
    }

    std::string out = "[" + quote(word) + ",[";
    for (size_t i = 0; i < syllables.size(); ++i) {
        const auto& s = syllables[i];
        if (i) out += ",";
        out += "[" + quote(s.onset) + "," + quote(s.glide_on) + "," + quote(s.nucleus) + "," +
               quote(s.glide_off) + "," + quote(s.coda) + "," + quote(s.separator) + "," +
               quote(s.surface) + "," + boolean(s.stressed) + "," + boolean(s.secondary) + "]";
    }
    out += "]," + std::to_string(sb::stressed_index(word)) + ",[";

    const auto units = sb::graphemes::tokenize(cleaned);
    for (size_t i = 0; i < units.size(); ++i) {
        if (i) out += ",";
        out += "[" + quote(units[i].text) + "," + quote(sb::graphemes::kind_name(units[i].kind)) + "]";
    }
    out += "],[";
    bool first = true;
    for (int b : sb::morphology::boundaries(cleaned, units)) {
        if (!first) out += ",";
        first = false;
        out += std::to_string(b);
    }
    out += "],";
    try {
        out += std::to_string(sb::stress::stressed_index(surfaces, cleaned));
    } catch (const std::out_of_range&) {
        out += "\"IndexError\"";
    }
    return out + "]";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: silabificador_parity <goldens dir>\n";
        return 2;
    }
    const auto lines = json_line::read_gz_lines(std::string(argv[1]) + "/syllables.jsonl.gz");
    if (lines.empty()) {
        std::cerr << "empty golden file\n";
        return 1;
    }
    const long expected = json_line::header_count(lines[0]);
    const std::string version = json_line::header_version(lines[0]);
    const bool version_ok = version == sb::kPythonReference;
    if (!version_ok) {
        std::cerr << "goldens are from silabificador " << version << ", the port names "
                  << sb::kPythonReference << "\n";
    }

    // Lines are grouped by corpus in header order; a header without corpora
    // counts every line under one name.
    auto corpora = json_line::header_corpora(lines[0]);
    if (corpora.empty()) corpora.emplace_back("all", expected);
    long owned = 0;
    for (const auto& corpus : corpora) owned += corpus.second;

    long checked = 0, mismatches = 0;
    size_t corpus = 0;
    long corpus_left = corpora[0].second, corpus_mismatches = 0;
    auto close_corpus = [&] {
        std::cout << "  " << corpora[corpus].first << ": " << corpora[corpus].second << " lines, "
                  << corpus_mismatches << " mismatches\n";
        corpus_mismatches = 0;
    };
    for (size_t i = 1; i < lines.size(); ++i) {
        while (corpus_left == 0 && corpus + 1 < corpora.size()) {
            close_corpus();
            corpus_left = corpora[++corpus].second;
        }
        --corpus_left;
        size_t pos = 1;  // past the opening '['
        const std::string word = json_line::unquote(lines[i], pos);
        const std::string got = record(word);
        ++checked;
        if (got != lines[i]) {
            ++corpus_mismatches;
            if (++mismatches <= 20) {
                std::cerr << "MISMATCH\n  python: " << lines[i] << "\n  c++:    " << got << "\n";
            }
        }
    }
    while (corpus < corpora.size()) {
        close_corpus();
        ++corpus;
    }
    std::cout << "goldens: " << expected << " in, " << checked << " checked, " << mismatches
              << " mismatches\n";
    // A short, empty or mislabelled golden fails as surely as a wrong line.
    const bool counts_ok = expected > 0 && checked == expected && owned == expected;
    if (!counts_ok) std::cerr << "golden count check failed\n";
    return (version_ok && counts_ok && mismatches == 0) ? 0 : 1;
}
