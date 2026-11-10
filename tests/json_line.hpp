// Just enough JSON to compare against Python's
// json.dumps(value, ensure_ascii=False, separators=(",", ":")) byte for byte.
#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

namespace json_line {

inline std::string quote(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out + "\"";
}

inline void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Decode the JSON string literal starting at line[pos] (which must be '"').
// Advances pos past the closing quote.
inline std::string unquote(const std::string& line, size_t& pos) {
    if (line.at(pos) != '"') throw std::runtime_error("expected a string at " + std::to_string(pos));
    std::string out;
    ++pos;
    auto hex4 = [&](size_t at) { return static_cast<unsigned>(std::stoul(line.substr(at, 4), nullptr, 16)); };
    while (line.at(pos) != '"') {
        char c = line[pos++];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        char e = line.at(pos++);
        switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned cp = hex4(pos);
                pos += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && line.compare(pos, 2, "\\u") == 0) {
                    unsigned low = hex4(pos + 2);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    pos += 6;
                }
                append_utf8(out, cp);
                break;
            }
            default:
                throw std::runtime_error("bad escape");
        }
    }
    ++pos;
    return out;
}

// Read every line of a gzip-compressed text file.
inline std::vector<std::string> read_gz_lines(const std::string& path) {
    gzFile file = gzopen(path.c_str(), "rb");
    if (file == nullptr) throw std::runtime_error("cannot open " + path);
    std::vector<std::string> lines;
    std::string current;
    char buffer[1 << 16];
    int n;
    while ((n = gzread(file, buffer, sizeof buffer)) > 0) {
        for (int i = 0; i < n; ++i) {
            if (buffer[i] == '\n') {
                lines.push_back(std::move(current));
                current.clear();
            } else {
                current.push_back(buffer[i]);
            }
        }
    }
    gzclose(file);
    if (!current.empty()) lines.push_back(std::move(current));
    return lines;
}

// A parsed JSON value: enough for the golden files (no floats, no objects).
struct Value {
    enum class Kind { Null, Bool, Int, String, Array } kind = Kind::Null;
    bool boolean = false;
    long integer = 0;
    std::string string;
    std::vector<Value> items;

    const Value& operator[](size_t i) const { return items.at(i); }
    bool is_null() const { return kind == Kind::Null; }
};

inline Value parse_value(const std::string& line, size_t& pos) {
    Value v;
    const char c = line.at(pos);
    if (c == '"') {
        v.kind = Value::Kind::String;
        v.string = unquote(line, pos);
    } else if (c == '[') {
        v.kind = Value::Kind::Array;
        ++pos;
        if (line.at(pos) == ']') {
            ++pos;
            return v;
        }
        while (true) {
            v.items.push_back(parse_value(line, pos));
            if (line.at(pos) == ',') {
                ++pos;
            } else if (line.at(pos) == ']') {
                ++pos;
                return v;
            } else {
                throw std::runtime_error("bad array at " + std::to_string(pos));
            }
        }
    } else if (line.compare(pos, 4, "true") == 0) {
        v.kind = Value::Kind::Bool;
        v.boolean = true;
        pos += 4;
    } else if (line.compare(pos, 5, "false") == 0) {
        v.kind = Value::Kind::Bool;
        pos += 5;
    } else if (line.compare(pos, 4, "null") == 0) {
        pos += 4;
    } else {
        size_t used = 0;
        v.kind = Value::Kind::Int;
        v.integer = std::stol(line.substr(pos, 24), &used);
        pos += used;
    }
    return v;
}

inline Value parse(const std::string& line) {
    size_t pos = 0;
    return parse_value(line, pos);
}

// The leading "count" of a golden header line such as {"...","count":87091}.
inline long header_count(const std::string& header) {
    size_t at = header.rfind("\"count\":");
    if (at == std::string::npos) throw std::runtime_error("golden header has no count");
    return std::stol(header.substr(at + 8));
}

// The silabificador release a golden header names: {"versions":{"silabificador":"2.1.7a1"},...}.
inline std::string header_version(const std::string& header) {
    const std::string key = "\"silabificador\":";
    size_t at = header.find(key);
    if (at == std::string::npos) throw std::runtime_error("golden header has no silabificador version");
    size_t pos = at + key.size();
    return unquote(header, pos);
}

// The corpora of a golden header, in file order, with the number of lines each
// owns: "corpora":[["gold_sample",500],...]. Empty when the header has none.
inline std::vector<std::pair<std::string, long>> header_corpora(const std::string& header) {
    std::vector<std::pair<std::string, long>> corpora;
    const std::string key = "\"corpora\":";
    size_t at = header.find(key);
    if (at == std::string::npos) return corpora;
    size_t pos = at + key.size();
    for (const Value& item : parse_value(header, pos).items) corpora.emplace_back(item[0].string, item[1].integer);
    return corpora;
}

}  // namespace json_line
