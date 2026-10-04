#ifndef QUAKE_DEMO_SOUND_HPP
#define QUAKE_DEMO_SOUND_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// A sound a server starts.
  struct DemoSound
  {
    /// The entity that makes it, and which of its channels, from 0 to 7.
    /// A sound on a channel takes the place of the one before on it, but
    /// for channel 0, which never cuts a sound off.
    std::int32_t entity = 0;
    std::int32_t channel = 0;

    /// Which sound, as an index into the names of DemoServerInfo.
    std::int32_t sound = 0;

    /// From 0 to 1.
    float volume = 1.0f;

    /// How fast it fades with distance: 0 is heard everywhere, 1 is usual.
    float attenuation = 1.0f;

    /// Where it is, in the units and axes of the game.
    LevelVector origin{};
  };
} // quake

#endif //QUAKE_DEMO_SOUND_HPP
