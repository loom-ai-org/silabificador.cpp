// Internal view of the phonotactic inventories, on code points.
#pragma once

#include <string_view>

#include "silabificador/silabificador.hpp"

namespace silabificador::phonotactics {

// Letters that can head a syllable nucleus.
inline constexpr std::u32string_view kVowels = U"aeiouáéíóúâêîôûàèìòùãõäëïöüy";
// The only vowels that can become glides; Portuguese has no mid or low glides.
inline constexpr std::u32string_view kGlideCapable = U"iuy";
// An accent on a close vowel pins it as a nucleus: pais against país.
inline constexpr std::u32string_view kStressMarked = U"áéíóúâêîôûàèìòù";
// A nasal nucleus licenses an offglide a plain vowel does not: mãe, pão, põe.
inline constexpr std::u32string_view kNasalVowels = U"ãõ";
inline constexpr std::u32string_view kConsonants = U"bcdfghjklmnpqrstvwxyzçñ";
inline constexpr std::u32string_view kObstruents = U"pbtdcgfv";
inline constexpr std::u32string_view kLiquids = U"lr";
// A close vowel closed by one of these is a nucleus: a.in.da, ca.ir, ju.iz.
inline constexpr std::u32string_view kHiatusCodas = U"lmnrz";

bool is_complex_onset(std::u32string_view cluster);
bool has_foreign_mark(std::u32string_view word);

}  // namespace silabificador::phonotactics
