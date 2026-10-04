#ifndef QUAKE_SPRITE_PICTURE_HPP
#define QUAKE_SPRITE_PICTURE_HPP

#include <cstdint>
#include <vector>

namespace quake
{
  /// One picture of a sprite, with where it hangs on the origin of its
  /// entity. A pixel is as large as a unit of the game.
  struct SpritePicture
  {
    /// How far right of the origin the left edge is, which is negative for
    /// a picture that is around its origin.
    std::int32_t left = 0;

    /// How far above the origin the top edge is.
    std::int32_t up = 0;

    std::int32_t width = 0;
    std::int32_t height = 0;

    /// One byte for each pixel, the number of a colour of the palette, row
    /// after row from the top. The last colour, `Palette::see_through`,
    /// stands for nothing being there: `Palette::ToRgba` with holes makes
    /// four bytes of each.
    std::vector<std::uint8_t> pixels;
  };
} // quake

#endif //QUAKE_SPRITE_PICTURE_HPP
