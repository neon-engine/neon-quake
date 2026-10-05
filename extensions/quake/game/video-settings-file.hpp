#ifndef QUAKE_VIDEO_SETTINGS_FILE_HPP
#define QUAKE_VIDEO_SETTINGS_FILE_HPP

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "menu-options.hpp"

namespace quake
{
  /// The video settings of the player as a settings file of the engine,
  /// `user://settings.yml`, which the engine reads before it shows its
  /// window: on top of the settings of the project, below the command line.
  ///
  /// The game keeps its options in a file of its own, and makes the video
  /// settings so once it runs. Without this file the window comes up as the
  /// project says, over the whole display, and is then seen to become what
  /// the player chose. With it, it comes up as chosen.
  ///
  /// What was never chosen is left out, so that the project decides it.
  struct VideoSettingsFile
  {
    /// Where the engine reads the settings of the player from.
    static constexpr std::string_view path = "user://settings.yml";

    /// The text of the file for these options.
    [[nodiscard]] static std::string Write(const MenuOptions &options)
    {
      static constexpr std::array<std::string_view, 3> modes = {"windowed", "borderless", "fullscreen"};

      std::string text =
        "# The video settings chosen in the menu of the game, which writes this\n"
        "# file anew whenever an option changes. See docs/settings.md of the engine.\n"
        "version: 1\n";

      const auto width = static_cast<int>(options.window_width);
      const auto height = static_cast<int>(options.window_height);
      const bool has_size = width > 0 && height > 0;
      const bool has_mode = options.window_mode >= 0.0f;
      if (has_size || has_mode)
      {
        text += "\nwindow:\n";
        if (has_size)
        {
          text += "  width: " + std::to_string(width) + "\n";
          text += "  height: " + std::to_string(height) + "\n";
        }
        if (has_mode)
        {
          const auto mode = static_cast<std::size_t>(std::clamp(options.window_mode, 0.0f, 2.0f));
          text += "  mode: " + std::string(modes[mode]) + "\n";
        }
      }

      text += "\nrendering:\n";
      text += std::string("  vsync: ") + (options.vertical_sync ? "true" : "false") + "\n";
      if (options.frame_limit >= 0.0f)
      {
        // none, or a number the engine takes
        const int limit = !(options.frame_limit > 0.0f)
                            ? 0
                            : static_cast<int>(std::clamp(
                              options.frame_limit, MenuOptions::least_frame_limit, MenuOptions::most_frame_limit));
        text += "  max_fps: " + std::to_string(limit) + "\n";
      }
      return text;
    }
  };
} // quake

#endif //QUAKE_VIDEO_SETTINGS_FILE_HPP
