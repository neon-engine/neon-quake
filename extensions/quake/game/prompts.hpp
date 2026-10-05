#ifndef QUAKE_PROMPTS_HPP
#define QUAKE_PROMPTS_HPP

#include <string_view>

#include "menu-key.hpp"
#include "prompt-device.hpp"

namespace quake
{
  /// The pictures of the buttons the menus are worked with, for what the
  /// player holds. A picture is named as a `HudPicture` names it: a file of
  /// the folder `prompts` of the game's own assets, 16 pixels wide and high
  /// but for the Enter key, which is twice that.
  ///
  /// The menus are bound by where a button is (`quake.input.yml`): the one
  /// below chooses and the one to the right goes back, as on an Xbox and a
  /// PlayStation controller. A Switch controller has its letters the other
  /// way around, and its games choose with A, to the right, and go back
  /// with B, below. So with one in the hands the two are told apart the
  /// other way around, see `Pressed`, and the pictures say so.
  struct Prompts
  {
    /// How many pixels wide and high a picture of a button is.
    static constexpr int size = 16;

    /// How many pixels wide and high the picture of that name is.
    [[nodiscard]] static constexpr int SizeOf(const std::string_view name)
    {
      return name == "prompts/key-enter.png" ? 2 * size : size;
    }

    /// What a press in the menus means with that in the hands: with a
    /// Switch controller, choosing and going back change places.
    [[nodiscard]] static constexpr MenuKey Pressed(const MenuKey key, const PromptDevice device)
    {
      if (device != PromptDevice::Switch) { return key; }
      if (key == MenuKey::Select) { return MenuKey::Back; }
      if (key == MenuKey::Back) { return MenuKey::Select; }
      return key;
    }

    /// The button that chooses an item: Enter, the button below, or A of a
    /// Switch controller.
    [[nodiscard]] static constexpr std::string_view Select(const PromptDevice device)
    {
      switch (device)
      {
        case PromptDevice::Xbox: return "prompts/xbox-a.png";
        case PromptDevice::PlayStation: return "prompts/playstation-cross.png";
        case PromptDevice::Switch: return "prompts/switch-a.png";
        default: return "prompts/key-enter.png";
      }
    }

    /// The button that goes back: Escape, the button to the right, or B of
    /// a Switch controller.
    [[nodiscard]] static constexpr std::string_view Back(const PromptDevice device)
    {
      switch (device)
      {
        case PromptDevice::Xbox: return "prompts/xbox-b.png";
        case PromptDevice::PlayStation: return "prompts/playstation-circle.png";
        case PromptDevice::Switch: return "prompts/switch-b.png";
        default: return "prompts/key-escape.png";
      }
    }
  };
} // quake

#endif //QUAKE_PROMPTS_HPP
