#ifndef QUAKE_MDL_PACKED_VERTEX_HPP
#define QUAKE_MDL_PACKED_VERTEX_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A vertex of a model in one pose, as the file keeps it, in four bytes.
  struct MdlPackedVertex
  {
    /// The place, a byte for each axis. The scale and the translate of the
    /// header make units of the game of it.
    std::array<std::uint8_t, 3> position{};

    /// Which of the 162 directions the original game has in a table of its
    /// own is the normal. The table is not here: `MdlMesh` computes normals
    /// from the triangles instead. The corners of a bounding box have a
    /// number here that means nothing.
    std::uint8_t normal_index = 0;
  };
} // quake

#endif //QUAKE_MDL_PACKED_VERTEX_HPP
