#ifndef QUAKE_BSP_TEXTURE_INFO_HPP
#define QUAKE_BSP_TEXTURE_INFO_HPP

#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// How a texture is laid on a face: two directions in the level, along
  /// which the texture runs to the right and down, and where it starts.
  ///
  /// A place of the level lies at `s = place . s_axis + s_offset` and
  /// `t = place . t_axis + t_offset` of the texture, counted in its pixels.
  struct BspTextureInfo
  {
    /// The bit of `flags` of a face that has no lightmap and may be as large
    /// as it likes: the sky and, from the old tools, the liquids.
    static constexpr std::int32_t special = 1;

    BspVector s_axis;
    float s_offset = 0.0f;
    BspVector t_axis;
    float t_offset = 0.0f;

    /// Which texture of the level.
    std::int32_t texture = 0;

    std::int32_t flags = 0;
  };
} // quake

#endif //QUAKE_BSP_TEXTURE_INFO_HPP
