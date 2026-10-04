#ifndef QUAKE_LIGHTMAP_ATLAS_VERTEX_HPP
#define QUAKE_LIGHTMAP_ATLAS_VERTEX_HPP

#include <cstdint>

namespace quake
{
  /// Where a vertex of the mesh of a level lies in the lightmap atlas.
  struct LightmapAtlasVertex
  {
    /// Where it lies on its page: 0 is the left or the top border of the
    /// picture, 1 the right or the bottom border.
    float u = 0.0f;
    float v = 0.0f;

    /// Which page of the atlas it lies on. All vertices of a face lie on
    /// the same one.
    std::uint32_t page = 0;
  };
} // quake

#endif //QUAKE_LIGHTMAP_ATLAS_VERTEX_HPP
