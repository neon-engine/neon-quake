#ifndef QUAKE_MDL_TRIANGLE_HPP
#define QUAKE_MDL_TRIANGLE_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// A triangle of a model: three of its vertices, which go around clockwise
  /// when the triangle is seen from outside the model.
  struct MdlTriangle
  {
    /// Whether the triangle is painted from the left half of the skin, the
    /// front. One that is not takes its vertices on the seam from the right
    /// half.
    bool faces_front = true;

    std::array<std::int32_t, 3> vertices{};
  };
} // quake

#endif //QUAKE_MDL_TRIANGLE_HPP
