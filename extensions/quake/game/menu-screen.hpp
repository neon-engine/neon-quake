#ifndef QUAKE_MENU_SCREEN_HPP
#define QUAKE_MENU_SCREEN_HPP

#include <cstdint>

namespace quake
{
  /// Which screen of the menus is open.
  enum class MenuScreen : std::uint8_t
  {
    /// The menus are closed and the game goes on.
    None,

    /// The first screen: single player, options, help, quit.
    Main,

    /// New game, load, save.
    SinglePlayer,

    /// The question before a new game ends the one that runs.
    NewGame,

    /// The twelve saved games, to load one.
    Load,

    /// The twelve saved games, to save over one.
    Save,

    /// The settings.
    Options,

    /// How the game is shown: the mode of the window, its size, and
    /// vertical sync. Reached from the settings. The original had a
    /// screen of the modes of its display here.
    Video,
    /// The six pages of help.
    Help,

    /// The question before the program ends.
    Quit,
  };
} // quake

#endif //QUAKE_MENU_SCREEN_HPP
