#ifndef QUAKE_BSP_NODE_HPP
#define QUAKE_BSP_NODE_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A fork of the tree that orders the level for drawing: a plane, and what
  /// lies in front of it and behind it.
  struct BspNode
  {
    std::int32_t plane = 0;

    /// What is in front and behind. A number that is not negative is another
    /// node. A negative one is a leaf: leaf `-(child + 1)`, so -1 is leaf 0.
    std::array<std::int16_t, 2> children{};

    /// The box around everything below this node.
    std::array<std::int16_t, 3> mins{};
    std::array<std::int16_t, 3> maxs{};

    /// The faces that lie on the plane of this node.
    std::uint16_t first_face = 0;
    std::uint16_t face_count = 0;
  };
} // quake

#endif //QUAKE_BSP_NODE_HPP
