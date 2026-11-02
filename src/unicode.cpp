#include <algorithm>

#include "detail.hpp"

namespace silabificador::detail {

namespace {
#include "unicode_tables.inc"

template <size_t N>
bool in_ranges(const char32_t (&table)[N][2], char32_t c) {
    size_t lo = 0, hi = N;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (c < table[mid][0]) {
            hi = mid;
        } else if (c > table[mid][1]) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

constexpr char32_t kCapitalSigma = 0x03A3;

// Python's handle_capital_sigma: Σ lowercases to final ς when it follows a
// cased letter (skipping case-ignorables) and no cased letter follows it.
char32_t lower_sigma(const U32& text, size_t i) {
    size_t j = i;
    char32_t c = 0;
    bool found = false;
    while (j > 0) {
        c = text[--j];
        if (!in_ranges(kCaseIgnorable, c)) {
            found = true;
            break;
        }
    }
    bool final_sigma = found && in_ranges(kCased, c);
    if (final_sigma && i + 1 < text.size()) {
        size_t k = i + 1;
        for (; k < text.size(); ++k) {
            c = text[k];
            if (!in_ranges(kCaseIgnorable, c)) break;
        }
        final_sigma = k == text.size() || !in_ranges(kCased, c);
    }
    return final_sigma ? 0x03C2 : 0x03C3;
}
}  // namespace

// Python's bytes.decode("utf-8", "replace"): each maximal subpart of an
// ill-formed sequence becomes one U+FFFD, and the byte that breaks a sequence
// is not consumed, so it starts the next one.
U32 from_utf8(std::string_view text);

}  // namespace silabificador::detail

namespace silabificador {

size_t code_points(std::string_view text) { return detail::from_utf8(text).size(); }

}  // namespace silabificador

namespace silabificador::detail {

U32 from_utf8(std::string_view text) {
    U32 out;
    out.reserve(text.size());
    const size_t n = text.size();
    auto byte = [&](size_t k) { return static_cast<unsigned char>(text[k]); };
    size_t i = 0;
    while (i < n) {
        const unsigned char b = byte(i);
        if (b < 0x80) {
            out.push_back(b);
            ++i;
            continue;
        }
        size_t need;
        char32_t cp;
        unsigned char lower = 0x80, upper = 0xBF;
        if (b >= 0xC2 && b <= 0xDF) {
            need = 1;
            cp = b & 0x1F;
        } else if (b >= 0xE0 && b <= 0xEF) {
            need = 2;
            cp = b & 0x0F;
            if (b == 0xE0) lower = 0xA0;
            if (b == 0xED) upper = 0x9F;
        } else if (b >= 0xF0 && b <= 0xF4) {
            need = 3;
            cp = b & 0x07;
            if (b == 0xF0) lower = 0x90;
            if (b == 0xF4) upper = 0x8F;
        } else {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        size_t j = i + 1;
        bool complete = true;
        for (size_t k = 0; k < need; ++k, ++j) {
            if (j >= n || byte(j) < lower || byte(j) > upper) {
                complete = false;
                break;
            }
            cp = (cp << 6) | (byte(j) & 0x3F);
            lower = 0x80;
            upper = 0xBF;
        }
        out.push_back(complete ? cp : 0xFFFD);
        i = j;
    }
    return out;
}

std::string to_utf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char32_t cp : text) {
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
    return out;
}

U32 py_strip(const U32& text) {
    size_t begin = 0, end = text.size();
    while (begin < end && in_ranges(kSpace, text[begin])) ++begin;
    while (end > begin && in_ranges(kSpace, text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

U32 py_lower(const U32& text) {
    U32 out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t c = text[i];
        if (c == kCapitalSigma) {
            out.push_back(lower_sigma(text, i));
            continue;
        }
        const auto* end = std::end(kLower);
        const auto* it = std::lower_bound(std::begin(kLower), end, c,
                                          [](const LowerEntry& e, char32_t v) { return e.cp < v; });
        if (it != end && it->cp == c) {
            out.append(it->to, it->len);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool contains(std::u32string_view set, char32_t c) {
    return set.find(c) != std::u32string_view::npos;
}

bool ends_with(std::u32string_view text, std::u32string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(std::u32string_view text, std::u32string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace silabificador::detail
