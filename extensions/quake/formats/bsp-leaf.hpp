#ifndef QUAKE_BSP_LEAF_HPP
#define QUAKE_BSP_LEAF_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// An end of the tree of nodes: a room without a fork in it. Leaf 0 is
  /// everything solid.
  struct BspLeaf
  {
    /// What fills it: -1 nothing, -2 solid, -3 water, -4 slime, -5 lava,
    /// -6 sky.
    std::int32_t contents = 0;

    /// Where the list of leaves seen from here starts in the visibility, or
    /// -1 when everything is seen.
    /// It means nothing in a level that has no visibility at all.
    std::int32_t visibility_offset = -1;

    std::array<std::int16_t, 3> mins{};
    std::array<std::int16_t, 3> maxs{};

    /// Its faces, as entries of the list of faces of leaves.
    std::uint16_t first_leaf_face = 0;
    std::uint16_t leaf_face_count = 0;

    /// How loud water, sky, slime, and lava sound here.
    std::array<std::uint8_t, 4> ambient_levels{};
  };
} // quake

#endif //QUAKE_BSP_LEAF_HPP
