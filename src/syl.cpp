// Public entry points. The work is layered:
//   phonotactics  what Portuguese licenses (data only)
//   graphemes     orthography to grapheme units   (layer 1)
//   nucleus       nucleus and glide resolution    (layer 2)
//   morphology    morpheme boundaries
//   parser        syllable assembly               (layer 3)
//   stress        which syllable bears the stress
#include "detail.hpp"

namespace silabificador {

bool Syllable::operator==(const Syllable& other) const {
    return onset == other.onset && glide_on == other.glide_on && nucleus == other.nucleus &&
           glide_off == other.glide_off && coda == other.coda && separator == other.separator &&
           surface == other.surface && stressed == other.stressed && secondary == other.secondary;
}

std::vector<Syllable> analyze(std::string_view word) {
    const detail::U32 cleaned = detail::py_lower(detail::py_strip(detail::from_utf8(word)));
    return detail::assign(detail::parse(cleaned), cleaned);
}

std::vector<std::string> syllabify(std::string_view word) {
    std::vector<std::string> out;
    for (auto& syllable : analyze(word)) out.push_back(std::move(syllable.surface));
    return out;
}

int stressed_index(std::string_view word) {
    const auto syllables = analyze(word);
    for (size_t index = 0; index < syllables.size(); ++index) {
        if (syllables[index].stressed) return static_cast<int>(index);
    }
    return 0;
}

std::vector<std::string> Syllabifier::syllabify(std::string_view word) const {
    return silabificador::syllabify(word);
}

std::vector<Syllable> Syllabifier::analyze(std::string_view word) const {
    return silabificador::analyze(word);
}

int Syllabifier::stressed_index(std::string_view word) const {
    return silabificador::stressed_index(word);
}

}  // namespace silabificador
