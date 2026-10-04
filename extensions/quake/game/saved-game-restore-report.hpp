#ifndef QUAKE_SAVED_GAME_RESTORE_REPORT_HPP
#define QUAKE_SAVED_GAME_RESTORE_REPORT_HPP

#include <cstddef>

namespace quake
{
  /// What `SavedGameCapture::Restore` left out of a saved game: names the
  /// program does not have, as when the game was saved with another
  /// version of the game code. The original says each on its console and
  /// goes on.
  struct SavedGameRestoreReport
  {
    /// Globals of the saved game that the program has none of the name for.
    std::size_t unknown_globals = 0;

    /// Fields of entities of the saved game that the program has no field
    /// of the name for.
    std::size_t unknown_fields = 0;
  };
} // quake

#endif //QUAKE_SAVED_GAME_RESTORE_REPORT_HPP
