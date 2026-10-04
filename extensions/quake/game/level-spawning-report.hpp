#ifndef QUAKE_LEVEL_SPAWNING_REPORT_HPP
#define QUAKE_LEVEL_SPAWNING_REPORT_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "level-failure.hpp"

namespace quake
{
  /// What became of the entities of a level's text when `LevelSpawning`
  /// handed them to the game code. Every entity of the text is counted in
  /// one of the first five.
  struct LevelSpawningReport
  {
    /// Entities whose function ran to its end. The game code may have
    /// removed one in it, which is its way of leaving one out.
    std::size_t spawned = 0;

    /// Entities left out by their `spawnflags` for the skill, or for a
    /// deathmatch.
    std::size_t left_out = 0;

    /// Entities without a classname, or with one the game code has no
    /// function for. The names are in `classnames_without_function`, each
    /// once, the empty one for no classname.
    std::size_t without_function = 0;
    std::vector<std::string> classnames_without_function;

    /// Entities whose function was stopped. Why is in `failures`.
    std::size_t failed = 0;
    std::vector<LevelFailure> failures;

    /// Entities the machine had no room for. They end the level's text:
    /// none after the first is looked at.
    std::size_t without_room = 0;

    /// Keys that were not written: no field of that name, a field of a type
    /// a text cannot fill, or a function that there is not.
    std::size_t unknown_keys = 0;
  };
} // quake

#endif //QUAKE_LEVEL_SPAWNING_REPORT_HPP
