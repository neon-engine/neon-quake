#ifndef QUAKE_DEMO_STATIC_SOUND_HPP
#define QUAKE_DEMO_STATIC_SOUND_HPP

#include <cstdint>

#include "level-vector.hpp"

namespace quake
{
  /// A sound that goes on for as long as the level does at one place, such
  /// as the hum of a light or the crackle of a torch.
  struct DemoStaticSound
  {
    /// Where it is, in the units and axes of the game.
    LevelVector origin{};

    /// Which sound, as an index into the names of DemoServerInfo.
    std::int32_t sound = 0;

    /// From 0 to 1.
    float volume = 1.0f;

    /// How fast it fades with distance, as DemoSound::attenuation.
    float attenuation = 1.0f;
  };
} // quake

#endif //QUAKE_DEMO_STATIC_SOUND_HPP
