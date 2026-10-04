#include "hud-text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace quake
{
  // The names of the pictures of the large digits.
  namespace
  {
    constexpr std::array<std::string_view, 10> white_digits = {
      "num_0", "num_1", "num_2", "num_3", "num_4", "num_5", "num_6", "num_7", "num_8", "num_9",
    };
    constexpr std::array<std::string_view, 10> red_digits = {
      "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5", "anum_6", "anum_7", "anum_8", "anum_9",
    };
  }

  void HudText::AddLine(
    std::vector<HudPicture> &pictures,
    const std::string_view text,
    const std::int32_t x,
    const std::int32_t y,
    const HudAnchor anchor,
    const bool in_bronze)
  {
    std::int32_t at = x;
    for (const char letter : text)
    {
      // a space is no letter, in white or in bronze
      const bool is_space = letter == ' ';
      std::int32_t character = static_cast<unsigned char>(letter);
      if (in_bronze && character < bronze) { character += bronze; }

      if (!is_space)
      {
        pictures.push_back({
          .kind = HudPictureKind::Character,
          .name = HudPicture::characters_name,
          .character = character,
          .x = at,
          .y = y,
          .anchor = anchor,
        });
      }
      at += HudPicture::character_size;
    }
  }

  void HudText::AddNumber(
    std::vector<HudPicture> &pictures,
    const std::int32_t number,
    const std::int32_t x,
    const std::int32_t y,
    const HudAnchor anchor,
    const bool in_red)
  {
    constexpr std::size_t field = 3;
    const std::string digits = std::to_string(std::clamp(number, -99, largest_number));

    std::int32_t at = x + static_cast<std::int32_t>(field - digits.size()) * digit_size;
    for (const char digit : digits)
    {
      const std::string_view name =
        digit == '-' ? (in_red ? "anum_minus" : "num_minus") : GetDigitName(digit - '0', in_red);
      pictures.push_back({.name = name, .x = at, .y = y, .anchor = anchor});
      at += digit_size;
    }
  }

  std::string_view HudText::GetDigitName(const std::int32_t digit, const bool in_red)
  {
    if (digit < 0 || digit > 9) { return {}; }

    return (in_red ? red_digits : white_digits)[static_cast<std::size_t>(digit)];
  }
} // quake
