#ifndef QUAKE_PROMPTS_HPP
#define QUAKE_PROMPTS_HPP

#include <string_view>

#include "prompt-device.hpp"

namespace quake
{
  /// The pictures of the buttons the menus are worked with, for what the
  /// player holds. A picture is named as a `HudPicture` names it: a file of
  /// the folder `prompts` of the game's own assets, 16 pixels wide and high.
  ///
  /// The menus are bound by where a button is, not by what is written on
  /// it: the one below chooses and the one to the right goes back
  /// (`quake.input.yml`). So a Switch controller, whose letters are the
  /// other way around, shows B for choosing and A for going back.
  struct Prompts
  {
    /// How many pixels wide and high a picture is.
    static constexpr int size = 16;

    /// The button that chooses an item: Enter, or the button below.
    [[nodiscard]] static constexpr std::string_view Select(const PromptDevice device)
    {
      switch (device)
      {
        case PromptDevice::Xbox: return "prompts/xbox-a.png";
        case PromptDevice::PlayStation: return "prompts/playstation-cross.png";
        case PromptDevice::Switch: return "prompts/switch-b.png";
        default: return "prompts/key-enter.png";
      }
    }

    /// The button that goes back: Escape, or the button to the right.
    [[nodiscard]] static constexpr std::string_view Back(const PromptDevice device)
    {
      switch (device)
      {
        case PromptDevice::Xbox: return "prompts/xbox-b.png";
        case PromptDevice::PlayStation: return "prompts/playstation-circle.png";
        case PromptDevice::Switch: return "prompts/switch-a.png";
        default: return "prompts/key-escape.png";
      }
    }
  };
} // quake

#endif //QUAKE_PROMPTS_HPP
