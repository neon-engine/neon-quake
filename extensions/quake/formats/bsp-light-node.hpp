#ifndef QUAKE_BSP_LIGHT_NODE_HPP
#define QUAKE_BSP_LIGHT_NODE_HPP

#include <array>
#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// A fork of the tree a level is drawn by, as `BspLightPoint` keeps it: a
  /// plane, what lies in front of it and behind it, and the faces that lie
  /// on it.
  ///
  /// It carries its plane itself, so that nothing of the level it was made
  /// from has to be kept.
  struct BspLightNode
  {
    /// Stands for a leaf in place of the number of a fork: there is nothing
    /// further down to look at.
    static constexpr std::int32_t leaf = -1;

    /// The direction the front of the plane looks in, of length one.
    BspVector normal;

    /// How far the plane is from the origin, along its normal.
    float distance = 0.0f;

    /// What is in front and behind: another fork, or `leaf`.
    std::array<std::int32_t, 2> children{leaf, leaf};

    /// The faces that lie on the plane, by their numbers in the level.
    std::uint32_t first_face = 0;
    std::uint32_t face_count = 0;
  };
} // quake

#endif //QUAKE_BSP_LIGHT_NODE_HPP
