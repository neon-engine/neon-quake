#ifndef QUAKE_BSP_VECTOR_HPP
#define QUAKE_BSP_VECTOR_HPP

namespace quake
{
  /// Three numbers of a level: a place, a direction, or a corner of a box.
  ///
  /// They are in the units and axes of the game, as the file has them: X and
  /// Y lie on the ground and Z points up. Nothing here turns them into the
  /// axes of the engine.
  struct BspVector
  {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  };
} // quake

#endif //QUAKE_BSP_VECTOR_HPP
