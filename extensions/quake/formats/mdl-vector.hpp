#ifndef QUAKE_MDL_VECTOR_HPP
#define QUAKE_MDL_VECTOR_HPP

namespace quake
{
  /// Three numbers of a model: a place, a direction, or a scale, in the
  /// game's own units and axes, where x is forward, y is left, and z is up.
  struct MdlVector
  {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
  };
} // quake

#endif //QUAKE_MDL_VECTOR_HPP
