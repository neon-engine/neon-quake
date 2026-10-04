#ifndef QUAKE_DEMO_PARTICLES_HPP
#define QUAKE_DEMO_PARTICLES_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// A burst of particles a server asks for, such as blood from a hit.
  struct DemoParticles
  {
    /// Where it is, in the units and axes of the game.
    LevelVector origin{};

    /// Where the particles drift, in 16ths of a unit a step.
    LevelVector direction{};

    /// The colour of the palette they take.
    std::int32_t color = 0;

    /// How many. A server writes 255 for the cloud of an explosion, which
    /// is 1024 here, as the original has it.
    std::int32_t count = 0;
  };
} // quake

#endif //QUAKE_DEMO_PARTICLES_HPP
