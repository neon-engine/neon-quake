#include "hud-text.hpp"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"

namespace
{
  using quake::HudAnchor;
  using quake::HudPicture;
  using quake::HudText;
  using quake::MakeLetter;
  using quake::MakePicture;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  TEST(HudTextTest, PutsEachLetterOfALineEightPixelsFurtherAndLeavesSpacesOut)
  {
    std::vector<HudPicture> pictures;
    HudText::AddLine(pictures, "Hi you", 16, 100, HudAnchor::Center);

    EXPECT_THAT(pictures, ElementsAre(
      MakeLetter('H', 16, 100, HudAnchor::Center),
      MakeLetter('i', 24, 100, HudAnchor::Center),
      MakeLetter('y', 40, 100, HudAnchor::Center),
      MakeLetter('o', 48, 100, HudAnchor::Center),
      MakeLetter('u', 56, 100, HudAnchor::Center)));
  }

  TEST(HudTextTest, WritesInBronzeByAdding128ToWhatIsNotBronzeYet)
  {
    std::vector<HudPicture> pictures;
    HudText::AddLine(pictures, "A \xc1", 0, 0, HudAnchor::Bottom, true);

    // the space becomes the blank of the bronze set, which is drawn
    EXPECT_THAT(pictures, ElementsAre(
      MakeLetter('A' + 128, 0, 0), MakeLetter(' ' + 128, 8, 0), MakeLetter(0xc1, 16, 0)));
  }

  TEST(HudTextTest, KeepsTheBytesOver127AsTheLettersTheyAre)
  {
    std::vector<HudPicture> pictures;
    HudText::AddLine(pictures, "\xff\x12", 0, 0, HudAnchor::Bottom);

    EXPECT_THAT(pictures, ElementsAre(MakeLetter(255, 0, 0), MakeLetter(18, 8, 0)));
  }

  TEST(HudTextTest, AddsToWhatTheListHasAndNothingForAnEmptyLine)
  {
    std::vector<HudPicture> pictures;
    HudText::AddLine(pictures, "", 0, 0, HudAnchor::Bottom);
    EXPECT_THAT(pictures, IsEmpty());

    HudText::AddLine(pictures, "a", 0, 0, HudAnchor::Bottom);
    HudText::AddLine(pictures, "b", 0, 8, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(MakeLetter('a', 0, 0), MakeLetter('b', 0, 8)));
  }

  TEST(HudTextTest, MovesANumberToTheRightOfItsThreeDigits)
  {
    std::vector<HudPicture> pictures;
    HudText::AddNumber(pictures, 100, 136, 176, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("num_1", 136, 176), MakePicture("num_0", 160, 176), MakePicture("num_0", 184, 176)));

    pictures.clear();
    HudText::AddNumber(pictures, 47, 136, 176, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(MakePicture("num_4", 160, 176), MakePicture("num_7", 184, 176)));

    pictures.clear();
    HudText::AddNumber(pictures, 0, 136, 176, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(MakePicture("num_0", 184, 176)));
  }

  TEST(HudTextTest, WritesANumberInRedDigits)
  {
    std::vector<HudPicture> pictures;
    HudText::AddNumber(pictures, 25, 0, 0, HudAnchor::Center, true);

    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("anum_2", 24, 0, HudAnchor::Center), MakePicture("anum_5", 48, 0, HudAnchor::Center)));
  }

  TEST(HudTextTest, ShowsANumberTooLargeAs999AndOneBelowNothingWithAMinus)
  {
    std::vector<HudPicture> pictures;
    HudText::AddNumber(pictures, 12345, 0, 0, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("num_9", 0, 0), MakePicture("num_9", 24, 0), MakePicture("num_9", 48, 0)));

    pictures.clear();
    HudText::AddNumber(pictures, -5, 0, 0, HudAnchor::Bottom);
    EXPECT_THAT(pictures, ElementsAre(MakePicture("num_minus", 24, 0), MakePicture("num_5", 48, 0)));

    pictures.clear();
    HudText::AddNumber(pictures, -5000, 0, 0, HudAnchor::Bottom, true);
    EXPECT_THAT(pictures, ElementsAre(
      MakePicture("anum_minus", 0, 0), MakePicture("anum_9", 24, 0), MakePicture("anum_9", 48, 0)));
  }

  TEST(HudTextTest, NamesTheDigitsAndNothingElse)
  {
    EXPECT_EQ(HudText::GetDigitName(0), "num_0");
    EXPECT_EQ(HudText::GetDigitName(9), "num_9");
    EXPECT_EQ(HudText::GetDigitName(7, true), "anum_7");
    EXPECT_EQ(HudText::GetDigitName(10), "");
    EXPECT_EQ(HudText::GetDigitName(-1), "");
  }
}
