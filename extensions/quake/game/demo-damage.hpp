#ifndef QUAKE_DEMO_DAMAGE_HPP
#define QUAKE_DEMO_DAMAGE_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// The player was hurt: the screen flashes red, and the view is kicked
  /// away from where the hurt came from.
  struct DemoDamage
  {
    /// How much of it the armour took, and how much the body.
    std::int32_t armor = 0;
    std::int32_t blood = 0;

    /// Where it came from, in the units and axes of the game.
    LevelVector from{};
  };
} // quake

#endif //QUAKE_DEMO_DAMAGE_HPP
