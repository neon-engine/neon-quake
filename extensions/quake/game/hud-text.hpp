#ifndef QUAKE_HUD_TEXT_HPP
#define QUAKE_HUD_TEXT_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "hud-anchor.hpp"
#include "hud-picture.hpp"

namespace quake
{
  /// Writes text and numbers as `HudPicture`s, added to a list.
  ///
  /// A text of the game is bytes, and each byte is a letter of the sheet
  /// `conchars`. From 32 to 127 they are the letters of ASCII in white.
  /// The same with 128 added are those letters again in a brownish tone,
  /// "bronze", which the game uses to make a word stand out. Under 32 are
  /// signs of the game, among them small yellow digits from 18 on.
  struct HudText
  {
    /// What is added to a letter to get it in bronze.
    static constexpr std::int32_t bronze = 128;

    /// The first of the ten small yellow digits of the sheet.
    static constexpr std::int32_t small_digits = 18;

    /// How many pixels wide and high a digit of the large numbers is.
    static constexpr std::int32_t digit_size = 24;

    /// The largest number the large digits show: three digits.
    static constexpr std::int32_t largest_number = 999;

    /// Adds one line of text, its first letter at x, y, and each next one
    /// 8 pixels to the right. With `in_bronze`, every letter under 128 is
    /// drawn from the bronze set.
    ///
    /// A space takes its room and is not drawn, as in the original. A
    /// line break is no letter here: it is drawn as the sign the sheet
    /// has at its place, so a text of several lines is cut up first, as
    /// `CenterText` does.
    static void AddLine(
      std::vector<HudPicture> &pictures,
      std::string_view text,
      std::int32_t x,
      std::int32_t y,
      HudAnchor anchor,
      bool in_bronze = false);

    /// Adds a number in the large digits, `num_0` to `num_9`, or the red
    /// ones, `anum_0` to `anum_9`, in a field of three digits that starts
    /// at x: a shorter number is moved to the right end of the field, 24
    /// pixels a digit, with nothing drawn in front of it.
    ///
    /// A number over 999 is shown as 999, as the ports show it, and one
    /// under -99 as -99, with `num_minus` for the sign.
    static void AddNumber(
      std::vector<HudPicture> &pictures,
      std::int32_t number,
      std::int32_t x,
      std::int32_t y,
      HudAnchor anchor,
      bool in_red = false);

    /// The name of the picture of one large digit, from 0 to 9, white or
    /// red. Empty for anything else.
    [[nodiscard]] static std::string_view GetDigitName(std::int32_t digit, bool in_red = false);
  };
} // quake

#endif //QUAKE_HUD_TEXT_HPP
