#include "frame-limit-delay.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::FrameLimitDelay;

  TEST(FrameLimitDelayTest, ALimitHoldsFiveSecondsAfterItWasAskedFor)
  {
    FrameLimitDelay delay;
    delay.Start(0);
    delay.Ask(60, 10.0);

    EXPECT_TRUE(delay.IsWaiting());
    EXPECT_EQ(delay.Take(10.0), std::nullopt);
    EXPECT_EQ(delay.Take(14.9), std::nullopt);
    EXPECT_EQ(delay.Take(15.0), 60);
  }

  TEST(FrameLimitDelayTest, ItIsHandedOutOnce)
  {
    FrameLimitDelay delay;
    delay.Ask(60, 0.0);

    EXPECT_EQ(delay.Take(6.0), 60);
    EXPECT_FALSE(delay.IsWaiting());
    EXPECT_EQ(delay.Take(7.0), std::nullopt);
  }

  TEST(FrameLimitDelayTest, EveryStepOfTheSliderStartsTheWaitAnew)
  {
    FrameLimitDelay delay;
    delay.Ask(60, 0.0);
    delay.Ask(70, 3.0);
    delay.Ask(80, 4.0);

    EXPECT_EQ(delay.Take(5.0), std::nullopt) << "five seconds after the first, and one after the last";
    EXPECT_EQ(delay.Take(8.9), std::nullopt);
    EXPECT_EQ(delay.Take(9.0), 80) << "only what was asked for last";
  }

  TEST(FrameLimitDelayTest, TheSameLimitAgainChangesNothing)
  {
    FrameLimitDelay delay;
    delay.Start(144);
    delay.Ask(144, 1.0);
    EXPECT_FALSE(delay.IsWaiting());

    // nor does it start the wait of another anew
    delay.Ask(60, 2.0);
    delay.Ask(60, 6.0);
    EXPECT_EQ(delay.Take(7.0), 60);
  }

  TEST(FrameLimitDelayTest, NoLimitIsWaitedForAsANumberIs)
  {
    FrameLimitDelay delay;
    delay.Start(60);
    delay.Ask(0, 0.0);

    EXPECT_EQ(delay.Take(4.0), std::nullopt);
    EXPECT_EQ(delay.Take(5.0), 0);
  }
}
