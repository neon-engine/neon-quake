#ifndef QUAKE_BSP_EDGE_HPP
#define QUAKE_BSP_EDGE_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A line between two vertices, shared by the faces on both sides of it.
  struct BspEdge
  {
    std::array<std::uint32_t, 2> vertices{};
  };
} // quake

#endif //QUAKE_BSP_EDGE_HPP
