#ifndef QUAKE_HUD_PICTURE_TEST_HPP
#define QUAKE_HUD_PICTURE_TEST_HPP

#include <cstdint>
#include <ostream>
#include <string_view>
#include <vector>

#include "hud-picture.hpp"

namespace quake
{
  /// How a test that fails writes a `HudPicture`.
  inline void PrintTo(const HudPicture &picture, std::ostream *stream)
  {
    if (picture.kind == HudPictureKind::Character) { *stream << "letter " << picture.character; }
    else { *stream << picture.name; }
    *stream << " at " << picture.x << ", " << picture.y
      << (picture.anchor == HudAnchor::Bottom ? " (bottom)" : " (centre)");
  }

  /// A picture at a place, for what a test expects.
  [[nodiscard]] inline HudPicture MakePicture(
    const std::string_view name,
    const std::int32_t x,
    const std::int32_t y,
    const HudAnchor anchor = HudAnchor::Bottom)
  {
    return {.name = name, .x = x, .y = y, .anchor = anchor};
  }

  /// A letter at a place, for what a test expects.
  [[nodiscard]] inline HudPicture MakeLetter(
    const std::int32_t character,
    const std::int32_t x,
    const std::int32_t y,
    const HudAnchor anchor = HudAnchor::Bottom)
  {
    return {
      .kind = HudPictureKind::Character,
      .name = HudPicture::characters_name,
      .character = character,
      .x = x,
      .y = y,
      .anchor = anchor,
    };
  }

  /// The letters of a line of text from a place on, spaces left out, for
  /// what a test expects.
  [[nodiscard]] inline std::vector<HudPicture> MakeLetters(
    const std::string_view text,
    const std::int32_t x,
    const std::int32_t y,
    const HudAnchor anchor = HudAnchor::Bottom)
  {
    std::vector<HudPicture> letters;
    for (std::size_t i = 0; i < text.size(); i++)
    {
      if (text[i] == ' ') { continue; }

      letters.push_back(
        MakeLetter(static_cast<unsigned char>(text[i]), x + static_cast<std::int32_t>(i) * 8, y, anchor));
    }
    return letters;
  }
} // quake

#endif //QUAKE_HUD_PICTURE_TEST_HPP
