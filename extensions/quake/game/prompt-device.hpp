#ifndef QUAKE_PROMPT_DEVICE_HPP
#define QUAKE_PROMPT_DEVICE_HPP

#include <cstdint>

namespace quake
{
  /// What the player holds, as far as the pictures of its buttons differ.
  enum class PromptDevice : std::uint8_t
  {
    /// The keyboard and the mouse.
    Keyboard,

    /// An Xbox controller, and any gamepad that does not say what it is:
    /// A below and B to the right.
    Xbox,

    /// A PlayStation controller, of the 4 or of the 5: the cross below and
    /// the circle to the right.
    PlayStation,

    /// A Switch controller: B below and A to the right, and A is the one
    /// that chooses.
    Switch,
  };
} // quake

#endif //QUAKE_PROMPT_DEVICE_HPP
