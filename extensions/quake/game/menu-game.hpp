#ifndef QUAKE_MENU_GAME_HPP
#define QUAKE_MENU_GAME_HPP

#include <vector>
#include <array>
#include <cstddef>
#include <string>

#include "menu-size.hpp"

namespace quake
{
  /// What the menus have to know about the game around them, which the
  /// host tells them with every key and every drawing.
  struct MenuGame
  {
    /// How many saved games there is room for, as in the original.
    static constexpr std::size_t slot_count = 12;

    /// Whether a game runs. A new game then asks first, since it ends
    /// the one that runs, and only then is there something to save.
    bool is_running = false;

    /// Whether the screen between two levels shows. Nothing can be saved
    /// then, as in the original.
    bool is_in_intermission = false;

    /// What each saved game says about itself: in the original the name
    /// of the level and the count of kills. An empty one is a slot with
    /// no saved game: it shows as unused, and cannot be loaded.
    std::array<std::string, slot_count> slots;

    /// The sizes the display offers the window, the largest first, which
    /// the video settings go through. Empty when there is no display.
    std::vector<MenuSize> display_sizes;
  };
} // quake

#endif //QUAKE_MENU_GAME_HPP
