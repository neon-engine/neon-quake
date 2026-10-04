#ifndef QUAKE_LIGHTMAP_ATLAS_BLOCK_HPP
#define QUAKE_LIGHTMAP_ATLAS_BLOCK_HPP

#include <cstdint>

namespace quake
{
  /// Where the lightmap of a face of a level was put in the lightmap atlas.
  ///
  /// It names the samples of the face alone. The border around them, which
  /// repeats the outermost samples, lies outside of it.
  struct LightmapAtlasBlock
  {
    /// Which page of the atlas it is on.
    std::uint32_t page = 0;

    /// The pixel of the page its first sample is at, counted from the left
    /// and from the top.
    std::uint32_t x = 0;
    std::uint32_t y = 0;

    /// How many pixels it is wide and high, which is the size of the
    /// lightmap of the face. A face without a lightmap has the one pixel in
    /// the middle of the fully bright block.
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    /// Whether these are samples of the face. When not, the face has no
    /// lightmap, and shares the fully bright block with all others like it.
    bool is_lit = false;
  };
} // quake

#endif //QUAKE_LIGHTMAP_ATLAS_BLOCK_HPP
