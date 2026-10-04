#ifndef QUAKE_BSP_PLANE_HPP
#define QUAKE_BSP_PLANE_HPP

#include <cstdint>

#include "bsp-vector.hpp"

namespace quake
{
  /// A plane that cuts the level in two: the faces lie on planes, and the
  /// nodes split space by them.
  struct BspPlane
  {
    /// The direction the front of the plane looks in, of length one.
    BspVector normal;

    /// How far the plane is from the origin, along its normal.
    float distance = 0.0f;

    /// 0, 1, or 2 when the normal is exactly the X, Y, or Z axis, and 3, 4,
    /// or 5 when it is only closest to that axis.
    std::int32_t type = 0;
  };
} // quake

#endif //QUAKE_BSP_PLANE_HPP
