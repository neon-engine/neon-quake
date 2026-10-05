#ifndef QUAKE_MENU_SIZE_HPP
#define QUAKE_MENU_SIZE_HPP

#include <cstdint>

namespace quake
{
  /// A size the display offers the window, in points, which the screen of
  /// the video settings lets a player choose.
  struct MenuSize
  {
    std::int32_t width = 0;
    std::int32_t height = 0;

    bool operator==(const MenuSize &) const = default;
  };
} // quake

#endif //QUAKE_MENU_SIZE_HPP
