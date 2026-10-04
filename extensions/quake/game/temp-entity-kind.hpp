#ifndef QUAKE_TEMP_ENTITY_KIND_HPP
#define QUAKE_TEMP_ENTITY_KIND_HPP

#include <cstdint>

namespace quake
{
  /// What a temp entity is: something that is shown for a moment and is no
  /// entity of the game code, such as the sparks of a shot or a bolt of
  /// lightning. The numbers are those of the byte the game code writes
  /// after the one that says the message is a temp entity.
  enum class TempEntityKind : std::int32_t
  {
    /// A nail hit a wall.
    Spike = 0,
    /// A nail of the larger gun hit a wall.
    SuperSpike = 1,
    /// A shot hit a wall.
    Gunshot = 2,
    /// A rocket or a grenade went off.
    Explosion = 3,
    /// The purple blast of a spawn.
    TarExplosion = 4,
    /// The bolt of a shambler, from an entity to a place.
    Lightning1 = 5,
    /// The bolt of the lightning gun.
    Lightning2 = 6,
    /// The shot of a scrag hit a wall.
    WizardSpike = 7,
    /// The shot of a hell knight hit a wall.
    KnightSpike = 8,
    /// The bolt that kills the boss of the first episode.
    Lightning3 = 9,
    /// The boss of the first episode rises from the lava.
    LavaSplash = 10,
    /// Something came out of a teleporter.
    Teleport = 11,
    /// An explosion with particles of colours the message names.
    Explosion2 = 12,
    /// A beam from an entity to a place, the hook of a mission pack.
    Beam = 13,
  };
} // quake

#endif //QUAKE_TEMP_ENTITY_KIND_HPP
