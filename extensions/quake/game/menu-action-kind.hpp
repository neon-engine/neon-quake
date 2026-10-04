#ifndef QUAKE_MENU_ACTION_KIND_HPP
#define QUAKE_MENU_ACTION_KIND_HPP

#include <cstdint>

namespace quake
{
  /// What a `MenuAction` asks of the host.
  enum class MenuActionKind : std::uint8_t
  {
    /// Nothing. Menu::Press() never gives one of these.
    None,

    /// End the game that runs, if one does, and start the level `start`
    /// for one player. The level itself asks for the skill.
    NewGame,

    /// Load the saved game of the slot `slot`.
    LoadGame,

    /// Save the game that runs into the slot `slot`.
    SaveGame,

    /// End the program.
    Quit,

    /// The menus closed with nothing else to do: go on playing.
    Resume,

    /// Set the option `name` to `value`, see `MenuOptions`.
    SetOption,

    /// Play the sound `name`, a file of the paks under `sound/`, as a
    /// sound of the screen and not of a place in the level.
    PlaySound,
  };
} // quake

#endif //QUAKE_MENU_ACTION_KIND_HPP
