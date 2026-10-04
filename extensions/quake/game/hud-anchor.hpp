#ifndef QUAKE_HUD_ANCHOR_HPP
#define QUAKE_HUD_ANCHOR_HPP

#include <cstdint>

namespace quake
{
  /// Where on a real screen the screen of 320 by 200 that a `HudPicture`
  /// counts in is put, once it is made larger by a whole or broken number.
  enum class HudAnchor : std::uint8_t
  {
    /// In the middle from left to right, with its lower edge on the lower
    /// edge of the real screen: the status bar and what is merged into it.
    Bottom,

    /// In the middle both ways: the screen between two levels, and text
    /// in the middle of the view.
    Center,
  };
} // quake

#endif //QUAKE_HUD_ANCHOR_HPP
