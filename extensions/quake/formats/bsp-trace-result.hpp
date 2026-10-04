#ifndef QUAKE_BSP_TRACE_RESULT_HPP
#define QUAKE_BSP_TRACE_RESULT_HPP

#include "bsp-vector.hpp"

namespace quake
{
  /// What a move from one place to another through a hull met, as the game
  /// code is told it.
  ///
  /// A move that met nothing has a fraction of 1 and ends where it was meant
  /// to end. One that was stopped ends a little before what stopped it, and
  /// says which plane that was.
  struct BspTraceResult
  {
    /// The whole move was inside what is solid: it started there and never
    /// left. The fraction is 1 then and the end is where the move was meant
    /// to end, as in the game, so this is asked first.
    bool all_solid = false;

    /// The move started inside what is solid.
    bool start_solid = false;

    /// A part of the move went through what is empty.
    bool in_open = false;

    /// A part of the move went through water, slime, lava, or the sky.
    bool in_water = false;

    /// How much of the move was made, from 0 to 1.
    float fraction = 1.0f;

    /// Where the move ended.
    BspVector end_position;

    /// The plane that stopped the move, looking towards where the move came
    /// from. It is left as it is when nothing stopped the move.
    BspVector plane_normal;
    float plane_distance = 0.0f;
  };
} // quake

#endif //QUAKE_BSP_TRACE_RESULT_HPP
