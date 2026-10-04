#ifndef QUAKE_TEMP_ENTITY_POINT_HPP
#define QUAKE_TEMP_ENTITY_POINT_HPP

#include "level-vector.hpp"
#include "temp-entity-kind.hpp"

namespace quake
{
  /// A temp entity at one place: an impact, an explosion, a splash, or the
  /// sparkle of a teleporter.
  struct TempEntityPoint
  {
    TempEntityKind kind = TempEntityKind::Spike;

    /// Where it is, in the units and axes of the game.
    LevelVector position{};
  };
} // quake

#endif //QUAKE_TEMP_ENTITY_POINT_HPP
