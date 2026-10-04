#ifndef QUAKE_PARTICLE_HPP
#define QUAKE_PARTICLE_HPP

#include <cstdint>

#include "level-vector.hpp"
#include "particle-kind.hpp"

namespace quake
{
  /// One small square of one colour: a spark, a drop of blood, a bit of
  /// smoke. The original draws every effect of its kind out of these.
  struct Particle
  {
    /// Where it is, and how far it goes in a second, in the units and axes
    /// of the game.
    LevelVector position{};
    LevelVector velocity{};

    /// Which colour of the palette of the game it has. A host has the
    /// palette.
    std::uint8_t colour = 0;

    /// What happens to it over time.
    ParticleKind kind = ParticleKind::Static;

    /// How far it is through the colours of its kind. The whole part is
    /// the colour it has. Only `Fire`, `Explode`, and `Explode2` use it.
    float ramp = 0.0f;

    /// The time of the system after which it is gone.
    float dies_at = 0.0f;
  };
} // quake

#endif //QUAKE_PARTICLE_HPP
