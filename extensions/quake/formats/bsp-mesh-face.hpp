#ifndef QUAKE_BSP_MESH_FACE_HPP
#define QUAKE_BSP_MESH_FACE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "bsp-face.hpp"

namespace quake
{
  /// A face of a model of a level as it is drawn: which vertices of the mesh
  /// are its corners, and its lightmap.
  struct BspMeshFace
  {
    /// Which face of the level it is.
    std::uint32_t face = 0;

    /// Which texture of the level it shows.
    std::int32_t texture = 0;

    /// Its corners among the vertices of the mesh, in order around it. No
    /// other face uses them.
    std::uint32_t first_vertex = 0;
    std::uint32_t vertex_count = 0;

    /// Whether its texture is the sky, which is not drawn as a wall.
    bool is_sky = false;

    /// Whether its texture is a liquid, which is drawn waving.
    bool is_liquid = false;

    /// How many samples its lightmap is wide and high. Both are zero for a
    /// face that is never lit by a lightmap, as the sky.
    std::uint32_t lightmap_width = 0;
    std::uint32_t lightmap_height = 0;

    /// What lights each of its lightmaps, as the face of the level says it.
    std::array<std::uint8_t, 4> light_styles{
      BspFace::no_style, BspFace::no_style, BspFace::no_style, BspFace::no_style};

    /// Where its lightmaps start in the lighting of the level, or -1 when it
    /// has none. A level with lighting draws such a face dark, a level
    /// without any draws everything fully lit.
    std::int32_t light_offset = -1;

    /// Its lightmaps: one byte for each sample, row after row from the top,
    /// `lightmap_width * lightmap_height` of them for each style that is
    /// used, one style after the other. Empty when it has none.
    std::vector<std::uint8_t> lightmap;

    /// How many lightmaps it has, one for each style up to the first that is
    /// `BspFace::no_style`.
    [[nodiscard]] std::size_t CountLightStyles() const
    {
      std::size_t count = 0;
      while (count < light_styles.size() && light_styles[count] != BspFace::no_style) { count++; }
      return count;
    }
  };
} // quake

#endif //QUAKE_BSP_MESH_FACE_HPP
