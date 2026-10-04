#ifndef QUAKE_SCOREBOARD_HPP
#define QUAKE_SCOREBOARD_HPP

#include <cstddef>
#include <vector>

#include "hud-picture.hpp"
#include "player-stats.hpp"

namespace quake
{
  /// The counts of a level, shown in the place of the status bar while the
  /// key for the scores is held, and when the player is dead: the picture
  /// `scorebar`, and over it, in letters of the console, the monsters
  /// killed and the secrets found of how many, the time played, and the
  /// name of the level. It is the one of a game alone; a game of several
  /// players has another, which is not here.
  ///
  /// ```
  /// Monsters: 12 / 34       Time :  5:07
  /// Secrets :  1 /  6     the Slipgate Complex
  /// ```
  struct Scoreboard
  {
    /// How many letters of the name of a level are shown: what the
    /// original kept of it. The name is put around a middle, so a longer
    /// one would leave the screen.
    static constexpr std::size_t longest_name = 40;

    /// What to draw, in order, with `HudAnchor::Bottom`.
    [[nodiscard]] static std::vector<HudPicture> Layout(const PlayerStats &stats);
  };
} // quake

#endif //QUAKE_SCOREBOARD_HPP
