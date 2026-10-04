#ifndef QUAKE_LEVEL_SPAWNING_HPP
#define QUAKE_LEVEL_SPAWNING_HPP

#include <cstdint>
#include <span>
#include <string_view>

#include "formats/bsp-entity.hpp"
#include "formats/qc-machine.hpp"
#include "level-spawning-report.hpp"
#include "level-spawning-settings.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// Hands the entities of a level to the game code, as the engine of the
  /// original does when a level starts.
  ///
  /// The text of a level only says what there is: keys and values. The game
  /// code makes a door or a monster of it. So each entity of the text gets
  /// an entity of the machine, its keys are written into the fields of the
  /// same names, and the function named by its `classname` is called with
  /// it as `self`.
  ///
  /// It only fills the machine. What the game code asks for meanwhile, a
  /// model, a sound, reaches the host through the builtins it registered,
  /// which must be there before Spawn().
  class LevelSpawning final
  {
    QcMachine &_machine;
    QcGlobals _globals;
    QcFields _fields;

    /// Writes a value of the text into the field a key names, by the type
    /// of the field. False when nothing was written.
    bool SetFieldFromText(std::int32_t entity, std::string_view key, std::string_view value);

    /// Whether the flags of an entity leave it out of this game.
    [[nodiscard]] bool IsLeftOut(std::int32_t entity, const LevelSpawningSettings &settings);

  public:
    /// The time a level starts at, in seconds. Not zero, so that the game
    /// code can tell a time that was set from one that was not.
    static constexpr float start_time = 1.0f;

    // The bits of `spawnflags` that leave an entity out of a game.
    static constexpr std::int32_t not_on_easy = 256;
    static constexpr std::int32_t not_on_medium = 512;
    static constexpr std::int32_t not_on_hard = 1024;
    static constexpr std::int32_t not_in_deathmatch = 2048;

    /// The machine must outlive this.
    explicit LevelSpawning(QcMachine &machine);

    /// Starts a level on a machine that has run nothing yet and has only
    /// the world.
    ///
    /// First what the game code expects to find: the globals `mapname`,
    /// `time`, `serverflags`, `deathmatch`, and `coop`, the world as the
    /// model of the level that pushes and is solid, and the entities kept
    /// for the players. Then the entities of the text, the first of which
    /// is the world.
    ///
    /// A function that is stopped does not end it: the entity stays as the
    /// function left it, and the report says why.
    LevelSpawningReport Spawn(std::span<const BspEntity> entities, const LevelSpawningSettings &settings);
  };
} // quake

#endif //QUAKE_LEVEL_SPAWNING_HPP
