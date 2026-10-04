#ifndef QUAKE_PARTICLE_RANDOM_HPP
#define QUAKE_PARTICLE_RANDOM_HPP

#include <cstdint>

namespace quake
{
  /// The random numbers particles are made with. The original takes them
  /// from `rand()` of the C library, which is not the same from one system
  /// to the next. This one is: a seed gives the same numbers everywhere,
  /// which is what lets a test say where a particle is.
  class ParticleRandom final
  {
    std::uint32_t _state;

  public:
    explicit constexpr ParticleRandom(const std::uint32_t seed) : _state(seed)
    {
    }

    /// A number from 0 to 32767, as the smallest `rand()` gives. The
    /// effects cut it down with `&` and `%`, as the original does.
    [[nodiscard]] constexpr std::int32_t Next()
    {
      // the upper bits of a linear congruential generator are the good ones
      _state = _state * 1664525u + 1013904223u;
      return static_cast<std::int32_t>((_state >> 16) & 0x7fffu);
    }
  };
} // quake

#endif //QUAKE_PARTICLE_RANDOM_HPP
