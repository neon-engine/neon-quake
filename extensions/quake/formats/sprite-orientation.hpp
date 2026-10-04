#ifndef QUAKE_SPRITE_ORIENTATION_HPP
#define QUAKE_SPRITE_ORIENTATION_HPP

#include <cstdint>

namespace quake
{
  /// How a sprite turns to whoever looks at it, with the numbers the file
  /// has for each way.
  enum class SpriteOrientation : std::int32_t
  {
    /// Flat to the screen, but upright in the world whatever way the view
    /// rolls.
    ParallelUpright = 0,

    /// Turned to the viewer around the axis that is up in the world.
    FacingUpright = 1,

    /// Flat to the screen. Most sprites are: explosions, bubbles.
    Parallel = 2,

    /// Not turned at all: it lies as the angles of its entity say, as a
    /// mark on a wall does.
    Oriented = 3,

    /// Flat to the screen, and rolled by the angle of its entity.
    ParallelOriented = 4,
  };
} // quake

#endif //QUAKE_SPRITE_ORIENTATION_HPP
