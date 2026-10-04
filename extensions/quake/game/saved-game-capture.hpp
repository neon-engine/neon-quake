#ifndef QUAKE_SAVED_GAME_CAPTURE_HPP
#define QUAKE_SAVED_GAME_CAPTURE_HPP

#include <string>

#include "formats/qc-machine.hpp"
#include "saved-game-restore-report.hpp"
#include "saved-game.hpp"

namespace quake
{
  /// Takes a `SavedGame` from a running game, and puts one back into a
  /// machine, as the original does when a player saves and loads. Neither
  /// calls game code.
  ///
  /// To save, a host does this, in the order of the original:
  ///
  /// 1. It refuses when there is nothing to save: no level runs, the
  ///    player is dead (`health` is zero or less), or the level is over
  ///    and its counts are shown.
  /// 2. It fills what it knows itself into a `SavedGame`: the comment, see
  ///    `SavedGameText::MakeComment`, the parms, the skill, the name of
  ///    the level, the time of `LevelRunning`, and the light styles as
  ///    `QcCoreBuiltins` holds them.
  /// 3. Capture() adds the globals and the entities, between two frames.
  /// 4. `SavedGameText::Write` makes the file.
  ///
  /// The parms are those the player came into the level with: what the
  /// host handed `LevelRunning::ConnectClient`, or for a new player the
  /// globals `parm1` to `parm16` right after it. They are not asked of the
  /// game code when the game is saved, since `SetChangeParms` takes the
  /// keys of the level away from the player it is run for.
  ///
  /// To load, a host does this, in the order of the original:
  ///
  /// 1. `SavedGameText::Read`.
  /// 2. It sets the console variable `skill` to the skill of the saved
  ///    game, and starts the level of its name as for a new game, on a
  ///    machine made now: the builtins registered, the level as model 1,
  ///    `LevelSpawning::Spawn`, and the two frames of a tenth of a second.
  ///    The game code does spawn every entity of the level's text, though
  ///    all of them are overwritten in step 4: that is how the models and
  ///    sounds get their numbers again, which `modelindex` of the saved
  ///    entities counts on, and how what is no entity is made again, the
  ///    static entities and the sounds of the level.
  /// 3. It does not let the player in: no `LevelRunning::ConnectClient`.
  ///    The entity of the player comes from the saved game. It gives each
  ///    light style of the saved game to
  ///    `QcCoreBuiltins::RestoreLightStyle`, which tells it of them as the
  ///    game code would.
  /// 4. Restore() writes the globals and every entity.
  /// 5. It links every entity that is not free, `LevelCollision::Link`,
  ///    sets the time, `LevelRunning::SetTime`, counts the player in,
  ///    `LevelRunning::SetClientCount`, and keeps the parms for the player.
  /// 6. It makes anew what it shows from the entities as they now are, and
  ///    looks where the `angles` of the player say.
  class SavedGameCapture final
  {
  public:
    /// Takes the game code's part of a saved game from a machine, and
    /// gives it back with what the host filled in before, which is left as
    /// it is.
    ///
    /// The globals are those the program marks to be saved, of the kinds
    /// the original saves: strings, floats, and entities. An entity has
    /// every field that is not zero, but for the parts of a vector, which
    /// the program names a second time with `_x`, `_y`, and `_z`: any name
    /// with `_` before its last letter is left out, as in the original.
    /// Fields of no type and pointers are left out too, which the original
    /// writes and cannot read.
    ///
    /// The machine is not const since it hands the cells of an entity out
    /// to be written as well. Nothing is written.
    [[nodiscard]] static SavedGame Capture(QcMachine &machine, SavedGame game = {});

    /// Writes the globals and the entities of a saved game into a machine
    /// that was made for the same program and has started the same level.
    ///
    /// Every entity of the machine is set to zero first, then gets the
    /// fields the saved game names, so that an entity has the number it
    /// had: entities are made up to the last of the saved game, a free one
    /// is free, and those the machine has beyond are freed. The original
    /// drops those; here they stay, free, since the machine never has
    /// fewer entities than it had. A global or a field the program does
    /// not have is left out and counted in `report`.
    ///
    /// Returns false and says why in `error` when the saved game has no
    /// entity, a value names a function or a field the program does not
    /// have or an entity the saved game does not have, or the machine has
    /// no room for the entities. The globals and the entities are then as
    /// they were, but for a machine without room, which is left with more
    /// entities than it had.
    static bool Restore(
      const SavedGame &game, QcMachine &machine, std::string &error, SavedGameRestoreReport *report = nullptr);
  };
} // quake

#endif //QUAKE_SAVED_GAME_CAPTURE_HPP
