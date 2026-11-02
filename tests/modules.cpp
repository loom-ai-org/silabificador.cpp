// Byte parity for every sub-module function called with arguments built by the
// golden dumper rather than produced by analyze(): odd units, empty texts,
// negative and descending indices, empty surfaces, and invalid UTF-8.
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "json_line.hpp"
#include "silabificador/silabificador.hpp"

namespace sb = silabificador;
using json_line::quote;
using json_line::Value;

namespace {
const char* boolean(bool value) { return value ? "true" : "false"; }

sb::graphemes::Kind kind_of(const std::string& name) {
    if (name == "vowel") return sb::graphemes::Kind::Vowel;
    if (name == "consonant") return sb::graphemes::Kind::Consonant;
    if (name == "separator") return sb::graphemes::Kind::Separator;
    throw std::runtime_error("unknown kind " + name);
}

std::vector<sb::graphemes::Grapheme> units_of(const Value& v) {
    std::vector<sb::graphemes::Grapheme> out;
    for (const auto& item : v.items) out.push_back({item[0].string, kind_of(item[1].string)});
    return out;
}

std::string units_json(const std::vector<sb::graphemes::Grapheme>& units) {
    std::string out = "[";
    for (size_t i = 0; i < units.size(); ++i) {
        if (i) out += ",";
        out += "[" + quote(units[i].text) + "," + quote(sb::graphemes::kind_name(units[i].kind)) + "]";
    }
    return out + "]";
}

std::string ints_json(const std::vector<int>& values) {
    std::string out = "[";
    for (size_t i = 0; i < values.size(); ++i) out += (i ? "," : "") + std::to_string(values[i]);
    return out + "]";
}

std::string syllable_json(const sb::Syllable& s) {
    return "[" + quote(s.onset) + "," + quote(s.glide_on) + "," + quote(s.nucleus) + "," +
           quote(s.glide_off) + "," + quote(s.coda) + "," + quote(s.separator) + "," +
           quote(s.surface) + "," + boolean(s.stressed) + "," + boolean(s.secondary) + "]";
}

std::string syllables_json(const std::vector<sb::Syllable>& syllables) {
    std::string out = "[";
    for (size_t i = 0; i < syllables.size(); ++i) out += (i ? "," : "") + syllable_json(syllables[i]);
    return out + "]";
}

sb::Syllable syllable_of_fields(const Value& v) {
    sb::Syllable s;
    s.onset = v[0].string;
    s.glide_on = v[1].string;
    s.nucleus = v[2].string;
    s.glide_off = v[3].string;
    s.coda = v[4].string;
    s.separator = v[5].string;
    s.surface = v[6].string;
    s.stressed = v[7].boolean;
    s.secondary = v[8].boolean;
    return s;
}

std::string from_hex(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

template <typename Call>
std::string guarded(Call call) {
    try {
        return call();
    } catch (const std::out_of_range&) {
        return quote("IndexError");
    }
}

// Recompute the golden line for one call; the last element is the result.
std::string replay(const Value& v) {
    const std::string& name = v[0].string;
    std::string head = "[" + quote(name) + ",";
    if (name == "resolve") {
        const auto units = units_of(v[1]);
        const int start = static_cast<int>(v[2].integer), end = static_cast<int>(v[3].integer);
        return head + units_json(units) + "," + std::to_string(start) + "," + std::to_string(end) + "," +
               guarded([&] {
                   std::string out = "[";
                   const auto groups = sb::nucleus::resolve(units, start, end);
                   for (size_t i = 0; i < groups.size(); ++i) out += (i ? "," : "") + ints_json(groups[i]);
                   return out + "]";
               }) + "]";
    }
    if (name == "syllable_of") {
        const auto units = units_of(v[1]);
        std::vector<int> group;
        for (const auto& item : v[2].items) group.push_back(static_cast<int>(item.integer));
        const bool none = v[2].is_null();
        return head + units_json(units) + "," + (none ? std::string("null") : ints_json(group)) + "," +
               guarded([&] { return syllable_json(sb::syllable_of(units, none ? nullptr : &group)); }) + "]";
    }
    if (name == "assign") {
        std::vector<sb::Syllable> syllables;
        for (const auto& item : v[1].items) syllables.push_back(syllable_of_fields(item));
        return head + syllables_json(syllables) + "," + quote(v[2].string) + "," +
               syllables_json(sb::stress::assign(syllables, v[2].string)) + "]";
    }
    if (name == "stressed_index") {
        std::vector<std::string> strings;
        std::string list = "[";
        for (size_t i = 0; i < v[1].items.size(); ++i) {
            strings.push_back(v[1][i].string);
            list += (i ? "," : "") + quote(strings.back());
        }
        return head + list + "]," + quote(v[2].string) + "," +
               guarded([&] { return std::to_string(sb::stress::stressed_index(strings, v[2].string)); }) + "]";
    }
    if (name == "boundaries") {
        const auto units = units_of(v[2]);
        std::vector<int> found;
        for (int b : sb::morphology::boundaries(v[1].string, units)) found.push_back(b);
        return head + quote(v[1].string) + "," + units_json(units) + "," + ints_json(found) + "]";
    }
    if (name == "parse") {
        return head + quote(v[1].string) + "," + syllables_json(sb::parser::parse(v[1].string)) + "]";
    }
    if (name == "tokenize") {
        return head + quote(v[1].string) + "," + units_json(sb::graphemes::tokenize(v[1].string)) + "]";
    }
    if (name == "is_vowel") {
        return head + quote(v[1].string) + "," + boolean(sb::phonotactics::is_vowel(v[1].string)) + "]";
    }
    if (name == "is_complex_onset") {
        return head + quote(v[1].string) + "," + boolean(sb::phonotactics::is_complex_onset(v[1].string)) + "]";
    }
    if (name == "decode") {
        return head + quote(v[1].string) + "," + units_json(sb::graphemes::tokenize(from_hex(v[1].string))) + "]";
    }
    throw std::runtime_error("unknown call " + name);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: silabificador_modules <goldens dir>\n";
        return 2;
    }
    const auto lines = json_line::read_gz_lines(std::string(argv[1]) + "/modules.jsonl.gz");
    if (lines.empty()) {
        std::cerr << "empty golden file\n";
        return 1;
    }
    const long expected = json_line::header_count(lines[0]);
    long checked = 0, mismatches = 0;
    std::map<std::string, long> by_call;
    for (size_t i = 1; i < lines.size(); ++i) {
        const Value call = json_line::parse(lines[i]);
        const std::string got = replay(call);
        ++checked;
        if (got != lines[i]) {
            ++by_call[call[0].string];
            if (++mismatches <= 20) {
                std::cerr << "MISMATCH\n  python: " << lines[i] << "\n  c++:    " << got << "\n";
            }
        }
    }
    for (const auto& [name, count] : by_call) std::cout << "  " << name << ": " << count << " mismatches\n";
    std::cout << "module goldens: " << expected << " in, " << checked << " checked, " << mismatches
              << " mismatches\n";
    return (checked == expected && mismatches == 0) ? 0 : 1;
}
