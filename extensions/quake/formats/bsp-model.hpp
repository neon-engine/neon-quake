#ifndef QUAKE_BSP_MODEL_HPP
#define QUAKE_BSP_MODEL_HPP

#include <array>
#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// A part of the level that is one thing. Model 0 is the world, the others
  /// are what moves in it: doors, lifts, buttons, and triggers, which an
  /// entity names as `*1`, `*2`, and so on.
  struct BspModel
  {
    BspVector mins;
    BspVector maxs;
    BspVector origin;

    /// Where its four trees start: the nodes it is drawn by, then the clip
    /// nodes a point, a player, and a large monster collide with. The fourth
    /// is not used by the game.
    std::array<std::int32_t, 4> head_nodes{};

    /// How many leaves it has, not counting leaf 0.
    std::int32_t leaf_count = 0;

    std::int32_t first_face = 0;
    std::int32_t face_count = 0;
  };
} // quake

#endif //QUAKE_BSP_MODEL_HPP
