#include "held-button.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::HeldButton;

  // a frame at 120 frames a second
  constexpr float frame = 1.0f / 120.0f;

  TEST(HeldButtonTest, IsHeldWhileDownAndNotBefore)
  {
    HeldButton button;
    EXPECT_FALSE(button.Update(false, frame));
    EXPECT_TRUE(button.Update(true, frame));
    EXPECT_TRUE(button.Update(true, frame));
  }

  TEST(HeldButtonTest, StaysHeldThroughAFrameInWhichItSeemsUp)
  {
    // as at every repeat of a Tab that is held, when the engine takes the
    // keyboard away for a frame
    HeldButton button;
    for (int repeat = 0; repeat < 10; repeat++)
    {
      for (int i = 0; i < 4; i++) { EXPECT_TRUE(button.Update(true, frame)); }
      EXPECT_TRUE(button.Update(false, frame)) << repeat;
    }
  }

  TEST(HeldButtonTest, IsLetGoOnceItWasUpForTheGraceSeconds)
  {
    HeldButton button;
    EXPECT_TRUE(button.Update(true, frame));

    // 0.05, 0.08, and then 0.11 of a second up
    EXPECT_TRUE(button.Update(false, 0.05f));
    EXPECT_TRUE(button.Update(false, 0.03f));
    EXPECT_FALSE(button.Update(false, 0.03f));
    EXPECT_FALSE(button.Update(false, frame));

    // and is held again from the next press, with the whole grace
    EXPECT_TRUE(button.Update(true, frame));
    EXPECT_TRUE(button.Update(false, 0.08f));
    EXPECT_FALSE(button.Update(false, 0.03f));
  }
}
