#include "any-case-name.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::FindAnyCaseName;

  TEST(AnyCaseNameTest, FindsTheArchivesOfTheReleaseOnSteamByTheirOwnName)
  {
    const std::vector<std::string> names = {"PAK0.PAK", "PAK1.PAK", "steam_autocloud.vdf"};

    EXPECT_EQ(FindAnyCaseName(names, "pak0.pak"), "PAK0.PAK");
    EXPECT_EQ(FindAnyCaseName(names, "pak1.pak"), "PAK1.PAK");
    EXPECT_EQ(FindAnyCaseName(names, "pak2.pak"), std::nullopt);
  }

  TEST(AnyCaseNameTest, FindsANameInTheCaseAskedFor)
  {
    const std::vector<std::string> names = {"pak0.pak", "track02.ogg"};

    EXPECT_EQ(FindAnyCaseName(names, "pak0.pak"), "pak0.pak");
    EXPECT_EQ(FindAnyCaseName(names, "track02.ogg"), "track02.ogg");
  }

  TEST(AnyCaseNameTest, FindsANameOfMixedCase)
  {
    EXPECT_EQ(FindAnyCaseName({"Track02.Ogg"}, "track02.ogg"), "Track02.Ogg");
  }

  TEST(AnyCaseNameTest, TakesTheArchiveInUpperCaseWhereBothAreThereAsGameDataAsksForIt)
  {
    // GameData asks for PAK0.PAK, so the original release's name wins, and
    // a folder of pak0.pak alone is still found
    EXPECT_EQ(FindAnyCaseName({"pak0.pak", "PAK0.PAK"}, "PAK0.PAK"), "PAK0.PAK");
    EXPECT_EQ(FindAnyCaseName({"pak0.pak"}, "PAK0.PAK"), "pak0.pak");
  }

  TEST(AnyCaseNameTest, TakesTheNameInTheCaseAskedForWhereBothAreThere)
  {
    // a file system that tells case apart can hold both
    EXPECT_EQ(FindAnyCaseName({"PAK0.PAK", "pak0.pak"}, "pak0.pak"), "pak0.pak");
  }

  TEST(AnyCaseNameTest, FindsNothingForANameThatIsOnlyAlike)
  {
    const std::vector<std::string> names = {"pak0.pak.bak", "pak0.pa", "xpak0.pak"};

    EXPECT_EQ(FindAnyCaseName(names, "pak0.pak"), std::nullopt);
    EXPECT_EQ(FindAnyCaseName({}, "pak0.pak"), std::nullopt);
  }
}
