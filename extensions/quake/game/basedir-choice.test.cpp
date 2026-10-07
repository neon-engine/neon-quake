#include "basedir-choice.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::Basedir;
  using quake::BasedirChoice;

  const std::vector<Basedir> two = {
    {.name = "librequake", .data_folder = "assets://basedirs/librequake/id1/"},
    {.name = "steam", .data_folder = "assets://basedirs/steam/id1/"},
  };

  TEST(BasedirChoiceTest, CannotStartWithoutData)
  {
    const auto choice = BasedirChoice::Of({}, "steam");

    EXPECT_EQ(choice.chosen, -1);
    EXPECT_FALSE(choice.asks);
    EXPECT_NE(choice.problem.find("There is no data of the game"), std::string::npos);
  }

  TEST(BasedirChoiceTest, TakesTheOnlyOneWithoutAsking)
  {
    const auto choice = BasedirChoice::Of({two[1]}, "");

    EXPECT_EQ(choice.chosen, 0);
    EXPECT_FALSE(choice.asks);
  }

  TEST(BasedirChoiceTest, TakesTheOneThatWasChosenBefore)
  {
    const auto choice = BasedirChoice::Of(two, "steam");

    EXPECT_EQ(choice.chosen, 1);
    EXPECT_FALSE(choice.asks);
    EXPECT_TRUE(choice.warning.empty());
  }

  TEST(BasedirChoiceTest, AsksWhenThereAreSeveralAndNoneWasChosen)
  {
    const auto choice = BasedirChoice::Of(two, "");

    EXPECT_EQ(choice.chosen, -1);
    EXPECT_TRUE(choice.asks);
  }

  TEST(BasedirChoiceTest, AsksAndSaysSoWhenTheOneChosenBeforeIsGone)
  {
    const auto choice = BasedirChoice::Of(two, "quake-ex");

    EXPECT_TRUE(choice.asks);
    EXPECT_NE(choice.warning.find("quake-ex"), std::string::npos);
  }

  TEST(BasedirChoiceTest, TakesTheOnlyOneThereIsWhenTheOneChosenBeforeIsGone)
  {
    const auto choice = BasedirChoice::Of({two[0]}, "steam");

    EXPECT_EQ(choice.chosen, 0);
    EXPECT_FALSE(choice.warning.empty());
  }

  TEST(BasedirChoiceTest, TakesAnId1StraightUnderTheFolderOverWhatWasChosenWithoutAWarning)
  {
    const auto choice = BasedirChoice::Of({{.name = "default", .data_folder = "assets://basedirs/id1/"}}, "steam");

    EXPECT_EQ(choice.chosen, 0);
    EXPECT_TRUE(choice.warning.empty());
  }

  TEST(BasedirChoiceTest, AsksWhenAskedToEvenWithOneThatWasChosen)
  {
    const auto choice = BasedirChoice::Of(two, "steam", true);

    EXPECT_TRUE(choice.asks);
    EXPECT_EQ(choice.chosen, -1);
  }
} // namespace
