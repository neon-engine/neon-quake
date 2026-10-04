#ifndef QUAKE_TEMP_ENTITY_BEAM_HPP
#define QUAKE_TEMP_ENTITY_BEAM_HPP

#include <cstdint>

#include "level-vector.hpp"
#include "temp-entity-kind.hpp"

namespace quake
{
  /// A temp entity from one place to another: a bolt of lightning, or a
  /// beam.
  struct TempEntityBeam
  {
    /// Lightning1, Lightning2, Lightning3, or Beam.
    TempEntityKind kind = TempEntityKind::Lightning1;

    /// The entity the beam comes from. It has one beam at a time: a new
    /// beam of the same entity takes the place of the one before.
    std::int32_t entity = 0;

    /// Where it starts and ends, in the units and axes of the game.
    LevelVector start{};
    LevelVector end{};
  };
} // quake

#endif //QUAKE_TEMP_ENTITY_BEAM_HPP
