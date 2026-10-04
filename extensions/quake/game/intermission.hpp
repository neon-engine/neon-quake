#ifndef QUAKE_INTERMISSION_HPP
#define QUAKE_INTERMISSION_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "hud-picture.hpp"
#include "player-stats.hpp"

namespace quake
{
  /// The screens between two levels and at the end of an episode, as
  /// lists of pictures to draw. No status bar is drawn under them.
  ///
  /// Their places are those of the original, in the screen of 320 by 200
  /// that `HudPicture` describes, with `HudAnchor::Center`.
  struct Intermission
  {
    /// How wide the pictures `gfx/complete.lmp` and `gfx/finale.lmp` of
    /// the original are. Both are put around the middle of the screen.
    /// The data of another game may have them narrower, and a host that
    /// knows the real width hands it in, so that they stay in the middle,
    /// as the ports see to.
    static constexpr std::int32_t complete_width = 192;
    static constexpr std::int32_t finale_width = 288;

    /// The screen after a level: `gfx/complete.lmp` on top, the plaque
    /// `gfx/inter.lmp` with the words Time, Secrets, and Kills on the
    /// left, and next to each word its number in the large digits: the
    /// time as minutes, `num_colon`, and two digits of seconds, then the
    /// secrets and the monsters as found, `num_slash`, and how many
    /// there are.
    ///
    /// `completed_seconds` is the time of the level at the moment it was
    /// done, which stands still while the screen shows.
    [[nodiscard]] static std::vector<HudPicture> Layout(
      const PlayerStats &stats,
      float completed_seconds,
      std::int32_t width_of_complete = complete_width);

    /// The screen at the end of an episode: `gfx/finale.lmp` on top, and
    /// the text of the game code under it, which appears letter after
    /// letter, see CenterText::LayoutRevealed().
    ///
    /// `seconds_shown` is for how long the text has been there.
    [[nodiscard]] static std::vector<HudPicture> LayoutFinale(
      std::string_view text,
      float seconds_shown,
      std::int32_t width_of_finale = finale_width);
  };
} // quake

#endif //QUAKE_INTERMISSION_HPP
