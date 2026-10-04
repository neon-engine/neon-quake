#ifndef QUAKE_LEVEL_TRACE_RESULT_HPP
#define QUAKE_LEVEL_TRACE_RESULT_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// What a move through a level and its entities met: what `BspTraceResult`
  /// says of a move through one hull, and which entity stopped it.
  struct LevelTraceResult
  {
    /// What `entity` is when nothing stopped the move.
    static constexpr std::int32_t nothing = -1;

    /// The whole move was inside what is solid. The fraction is 1 then, as
    /// in the game, so this is asked first.
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
    LevelVector end_position{};

    /// The plane that stopped the move, looking towards where the move came
    /// from.
    LevelVector plane_normal{};
    float plane_distance = 0.0f;

    /// The entity that stopped the move or that it started inside of: 0 for
    /// the world, `nothing` when there is none.
    std::int32_t entity = nothing;

    /// Whether something stopped the move or held it from the start.
    [[nodiscard]] constexpr bool HasEntity() const
    {
      return entity != nothing;
    }
  };
} // quake

#endif //QUAKE_LEVEL_TRACE_RESULT_HPP
