#ifndef QUAKE_MENU_KEY_HPP
#define QUAKE_MENU_KEY_HPP

#include <cstdint>

namespace quake
{
  /// What a player pressed while the menus are open, as the menus see it.
  /// A host maps its keys, the buttons of a pad, and the wheel of the
  /// mouse onto these, and hands each press to Menu::Press().
  enum class MenuKey : std::uint8_t
  {
    /// The arrows of the original.
    Up,
    Down,
    Left,
    Right,

    /// The original's enter: goes into what the cursor is on, or changes
    /// it. On a screen that asks a question it says yes as well.
    Select,

    /// The original's escape: one screen back, and out of the first one.
    /// On a screen that asks a question it says no.
    Back,

    /// The letter y. It answers a question, and is nothing anywhere else.
    Yes,

    /// The letter n. It answers a question, and is nothing anywhere else.
    No,
  };
} // quake

#endif //QUAKE_MENU_KEY_HPP
