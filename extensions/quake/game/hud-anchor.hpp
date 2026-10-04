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

    /// From the upper left corner of the real screen, and smaller than the
    /// rest, as the console of the original is: the lines the game tells
    /// the player. x and y count from that corner.
    TopLeft,

    /// From the middle of the real screen, as small as `TopLeft`: the
    /// cross a player aims with. x and y count from the middle.
    MiddleSmall,
  };
} // quake

#endif //QUAKE_HUD_ANCHOR_HPP
