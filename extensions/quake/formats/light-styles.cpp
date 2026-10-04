#include "light-styles.hpp"

#include <algorithm>
#include <cmath>

namespace quake
{
  // The texts of the styles a mapper chooses from, as the game code of the
  // original gives them when a level starts.
  namespace
  {
    constexpr std::array<std::string_view, 12> default_texts = {
      // 0: the light that stays as it is
      "m",
      // 1: a flicker
      "mmnmmommommnonmmonqnmmo",
      // 2: a slow and strong pulse
      "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
      // 3: a candle
      "mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
      // 4: a fast strobe
      "mamamamamama",
      // 5: a gentle pulse
      "jklmnopqrstuvwxyzyxwvutsrqponmlkj",
      // 6: another flicker
      "nmonqnmomnmomomno",
      // 7: another candle
      "mmmaaaabcdefgmmmmaaaammmaamm",
      // 8: a third candle
      "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
      // 9: a slow strobe
      "aaaaaaaazzzzzzzz",
      // 10: a tube that flickers
      "mmamammmmammamamaaamammma",
      // 11: a slow pulse that does not go dark for long
      "abcdefghijklmnopqrrqponmlkjihgfedcba",
    };

    const std::string no_text;
  }

  LightStyles::LightStyles()
  {
    for (std::size_t style = 0; style < default_texts.size(); style++) { _texts[style] = default_texts[style]; }
  }

  bool LightStyles::Set(const std::size_t style, const std::string_view text)
  {
    if (style >= count) { return false; }

    _texts[style] = text;
    return true;
  }

  const std::string &LightStyles::GetText(const std::size_t style) const
  {
    return style < count ? _texts[style] : no_text;
  }

  float LightStyles::GetValueOfLetter(const char letter)
  {
    const int steps = std::clamp(letter - 'a', 0, 'z' - 'a');
    return static_cast<float>(steps) / static_cast<float>('m' - 'a');
  }

  std::size_t LightStyles::GetLetterAt(const std::size_t length, const double time)
  {
    if (length == 0 || !(time > 0.0)) { return 0; }

    // the rest of a division of numbers with fractions, so that a time of
    // any length stays a number that can be counted with
    const double step = std::floor(time * letters_per_second);
    const double letter = std::fmod(step, static_cast<double>(length));
    return std::isfinite(letter) ? static_cast<std::size_t>(letter) : 0;
  }

  float LightStyles::GetValue(const std::size_t style, const double time) const
  {
    if (style >= count || _texts[style].empty()) { return 1.0f; }

    const std::string &text = _texts[style];
    return GetValueOfLetter(text[GetLetterAt(text.size(), time)]);
  }

  LightStyles::Values LightStyles::GetValues(const double time) const
  {
    Values values{};
    for (std::size_t style = 0; style < count; style++) { values[style] = GetValue(style, time); }
    return values;
  }
} // quake
