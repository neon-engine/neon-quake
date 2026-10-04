#include "center-text.hpp"

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"

namespace
{
  using quake::CenterText;
  using quake::HudAnchor;
  using quake::HudPicture;
  using quake::MakeLetter;
  using quake::MakeLetters;
  using ::testing::ElementsAre;
  using ::testing::ElementsAreArray;
  using ::testing::IsEmpty;

  constexpr HudAnchor center = HudAnchor::Center;

  /// Several lists as one.
  std::vector<HudPicture> join(const std::vector<std::vector<HudPicture>> &lists)
  {
    std::vector<HudPicture> all;
    for (const auto &list : lists) { all.insert(all.end(), list.begin(), list.end()); }
    return all;
  }

  TEST(CenterTextTest, PutsEachLineAroundTheMiddleByItself)
  {
    // 10 letters start at (320 - 80) / 2, and 3 at (320 - 24) / 2
    const auto pictures = CenterText::Layout("You need t\nthe\n\ngold key", 0.0f);

    EXPECT_THAT(pictures, ElementsAreArray(join({
      MakeLetters("You need t", 120, 70, center),
      MakeLetters("the", 148, 78, center),
      MakeLetters("gold key", 128, 94, center),
    })));
  }

  TEST(CenterTextTest, StartsHigherForATextOfMoreThanFourLines)
  {
    EXPECT_THAT(CenterText::Layout("a\nb\nc\nd", 0.0f), ElementsAre(
      MakeLetter('a', 156, 70, center), MakeLetter('b', 156, 78, center), MakeLetter('c', 156, 86, center),
      MakeLetter('d', 156, 94, center)));

    const auto five = CenterText::Layout("a\nb\nc\nd\ne", 0.0f);
    ASSERT_EQ(five.size(), 5u);
    EXPECT_EQ(five.front(), MakeLetter('a', 156, 48, center));
    EXPECT_EQ(five.back(), MakeLetter('e', 156, 80, center));
  }

  TEST(CenterTextTest, ShowsAnOrdinaryTextForTwoSecondsAndThenNothing)
  {
    EXPECT_EQ(CenterText::Layout("hello", 1.99f).size(), 5u);
    EXPECT_THAT(CenterText::Layout("hello", 2.0f), IsEmpty());
    EXPECT_THAT(CenterText::Layout("hello", 60.0f), IsEmpty());
    EXPECT_THAT(CenterText::Layout("", 0.0f), IsEmpty());
  }

  TEST(CenterTextTest, LeavesOutWhatALineHasBeyondFortyLetters)
  {
    const std::string wide(45, 'x');
    const auto pictures = CenterText::Layout(wide + "\nend", 0.0f);

    ASSERT_EQ(pictures.size(), 43u);
    EXPECT_EQ(pictures[0], MakeLetter('x', 0, 70, center));
    EXPECT_EQ(pictures[39], MakeLetter('x', 312, 70, center));
    EXPECT_EQ(pictures[40], MakeLetter('e', 148, 78, center));
  }

  TEST(CenterTextTest, RevealsTheStoryEightLettersASecondFromOneLetterOn)
  {
    const std::string_view story = "The end\nof it";

    EXPECT_THAT(CenterText::LayoutRevealed(story, 0.0f), ElementsAre(MakeLetter('T', 132, 70, center)));

    // after half a second four more; the line stays where all of it will be
    EXPECT_THAT(
      CenterText::LayoutRevealed(story, 0.5f), ElementsAreArray(MakeLetters("The e", 132, 70, center)));

    // the space counted, the line break did not: 7 of the first line and 2 of the second
    EXPECT_THAT(CenterText::LayoutRevealed(story, 1.0f), ElementsAreArray(join({
      MakeLetters("The end", 132, 70, center),
      MakeLetters("of", 140, 78, center),
    })));

    const auto all = join({MakeLetters("The end", 132, 70, center), MakeLetters("of it", 140, 78, center)});
    EXPECT_THAT(CenterText::LayoutRevealed(story, 2.0f), ElementsAreArray(all));
    EXPECT_THAT(CenterText::LayoutRevealed(story, 1.0e30f), ElementsAreArray(all));
  }

  TEST(CenterTextTest, RevealsAtAnotherSpeedAndOneLetterForATimeThatIsNone)
  {
    EXPECT_EQ(CenterText::LayoutRevealed("abcdefgh", 1.0f, 2.0f).size(), 3u);
    EXPECT_EQ(CenterText::LayoutRevealed("abcdefgh", -5.0f).size(), 1u);
    EXPECT_EQ(CenterText::LayoutRevealed("abcdefgh", std::numeric_limits<float>::quiet_NaN()).size(), 1u);
    EXPECT_THAT(CenterText::LayoutRevealed("", 5.0f), IsEmpty());
  }

  TEST(CenterTextTest, GivesTheFirstLettersOfAText)
  {
    EXPECT_THAT(CenterText::LayoutLetters("ab\ncd", 0), IsEmpty());
    EXPECT_THAT(CenterText::LayoutLetters("ab\ncd", 3), ElementsAre(
      MakeLetter('a', 152, 70, center), MakeLetter('b', 160, 70, center), MakeLetter('c', 152, 78, center)));
    EXPECT_EQ(CenterText::LayoutLetters("ab\ncd", 99).size(), 4u);
  }
}
