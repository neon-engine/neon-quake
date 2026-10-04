#include "light-styles.hpp"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::LightStyles;
  using ::testing::Each;

  TEST(LightStylesTest, CountsALetterFromDarkOverNormalToDouble)
  {
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('a'), 0.0f);
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('g'), 0.5f);
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('m'), 1.0f);
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('y'), 2.0f);
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('z'), 25.0f / 12.0f);

    // what is no such letter is the nearest of them
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('A'), 0.0f);
    EXPECT_FLOAT_EQ(LightStyles::GetValueOfLetter('~'), 25.0f / 12.0f);
  }

  TEST(LightStylesTest, StartsWithTheStylesOfTheOriginalGame)
  {
    const LightStyles styles;

    EXPECT_EQ(styles.GetText(0), "m");
    EXPECT_EQ(styles.GetText(1), "mmnmmommommnonmmonqnmmo");
    EXPECT_EQ(styles.GetText(4), "mamamamamama");
    EXPECT_EQ(styles.GetText(9), "aaaaaaaazzzzzzzz");
    EXPECT_EQ(styles.GetText(11), "abcdefghijklmnopqrrqponmlkjihgfedcba");
    for (std::size_t style = 0; style < 12; style++) { EXPECT_FALSE(styles.GetText(style).empty()) << style; }
    for (std::size_t style = 12; style < LightStyles::count; style++) { EXPECT_EQ(styles.GetText(style), ""); }
  }

  TEST(LightStylesTest, ShowsTenLettersASecondAndStartsAgainWhenTheTextIsOver)
  {
    LightStyles styles;
    ASSERT_TRUE(styles.Set(40, "amy"));

    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.0), 0.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.05), 0.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.15), 1.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.25), 2.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.35), 0.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 0.45), 1.0f);

    // after a long time too
    EXPECT_FLOAT_EQ(styles.GetValue(40, 3000.05), 0.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 3000.15), 1.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 3000.25), 2.0f);

    // a time before the start is the start, and a time that is none too
    EXPECT_FLOAT_EQ(styles.GetValue(40, -5.0), 0.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(40, 1e300 * 1e300), 0.0f);
  }

  TEST(LightStylesTest, FollowsTheFlickerOfTheOriginal)
  {
    const LightStyles styles;
    const std::string flicker = "mmnmmommommnonmmonqnmmo";

    for (std::size_t step = 0; step < flicker.size() * 2; step++)
    {
      const double time = (static_cast<double>(step) + 0.5) / 10.0;
      EXPECT_FLOAT_EQ(styles.GetValue(1, time), LightStyles::GetValueOfLetter(flicker[step % flicker.size()]))
        << "step " << step;
    }

    // the light that stays as it is does
    EXPECT_FLOAT_EQ(styles.GetValue(0, 0.0), 1.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(0, 12.34), 1.0f);
  }

  TEST(LightStylesTest, SwitchesALightOffAndOn)
  {
    LightStyles styles;

    ASSERT_TRUE(styles.Set(32, "a"));
    EXPECT_FLOAT_EQ(styles.GetValue(32, 7.0), 0.0f);

    ASSERT_TRUE(styles.Set(32, "m"));
    EXPECT_FLOAT_EQ(styles.GetValue(32, 7.0), 1.0f);
    EXPECT_EQ(styles.GetText(32), "m");
  }

  TEST(LightStylesTest, TakesAStyleWithoutATextAsNormalLight)
  {
    LightStyles styles;
    EXPECT_FLOAT_EQ(styles.GetValue(50, 3.0), 1.0f);

    ASSERT_TRUE(styles.Set(3, ""));
    EXPECT_FLOAT_EQ(styles.GetValue(3, 0.0), 1.0f);
    EXPECT_FLOAT_EQ(styles.GetValue(3, 0.75), 1.0f);
  }

  TEST(LightStylesTest, RefusesAStyleThereIsNot)
  {
    LightStyles styles;
    EXPECT_FALSE(styles.Set(64, "a"));
    EXPECT_FALSE(styles.Set(1000, "a"));
    EXPECT_TRUE(styles.Set(63, "a"));

    EXPECT_EQ(styles.GetText(64), "");
    EXPECT_FLOAT_EQ(styles.GetValue(64, 0.0), 1.0f);
  }

  TEST(LightStylesTest, HandsOutWhatEveryStyleIsWorthAtATime)
  {
    LightStyles styles;
    styles.Set(33, "a");
    styles.Set(34, "ma");

    const LightStyles::Values values = styles.GetValues(0.15);
    for (std::size_t style = 0; style < LightStyles::count; style++)
    {
      EXPECT_FLOAT_EQ(values[style], styles.GetValue(style, 0.15)) << style;
    }
    EXPECT_FLOAT_EQ(values[0], 1.0f);
    EXPECT_FLOAT_EQ(values[4], 0.0f);
    EXPECT_FLOAT_EQ(values[33], 0.0f);
    EXPECT_FLOAT_EQ(values[34], 0.0f);
    EXPECT_FLOAT_EQ(values[60], 1.0f);
  }
}
