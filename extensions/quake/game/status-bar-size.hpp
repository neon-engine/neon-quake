#ifndef QUAKE_STATUS_BAR_SIZE_HPP
#define QUAKE_STATUS_BAR_SIZE_HPP

#include <cstdint>

namespace quake
{
  /// How much of the status bar shows: the steps of the original's
  /// `viewsize`, which made the view larger by taking the bars away.
  enum class StatusBarSize : std::uint8_t
  {
    /// The status bar and, above it, the bar of what is carried.
    Full,

    /// The status bar alone.
    BarOnly,

    /// Neither. The level's counts still show while they are asked for,
    /// or when the player is dead.
    None,
  };
} // quake

#endif //QUAKE_STATUS_BAR_SIZE_HPP
