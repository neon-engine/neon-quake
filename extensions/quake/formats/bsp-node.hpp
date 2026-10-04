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
    ///
    /// They are as wide as the widest form of a level has them. A file of
    /// version 29 keeps them in 16 bits, and one with more than 32767 nodes
    /// keeps the numbers above that as negative ones: `BspFile` reads those
    /// back into the numbers of the nodes they are, so that a negative
    /// child here is always a leaf.
    std::array<std::int32_t, 2> children{};

    /// The box around everything below this node. Version 29 has whole
    /// numbers here.
    std::array<float, 3> mins{};
    std::array<float, 3> maxs{};

    /// The faces that lie on the plane of this node.
    std::uint32_t first_face = 0;
    std::uint32_t face_count = 0;
  };
} // quake

#endif //QUAKE_BSP_NODE_HPP
