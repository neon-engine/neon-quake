#ifndef QUAKE_SPACE_HPP
#define QUAKE_SPACE_HPP

#include "bsp-vector.hpp"

namespace quake
{
  /// How the space of the game becomes the space of Neon Engine.
  ///
  /// The game counts in units of its own, with x to the east, y to the
  /// north, and z up. The engine counts in metres, with y up and the
  /// camera looking down the negative z. Everything the formats read stays
  /// as the game has it; this is the one place where it is turned over, for
  /// what is handed to the engine.
  ///
  /// Turning the axes is a turn and no mirror, so a triangle keeps the way
  /// it winds. The game winds its triangles clockwise seen from outside and
  /// the engine anticlockwise, which whoever hands triangles over sees to,
  /// by swapping two corners of each.
  struct QuakeSpace
  {
    /// How long a unit of the game is. The player of the game is 56 units
    /// tall, which this makes 1.75 metres.
    static constexpr float metres_per_unit = 1.0f / 32.0f;

    /// A place: x stays, what was up becomes y, and what was north becomes
    /// the negative z; in metres.
    [[nodiscard]] static constexpr BspVector ToEnginePosition(const BspVector &place)
    {
      return {place.x * metres_per_unit, place.z * metres_per_unit, -place.y * metres_per_unit};
    }

    /// A place of the engine as the game has it, in its units: what
    /// ToEnginePosition() does, the other way around.
    [[nodiscard]] static constexpr BspVector ToGamePosition(const BspVector &place)
    {
      return {place.x / metres_per_unit, -place.z / metres_per_unit, place.y / metres_per_unit};
    }

    /// A direction, such as a normal: turned as a place is, and as long as
    /// it was.
    [[nodiscard]] static constexpr BspVector ToEngineDirection(const BspVector &direction)
    {
      return {direction.x, direction.z, -direction.y};
    }

    /// The yaw of the engine, in degrees, for an angle of the game, which
    /// is 0 towards the east and grows anticlockwise seen from above. The
    /// engine looks down the negative z at a yaw of 0, which is the game's
    /// north, and its yaw grows the same way.
    [[nodiscard]] static constexpr float ToEngineYaw(const float angle)
    {
      return angle - 90.0f;
    }
  };
} // quake

#endif //QUAKE_SPACE_HPP
