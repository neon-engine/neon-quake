#ifndef QUAKE_MENU_OPTIONS_HPP
#define QUAKE_MENU_OPTIONS_HPP

#include <string_view>

namespace quake
{
  /// The settings the options screen shows and changes. The host keeps
  /// them: it hands them to the menus to be shown, and is told with a
  /// `MenuAction` what to set one to, which Set() does for it.
  ///
  /// They start as the original starts them. The names are those of the
  /// original's settings, where it had one for the same thing.
  struct MenuOptions
  {
    static constexpr std::string_view screen_size_name = "viewsize";
    static constexpr std::string_view gamma_name = "gamma";
    static constexpr std::string_view mouse_speed_name = "sensitivity";
    static constexpr std::string_view stick_speed_name = "joy_sensitivity";
    static constexpr std::string_view music_volume_name = "bgmvolume";
    static constexpr std::string_view sound_volume_name = "volume";
    static constexpr std::string_view always_run_name = "always_run";
    static constexpr std::string_view invert_mouse_name = "invert_mouse";

    /// How much of the screen the view takes, from 30 to 120 in steps of
    /// 10. From 110 on the bars of the status bar go away, see
    /// StatusBarOptions::SizeOfViewSize(). It starts with the status bar
    /// alone, as the ports of today are mostly played; the original
    /// started at 100, with the bar of what is carried above it.
    float screen_size = 110.0f;

    /// The brightness, from 1, as the pictures are, down to 0.5, the
    /// brightest, in steps of 0.05. The screen calls it brightness, and
    /// its slider goes to the right as the number goes down.
    float gamma = 1.0f;

    /// How far the mouse turns the view, from 1 to 11 in steps of 0.5.
    float mouse_speed = 3.0f;

    /// How far the stick of a controller turns the view, from 1 to 11 in
    /// steps of 0.5, apart from the mouse. The original had no such option.
    float stick_speed = 3.0f;

    /// How loud the music is, from 0 to 1 in steps of 0.1.
    float music_volume = 1.0f;

    /// How loud the sounds are, from 0 to 1 in steps of 0.1.
    float sound_volume = 0.7f;

    /// Whether the player runs without the key for it.
    bool always_run = false;

    /// Whether moving the mouse forward looks down.
    bool invert_mouse = false;

    /// Sets the option of a name, as a `MenuAction` of the kind
    /// `SetOption` asks. False for a name that is none of them.
    constexpr bool Set(const std::string_view name, const float value)
    {
      if (name == screen_size_name) { screen_size = value; }
      else if (name == gamma_name) { gamma = value; }
      else if (name == mouse_speed_name) { mouse_speed = value; }
      else if (name == stick_speed_name) { stick_speed = value; }
      else if (name == music_volume_name) { music_volume = value; }
      else if (name == sound_volume_name) { sound_volume = value; }
      else if (name == always_run_name) { always_run = value != 0.0f; }
      else if (name == invert_mouse_name) { invert_mouse = value != 0.0f; }
      else { return false; }

      return true;
    }

    bool operator==(const MenuOptions &) const = default;
  };
} // quake

#endif //QUAKE_MENU_OPTIONS_HPP
