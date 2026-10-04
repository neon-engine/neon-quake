#ifndef QUAKE_STATUS_BAR_OPTIONS_HPP
#define QUAKE_STATUS_BAR_OPTIONS_HPP

#include "status-bar-size.hpp"

namespace quake
{
  /// What a player chose about the status bar.
  struct StatusBarOptions
  {
    StatusBarSize size = StatusBarSize::Full;

    /// Whether the key for the scores is held: the status bar then gives
    /// its place to the counts of the level, see `Scoreboard`.
    bool shows_scores = false;

    /// The size for a number of the original's `viewsize`: under 110
    /// both bars, under 120 the status bar alone, from there on neither.
    [[nodiscard]] static constexpr StatusBarSize SizeOfViewSize(const float view_size)
    {
      if (view_size < 110.0f) { return StatusBarSize::Full; }

      return view_size < 120.0f ? StatusBarSize::BarOnly : StatusBarSize::None;
    }
  };
} // quake

#endif //QUAKE_STATUS_BAR_OPTIONS_HPP
