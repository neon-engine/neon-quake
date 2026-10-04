#ifndef QUAKE_PARTICLE_SETTINGS_HPP
#define QUAKE_PARTICLE_SETTINGS_HPP

#include <cstddef>
#include <cstdint>

namespace quake
{
  /// What a `ParticleSystem` is made with.
  struct ParticleSettings
  {
    /// How many particles there are at the most. An effect that finds no
    /// room makes fewer particles, or none. The original has 2048, which
    /// two explosions fill. Ports of today have 16384.
    std::size_t limit = 16384;

    /// What the random numbers start from. The same seed and the same
    /// calls give the same particles.
    std::uint32_t seed = 1;
  };
} // quake

#endif //QUAKE_PARTICLE_SETTINGS_HPP
