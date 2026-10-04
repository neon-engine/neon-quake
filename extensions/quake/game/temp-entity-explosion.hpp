#ifndef QUAKE_TEMP_ENTITY_EXPLOSION_HPP
#define QUAKE_TEMP_ENTITY_EXPLOSION_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// The temp entity `Explosion2`: an explosion whose particles have the
  /// colours the game code names.
  struct TempEntityExplosion
  {
    /// Where it is, in the units and axes of the game.
    LevelVector position{};

    /// The first colour of the palette the particles take, and how many
    /// colours from there on they go through. Each is a byte.
    std::int32_t color_start = 0;
    std::int32_t color_count = 0;
  };
} // quake

#endif //QUAKE_TEMP_ENTITY_EXPLOSION_HPP
