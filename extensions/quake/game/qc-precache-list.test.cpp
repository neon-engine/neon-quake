#include "qc-precache-list.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::QcPrecacheList;
  using ::testing::ElementsAre;

  TEST(QcPrecacheListTest, StartsWithNumberZeroThatIsNoFile)
  {
    const QcPrecacheList list;

    EXPECT_EQ(list.GetCount(), 1);
    EXPECT_EQ(list.GetName(0), "");
    EXPECT_EQ(list.Find(""), 0);
  }

  TEST(QcPrecacheListTest, NumbersTheNamesInTheOrderTheyAreAdded)
  {
    QcPrecacheList list;

    EXPECT_EQ(list.Add("maps/start.bsp"), 1);
    EXPECT_EQ(list.Add("progs/player.mdl"), 2);
    EXPECT_EQ(list.Add("progs/eyes.mdl"), 3);
    EXPECT_EQ(list.GetCount(), 4);
    EXPECT_EQ(list.GetName(2), "progs/player.mdl");
    EXPECT_EQ(list.Find("progs/eyes.mdl"), 3);
    EXPECT_THAT(list.GetNames(), ElementsAre("", "maps/start.bsp", "progs/player.mdl", "progs/eyes.mdl"));
  }

  TEST(QcPrecacheListTest, KeepsANameOnce)
  {
    QcPrecacheList list;

    EXPECT_EQ(list.Add("progs/player.mdl"), 1);
    EXPECT_EQ(list.Add("progs/eyes.mdl"), 2);
    EXPECT_EQ(list.Add("progs/player.mdl"), 1);
    EXPECT_EQ(list.Add(""), 0);
    EXPECT_EQ(list.GetCount(), 3);
  }

  TEST(QcPrecacheListTest, FindsNothingForANameThatWasNotAddedAndNamesNothingForANumberThereIsNot)
  {
    QcPrecacheList list;
    list.Add("progs/player.mdl");

    EXPECT_FALSE(list.Find("progs/eyes.mdl").has_value());
    // names are compared exactly
    EXPECT_FALSE(list.Find("progs/Player.mdl").has_value());
    EXPECT_EQ(list.GetName(2), "");
    EXPECT_EQ(list.GetName(-1), "");
  }
}
