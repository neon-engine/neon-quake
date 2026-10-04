#ifndef QUAKE_LIGHTMAP_ATLAS_PAGE_HPP
#define QUAKE_LIGHTMAP_ATLAS_PAGE_HPP

#include <cstdint>
#include <vector>

namespace quake
{
  /// One picture of a lightmap atlas, which holds the lightmaps of many
  /// faces of a level next to each other.
  struct LightmapAtlasPage
  {
    /// How many pixels it is wide and high.
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    /// Its pixels, row after row from the top, four bytes for each: red,
    /// green, and blue of the light, which are all the same when the level
    /// has no coloured light, and how solid it is, which is always 255.
    /// This is what the engine takes as a picture. A pixel no face uses is
    /// black.
    std::vector<std::uint8_t> pixels;
  };
} // quake

#endif //QUAKE_LIGHTMAP_ATLAS_PAGE_HPP
