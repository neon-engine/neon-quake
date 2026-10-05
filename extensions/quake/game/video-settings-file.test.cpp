#include "video-settings-file.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::MenuOptions;
  using quake::VideoSettingsFile;

  constexpr std::string_view head =
    "# The video settings chosen in the menu of the game, which writes this\n"
    "# file anew whenever an option changes. See docs/settings.md of the engine.\n"
    "version: 1\n";

  TEST(VideoSettingsFileTest, WritesWhatThePlayerChoseAsTheEngineReadsIt)
  {
    MenuOptions options;
    options.window_mode = 0.0f;
    options.window_width = 1280.0f;
    options.window_height = 720.0f;
    options.vertical_sync = true;
    options.frame_limit = 144.0f;

    EXPECT_EQ(VideoSettingsFile::Write(options), std::string(head) +
      "\nwindow:\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  mode: windowed\n"
      "\nrendering:\n"
      "  vsync: true\n"
      "  max_fps: 144\n");
  }

  TEST(VideoSettingsFileTest, NamesTheThreeModesOfTheWindow)
  {
    MenuOptions options;
    options.window_mode = 1.0f;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  mode: borderless\n"), std::string::npos);
    options.window_mode = 2.0f;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  mode: fullscreen\n"), std::string::npos);
    options.window_mode = 9.0f;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  mode: fullscreen\n"), std::string::npos) << "held to the last";
  }

  TEST(VideoSettingsFileTest, LeavesOutWhatWasNeverChosenSoThatTheProjectDecidesIt)
  {
    // as the options start: no mode, no size, no limit, and vertical sync off
    EXPECT_EQ(VideoSettingsFile::Write(MenuOptions{}), std::string(head) +
      "\nrendering:\n"
      "  vsync: false\n");

    // a size is two numbers, and none with one of them
    MenuOptions options;
    options.window_width = 1280.0f;
    EXPECT_EQ(VideoSettingsFile::Write(options).find("window:"), std::string::npos);
  }

  TEST(VideoSettingsFileTest, WritesNoLimitAsNoneAndHoldsANumberToWhatTheEngineTakes)
  {
    MenuOptions options;
    options.frame_limit = 90.0f;
    options.unlimited_frames = true;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  max_fps: 0\n"), std::string::npos) << "whatever the number";

    options.unlimited_frames = false;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  max_fps: 90\n"), std::string::npos);
    options.frame_limit = 12.0f;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  max_fps: 30\n"), std::string::npos);
    options.frame_limit = 1000.0f;
    EXPECT_NE(VideoSettingsFile::Write(options).find("  max_fps: 300\n"), std::string::npos);
  }
}
