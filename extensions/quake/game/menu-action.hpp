#ifndef QUAKE_MENU_ACTION_HPP
#define QUAKE_MENU_ACTION_HPP

#include <cstdint>
#include <string_view>

#include "menu-action-kind.hpp"

namespace quake
{
  /// One thing the menus ask of the host after a key. The menus change
  /// nothing but themselves: they start no game, set no option, and play
  /// no sound. The host does, in the order of the list it is given.
  struct MenuAction
  {
    /// The sound of moving the cursor.
    static constexpr std::string_view move_sound = "misc/menu1.wav";

    /// The sound of going into a screen.
    static constexpr std::string_view enter_sound = "misc/menu2.wav";

    /// The sound of changing a setting.
    static constexpr std::string_view change_sound = "misc/menu3.wav";

    MenuActionKind kind = MenuActionKind::None;

    /// Which option, or which sound. The names are constants of the
    /// program, so the view stays good for ever.
    std::string_view name;

    /// What the option is to be. For one that is on or off, 1 or 0.
    float value = 0.0f;

    /// Which saved game, from 0 to 11: the file `s0.sav` to `s11.sav`
    /// of the original.
    std::int32_t slot = 0;

    bool operator==(const MenuAction &) const = default;
  };
} // quake

#endif //QUAKE_MENU_ACTION_HPP
