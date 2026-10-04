#ifndef QUAKE_LEVEL_SPAWNING_SETTINGS_HPP
#define QUAKE_LEVEL_SPAWNING_SETTINGS_HPP

#include <cstdint>
#include <string>

namespace quake
{
  /// What a host says of the game a level is started for. In the original
  /// these are console variables and what the server knows; here the host
  /// hands them over.
  struct LevelSpawningSettings
  {
    /// The name of the level without folder and ending, such as `start`:
    /// the global `mapname`.
    std::string map_name;

    /// The file of the level, such as `maps/start.bsp`: the field `model`
    /// of the world.
    std::string model_name;

    /// 0 easy, 1 medium, 2 hard, 3 nightmare. It decides which entities of
    /// the level are left out. What is outside counts as the nearest.
    int skill = 1;

    /// The globals of the same names. In a deathmatch the skill leaves
    /// nothing out, and the entities marked as not for one are.
    float deathmatch = 0.0f;
    float coop = 0.0f;

    /// The global `serverflags`: what is carried through an episode, the
    /// runes.
    float server_flags = 0.0f;

    /// How many entities after the world are kept for players. The entities
    /// of the level come after them.
    std::int32_t player_count = 1;
  };
} // quake

#endif //QUAKE_LEVEL_SPAWNING_SETTINGS_HPP
