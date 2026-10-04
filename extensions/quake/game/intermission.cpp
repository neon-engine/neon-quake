#include "intermission.hpp"

#include "center-text.hpp"
#include "hud-text.hpp"

namespace quake
{
  std::vector<HudPicture> Intermission::Layout(
    const PlayerStats &stats,
    const float completed_seconds,
    const std::int32_t width_of_complete)
  {
    constexpr HudAnchor anchor = HudAnchor::Center;
    constexpr std::int32_t time_row = 64;
    constexpr std::int32_t secrets_row = 104;
    constexpr std::int32_t monsters_row = 144;

    std::vector<HudPicture> pictures;
    pictures.push_back({.name = "gfx/complete.lmp", .x = 160 - width_of_complete / 2, .y = 24, .anchor = anchor});
    pictures.push_back({.name = "gfx/inter.lmp", .x = 0, .y = 56, .anchor = anchor});

    // a time that is no number, or before the start, is shown as none
    const float time = completed_seconds > 0.0f && completed_seconds < 1.0e9f ? completed_seconds : 0.0f;
    const auto seconds = static_cast<std::int32_t>(time);
    HudText::AddNumber(pictures, seconds / 60, 160, time_row, anchor);
    pictures.push_back({.name = "num_colon", .x = 234, .y = time_row, .anchor = anchor});
    pictures.push_back({.name = HudText::GetDigitName(seconds % 60 / 10), .x = 246, .y = time_row, .anchor = anchor});
    pictures.push_back({.name = HudText::GetDigitName(seconds % 10), .x = 266, .y = time_row, .anchor = anchor});

    HudText::AddNumber(pictures, stats.found_secrets, 160, secrets_row, anchor);
    pictures.push_back({.name = "num_slash", .x = 232, .y = secrets_row, .anchor = anchor});
    HudText::AddNumber(pictures, stats.total_secrets, 240, secrets_row, anchor);

    HudText::AddNumber(pictures, stats.killed_monsters, 160, monsters_row, anchor);
    pictures.push_back({.name = "num_slash", .x = 232, .y = monsters_row, .anchor = anchor});
    HudText::AddNumber(pictures, stats.total_monsters, 240, monsters_row, anchor);
    return pictures;
  }

  std::vector<HudPicture> Intermission::LayoutFinale(
    const std::string_view text,
    const float seconds_shown,
    const std::int32_t width_of_finale)
  {
    std::vector<HudPicture> pictures;
    pictures.push_back({
      .name = "gfx/finale.lmp",
      .x = (320 - width_of_finale) / 2,
      .y = 16,
      .anchor = HudAnchor::Center,
    });

    const std::vector<HudPicture> letters = CenterText::LayoutRevealed(text, seconds_shown);
    pictures.insert(pictures.end(), letters.begin(), letters.end());
    return pictures;
  }
} // quake
