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
    static constexpr std::string_view window_mode_name = "vid_mode";
    static constexpr std::string_view window_width_name = "vid_width";
    static constexpr std::string_view window_height_name = "vid_height";
    static constexpr std::string_view vertical_sync_name = "vid_vsync";
    static constexpr std::string_view frame_limit_name = "vid_maxfps";

    /// The least and the most frames a second a limit is, and how far a
    /// step of its slider goes.
    static constexpr float least_frame_limit = 30.0f;
    static constexpr float most_frame_limit = 300.0f;
    static constexpr float frame_limit_step = 10.0f;
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

    /// How the window is shown: 0 as a window, 1 without borders over the
    /// whole display, 2 as the one thing on a display that is switched to
    /// the size of the window. Below 0 for as the game was started, which
    /// the host makes one of the three before the menu shows it.
    float window_mode = -1.0f;

    /// The size of the window, in points. 0 for as the game was started.
    float window_width = 0.0f;
    float window_height = 0.0f;

    /// Whether a frame waits for the screen before it is shown. Off, so
    /// that the game draws every frame it can, as it is played today.
    bool vertical_sync = false;

    /// The most frames a second, from 30 to 300, or 0 for as many as can
    /// be drawn. Below 0 for as the game was started, which the host makes
    /// one of them before the menu shows it.
    float frame_limit = -1.0f;

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
      else if (name == window_mode_name) { window_mode = value; }
      else if (name == window_width_name) { window_width = value; }
      else if (name == window_height_name) { window_height = value; }
      else if (name == vertical_sync_name) { vertical_sync = value != 0.0f; }
      else if (name == frame_limit_name) { frame_limit = value; }
      else if (name == always_run_name) { always_run = value != 0.0f; }
      else if (name == invert_mouse_name) { invert_mouse = value != 0.0f; }
      else { return false; }

      return true;
    }

    bool operator==(const MenuOptions &) const = default;
  };
} // quake

#endif //QUAKE_MENU_OPTIONS_HPP
