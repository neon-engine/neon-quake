#include "scoreboard.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"

namespace
{
  using quake::HudPicture;
  using quake::MakeLetters;
  using quake::MakeBackground;
  using quake::MakePicture;
  using quake::PlayerStats;
  using quake::Scoreboard;
  using ::testing::ElementsAreArray;

  /// What the scoreboard is expected to be: the picture and four lines.
  std::vector<HudPicture> expect_board(
    const std::string_view monsters,
    const std::string_view secrets,
    const std::string_view time,
    const std::string_view name,
    const std::int32_t name_x)
  {
    std::vector<HudPicture> all = {MakeBackground("scorebar", 0, 176)};
    for (const auto &list : {
           MakeLetters(monsters, 8, 180), MakeLetters(secrets, 8, 188), MakeLetters(time, 184, 180),
           MakeLetters(name, name_x, 188),
         })
    {
      all.insert(all.end(), list.begin(), list.end());
    }
    return all;
  }

  TEST(ScoreboardTest, WritesTheCountsTheTimeAndTheNameOfTheLevelOverItsPicture)
  {
    PlayerStats stats;
    stats.killed_monsters = 12;
    stats.total_monsters = 34;
    stats.found_secrets = 1;
    stats.total_secrets = 6;
    stats.time = 307.9f;
    stats.level_name = "the Slipgate Complex";

    // a name of 20 letters around x 232
    EXPECT_THAT(Scoreboard::Layout(stats), ElementsAreArray(expect_board(
      "Monsters: 12 / 34", "Secrets :  1 /  6", "Time :  5:07", "the Slipgate Complex", 152)));
  }

  TEST(ScoreboardTest, WritesALevelWithNothingToCountAndNoName)
  {
    EXPECT_THAT(Scoreboard::Layout({}), ElementsAreArray(expect_board(
      "Monsters:  0 /  0", "Secrets :  0 /  0", "Time :  0:00", "", 232)));
  }

  TEST(ScoreboardTest, WritesLargeCountsAndALongTime)
  {
    PlayerStats stats;
    stats.killed_monsters = 1234;
    stats.total_monsters = 5678;
    stats.time = 3600.0f * 3.0f + 59.0f;

    EXPECT_THAT(Scoreboard::Layout(stats), ElementsAreArray(expect_board(
      "Monsters:1234 /5678", "Secrets :  0 /  0", "Time :180:59", "", 232)));
  }

  TEST(ScoreboardTest, ShowsFortyLettersOfANameAtTheMost)
  {
    PlayerStats stats;
    stats.level_name = std::string(40, 'n') + "more";

    EXPECT_THAT(Scoreboard::Layout(stats), ElementsAreArray(expect_board(
      "Monsters:  0 /  0", "Secrets :  0 /  0", "Time :  0:00", std::string(40, 'n'), 72)));
  }

  TEST(ScoreboardTest, ShowsATimeThatIsNoneAsNothing)
  {
    PlayerStats stats;
    stats.time = -3.0f;
    const auto before = Scoreboard::Layout(stats);
    stats.time = std::numeric_limits<float>::infinity();
    const auto endless = Scoreboard::Layout(stats);

    const auto expected = expect_board("Monsters:  0 /  0", "Secrets :  0 /  0", "Time :  0:00", "", 232);
    EXPECT_THAT(before, ElementsAreArray(expected));
    EXPECT_THAT(endless, ElementsAreArray(expected));
  }
}
