#include "intermission.hpp"

#include <limits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"

namespace
{
  using quake::HudAnchor;
  using quake::HudPicture;
  using quake::Intermission;
  using quake::MakeLetter;
  using quake::MakePicture;
  using quake::PlayerStats;
  using ::testing::ElementsAre;

  constexpr HudAnchor center = HudAnchor::Center;

  TEST(IntermissionTest, PutsTheTimeTheSecretsAndTheMonstersNextToThePlaque)
  {
    PlayerStats stats;
    stats.found_secrets = 3;
    stats.total_secrets = 12;
    stats.killed_monsters = 47;
    stats.total_monsters = 105;

    // 12 minutes and 7 seconds
    EXPECT_THAT(Intermission::Layout(stats, 727.6f), ElementsAre(
      MakePicture("gfx/complete.lmp", 64, 24, center),
      MakePicture("gfx/inter.lmp", 0, 56, center),

      MakePicture("num_1", 184, 64, center),
      MakePicture("num_2", 208, 64, center),
      MakePicture("num_colon", 234, 64, center),
      MakePicture("num_0", 246, 64, center),
      MakePicture("num_7", 266, 64, center),

      MakePicture("num_3", 208, 104, center),
      MakePicture("num_slash", 232, 104, center),
      MakePicture("num_1", 264, 104, center),
      MakePicture("num_2", 288, 104, center),

      MakePicture("num_4", 184, 144, center),
      MakePicture("num_7", 208, 144, center),
      MakePicture("num_slash", 232, 144, center),
      MakePicture("num_1", 240, 144, center),
      MakePicture("num_0", 264, 144, center),
      MakePicture("num_5", 288, 144, center)));
  }

  TEST(IntermissionTest, ShowsALevelDoneAtOnceWithNothingInIt)
  {
    EXPECT_THAT(Intermission::Layout({}, 0.0f), ElementsAre(
      MakePicture("gfx/complete.lmp", 64, 24, center),
      MakePicture("gfx/inter.lmp", 0, 56, center),
      MakePicture("num_0", 208, 64, center),
      MakePicture("num_colon", 234, 64, center),
      MakePicture("num_0", 246, 64, center),
      MakePicture("num_0", 266, 64, center),
      MakePicture("num_0", 208, 104, center),
      MakePicture("num_slash", 232, 104, center),
      MakePicture("num_0", 288, 104, center),
      MakePicture("num_0", 208, 144, center),
      MakePicture("num_slash", 232, 144, center),
      MakePicture("num_0", 288, 144, center)));
  }

  TEST(IntermissionTest, KeepsAPictureOfAnotherWidthInTheMiddle)
  {
    const auto pictures = Intermission::Layout({}, 0.0f, 142);
    ASSERT_FALSE(pictures.empty());
    EXPECT_EQ(pictures[0], MakePicture("gfx/complete.lmp", 89, 24, center));

    const auto finale = Intermission::LayoutFinale("", 0.0f, 112);
    ASSERT_FALSE(finale.empty());
    EXPECT_EQ(finale[0], MakePicture("gfx/finale.lmp", 104, 16, center));
  }

  TEST(IntermissionTest, ShowsATimeThatIsNoneAsNothing)
  {
    const auto pictures = Intermission::Layout({}, std::numeric_limits<float>::quiet_NaN());
    ASSERT_EQ(pictures.size(), 12u);
    EXPECT_EQ(pictures[2], MakePicture("num_0", 208, 64, center));
  }

  TEST(IntermissionTest, RevealsTheStoryUnderThePictureOfTheFinale)
  {
    EXPECT_THAT(Intermission::LayoutFinale("Well\ndone", 0.25f), ElementsAre(
      MakePicture("gfx/finale.lmp", 16, 16, center),
      MakeLetter('W', 144, 70, center),
      MakeLetter('e', 152, 70, center),
      MakeLetter('l', 160, 70, center)));

    EXPECT_EQ(Intermission::LayoutFinale("Well\ndone", 10.0f).size(), 9u);
  }
}
