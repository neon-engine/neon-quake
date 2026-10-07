#include "scoreboard.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "hud-text.hpp"
#include "status-bar.hpp"

namespace quake
{
  std::vector<HudPicture> Scoreboard::Layout(const PlayerStats &stats)
  {
    // the row of the status bar, which the board takes the place of
    constexpr HudAnchor anchor = HudAnchor::Bottom;
    constexpr std::int32_t top = 176;

    std::vector<HudPicture> pictures;
    pictures.push_back({.name = "scorebar", .x = 0, .y = top, .anchor = anchor, .opacity = StatusBar::background_opacity});

    // snprintf never writes past the array, and says how much it wanted to
    std::array<char, 64> line{};
    const auto add = [&](const int length, const std::int32_t x, const std::int32_t y)
    {
      if (length <= 0) { return; }

      const std::size_t count = std::min(static_cast<std::size_t>(length), line.size() - 1);
      HudText::AddLine(pictures, std::string_view(line.data(), count), x, y, anchor);
    };

    add(
      std::snprintf(line.data(), line.size(), "Monsters:%3i /%3i", stats.killed_monsters, stats.total_monsters),
      8, top + 4);
    add(
      std::snprintf(line.data(), line.size(), "Secrets :%3i /%3i", stats.found_secrets, stats.total_secrets),
      8, top + 12);

    // a time that is no number, or before the start, is shown as none
    const float time = stats.time > 0.0f && stats.time < 1.0e9f ? stats.time : 0.0f;
    const auto seconds = static_cast<std::int32_t>(time);
    add(
      std::snprintf(line.data(), line.size(), "Time :%3i:%02i", seconds / 60, seconds % 60),
      184, top + 4);

    const std::string_view name = std::string_view(stats.level_name).substr(0, longest_name);
    HudText::AddLine(
      pictures, name, 232 - static_cast<std::int32_t>(name.size()) * 4, top + 12, anchor);
    return pictures;
  }
} // quake
