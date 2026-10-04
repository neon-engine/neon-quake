#ifndef QUAKE_PICTURE_HPP
#define QUAKE_PICTURE_HPP

#include <cstdint>
#include <vector>

namespace quake
{
  /// A picture of the menus or of the status bar, as a `.lmp` file or a lump
  /// of `gfx.wad` holds it: so many pixels wide, so many high, and one byte
  /// for each pixel, the number of a colour of the palette.
  ///
  /// The pixels go row after row from the top, each row from the left. A
  /// picture of the menus has holes where the colour is
  /// `Palette::see_through`; `Palette::ToRgba()` makes of the pixels what
  /// the engine draws.
  struct Picture
  {
    /// The most pixels a side may have. No picture of the game comes near
    /// it, the largest being 320 by 200; a file that says more is taken to
    /// be broken rather than believed.
    static constexpr std::int32_t max_side = 16384;

    std::int32_t width = 0;
    std::int32_t height = 0;

    /// `width` times `height` numbers of colours.
    std::vector<std::uint8_t> pixels;
  };
} // quake

#endif //QUAKE_PICTURE_HPP
