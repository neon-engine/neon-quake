#ifndef QUAKE_BSP_HULL_NODE_HPP
#define QUAKE_BSP_HULL_NODE_HPP

#include <array>
#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// A fork of a hull: a plane, and what lies in front of it and behind it.
  ///
  /// It carries its plane itself, so that a hull keeps nothing of the level
  /// it was made from.
  struct BspHullNode
  {
    /// The direction the front of the plane looks in, of length one.
    BspVector normal;

    /// How far the plane is from the origin, along its normal.
    float distance = 0.0f;

    /// What is in front and behind. A number that is not negative is another
    /// node of the same hull. A negative one is what fills the space there,
    /// a value of `BspContents`.
    std::array<std::int32_t, 2> children{};
  };
} // quake

#endif //QUAKE_BSP_HULL_NODE_HPP
