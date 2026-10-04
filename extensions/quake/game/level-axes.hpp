#ifndef QUAKE_LEVEL_AXES_HPP
#define QUAKE_LEVEL_AXES_HPP

#include <cmath>
#include <numbers>

#include "level-vector.hpp"

namespace quake
{
  /// The three directions of something that is turned by angles: where it
  /// looks, what is to its right, and what is above it. It is what the
  /// builtin `makevectors` gives the game code, for what moves a player.
  struct LevelAxes
  {
    LevelVector forward{};
    LevelVector right{};
    LevelVector up{};

    /// The axes of pitch, yaw, and roll in degrees. A pitch above zero
    /// looks down, and the yaw is counted from the X axis towards the Y
    /// axis.
    [[nodiscard]] static LevelAxes Of(const LevelVector &angles)
    {
      constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;
      const float sin_pitch = std::sin(angles[0] * degrees_to_radians);
      const float cos_pitch = std::cos(angles[0] * degrees_to_radians);
      const float sin_yaw = std::sin(angles[1] * degrees_to_radians);
      const float cos_yaw = std::cos(angles[1] * degrees_to_radians);
      const float sin_roll = std::sin(angles[2] * degrees_to_radians);
      const float cos_roll = std::cos(angles[2] * degrees_to_radians);

      return {
        .forward = {cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch},
        .right = {
          cos_roll * sin_yaw - sin_roll * sin_pitch * cos_yaw,
          -cos_roll * cos_yaw - sin_roll * sin_pitch * sin_yaw,
          -sin_roll * cos_pitch,
        },
        .up = {
          cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw,
          cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw,
          cos_roll * cos_pitch,
        },
      };
    }
  };
} // quake

#endif //QUAKE_LEVEL_AXES_HPP
