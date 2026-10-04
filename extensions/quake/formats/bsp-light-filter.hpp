#ifndef QUAKE_BSP_LIGHT_FILTER_HPP
#define QUAKE_BSP_LIGHT_FILTER_HPP

namespace quake
{
  /// How the light at a place of a face is taken from the samples of its
  /// lightmap, which lie sixteen units apart.
  enum class BspLightFilter
  {
    /// One sample: the one at or before the place along both directions of
    /// the texture, as the original game takes it. The light of a thing
    /// that moves changes in steps then, every sixteen units.
    Nearest,

    /// The four samples around the place, each counted by how near it is,
    /// as the later ports take it. The light changes smoothly then, and is
    /// what the wall itself shows at the place.
    Bilinear,
  };
} // quake

#endif //QUAKE_BSP_LIGHT_FILTER_HPP
