#include "center-text.hpp"

#include <algorithm>
#include <limits>

#include "hud-text.hpp"

namespace quake
{
  std::vector<HudPicture> CenterText::Layout(const std::string_view text, const float seconds_shown)
  {
    if (!(seconds_shown < hold_seconds)) { return {}; }

    return LayoutLetters(text, std::numeric_limits<std::size_t>::max());
  }

  std::vector<HudPicture> CenterText::LayoutRevealed(
    const std::string_view text,
    const float seconds_shown,
    const float speed)
  {
    // never more than the text has: a float that is too large, or no
    // number, is not to be cast
    const float wanted = speed * seconds_shown;
    std::size_t letters = 1;
    if (wanted >= static_cast<float>(text.size())) { letters = text.size(); }
    else if (wanted > 0.0f) { letters += static_cast<std::size_t>(wanted); }

    return LayoutLetters(text, letters);
  }

  std::vector<HudPicture> CenterText::LayoutLetters(const std::string_view text, const std::size_t letters)
  {
    const auto lines = 1 + std::ranges::count(text, '\n');
    std::int32_t y = lines <= 4 ? 70 : 48;

    std::vector<HudPicture> pictures;
    std::size_t remaining = letters;
    std::size_t start = 0;
    while (start <= text.size() && remaining > 0)
    {
      const std::size_t line_break = std::min(text.find('\n', start), text.size());
      const std::size_t length = std::min({line_break - start, line_length, remaining});

      // the line is put around the middle as a whole, also while only a
      // part of it shows
      const std::size_t whole = std::min(line_break - start, line_length);
      const std::int32_t x = (320 - static_cast<std::int32_t>(whole) * HudPicture::character_size) / 2;
      HudText::AddLine(pictures, text.substr(start, length), x, y, HudAnchor::Center);

      remaining -= length;
      y += HudPicture::character_size;
      start = line_break + 1;
    }
    return pictures;
  }
} // quake
