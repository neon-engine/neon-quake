#include "palette.hpp"

#include <algorithm>

namespace quake
{
  bool Palette::Read(const std::span<const std::uint8_t> bytes)
  {
    if (bytes.size() != size_in_bytes) { return false; }

    std::ranges::copy(bytes, _colours.begin());
    return true;
  }

  std::array<std::uint8_t, 3> Palette::GetColour(const std::uint8_t index) const
  {
    const std::size_t at = static_cast<std::size_t>(index) * 3;
    return {_colours[at], _colours[at + 1], _colours[at + 2]};
  }

  std::vector<std::uint8_t> Palette::ToRgba(const std::span<const std::uint8_t> pixels, const bool holes) const
  {
    std::vector<std::uint8_t> rgba;
    rgba.reserve(pixels.size() * 4);

    for (const std::uint8_t pixel : pixels)
    {
      if (holes && pixel == see_through)
      {
        rgba.insert(rgba.end(), {0, 0, 0, 0});
        continue;
      }

      const auto colour = GetColour(pixel);
      rgba.insert(rgba.end(), {colour[0], colour[1], colour[2], 255});
    }
    return rgba;
  }

  std::vector<std::uint8_t> Palette::ToGlowRgba(const std::span<const std::uint8_t> pixels, const bool holes) const
  {
    const auto glows = [holes](const std::uint8_t pixel)
    {
      return pixel >= first_glowing && !(holes && pixel == see_through);
    };
    if (std::ranges::none_of(pixels, glows)) { return {}; }

    std::vector<std::uint8_t> rgba;
    rgba.reserve(pixels.size() * 4);
    for (const std::uint8_t pixel : pixels)
    {
      if (!glows(pixel))
      {
        rgba.insert(rgba.end(), {0, 0, 0, 0});
        continue;
      }

      const auto colour = GetColour(pixel);
      rgba.insert(rgba.end(), {colour[0], colour[1], colour[2], 255});
    }
    return rgba;
  }
} // quake
