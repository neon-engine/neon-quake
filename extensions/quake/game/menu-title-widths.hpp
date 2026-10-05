#ifndef QUAKE_MENU_TITLE_WIDTHS_HPP
#define QUAKE_MENU_TITLE_WIDTHS_HPP

#include <cstdint>

namespace quake
{
  /// How wide the pictures are that the menus put around the middle of
  /// the screen: the title on top of each screen. These are the widths of
  /// the original. The data of another game has them wider or narrower,
  /// and a host that knows the real widths hands them in, so that the
  /// titles stay in the middle.
  struct MenuTitleWidths
  {
    /// `gfx/ttl_main.lmp`
    std::int32_t main = 96;

    /// `gfx/ttl_sgl.lmp`
    std::int32_t single_player = 128;

    /// `gfx/p_load.lmp`
    std::int32_t load = 104;

    /// `gfx/p_save.lmp`
    std::int32_t save = 104;

    /// `gfx/p_option.lmp`
    std::int32_t options = 144;
  };
} // quake

#endif //QUAKE_MENU_TITLE_WIDTHS_HPP
