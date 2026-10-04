#ifndef QUAKE_BSP_MESH_GROUP_HPP
#define QUAKE_BSP_MESH_GROUP_HPP

#include <cstdint>
#include <vector>

namespace quake
{
  /// The triangles of a model of a level that show the same texture, which
  /// is what a renderer draws in one go.
  struct BspMeshGroup
  {
    /// Which texture of the level.
    std::int32_t texture = 0;

    /// Whether the texture is the sky.
    bool is_sky = false;

    /// Whether the texture is a liquid.
    bool is_liquid = false;

    /// The triangles, three vertices of the mesh for each.
    std::vector<std::uint32_t> indices;
  };
} // quake

#endif //QUAKE_BSP_MESH_GROUP_HPP
