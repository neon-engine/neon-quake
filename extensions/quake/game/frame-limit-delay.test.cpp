#include "frame-limit-delay.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::FrameLimitDelay;

  TEST(FrameLimitDelayTest, ALimitHoldsASecondAfterItWasAskedFor)
  {
    FrameLimitDelay delay;
    delay.Start(0);
    delay.Ask(60, 10.0);

    EXPECT_TRUE(delay.IsWaiting());
    EXPECT_EQ(delay.Take(10.0), std::nullopt);
    EXPECT_EQ(delay.Take(10.9), std::nullopt);
    EXPECT_EQ(delay.Take(11.0), 60);
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
    delay.Ask(70, 0.6);
    delay.Ask(80, 1.2);

    EXPECT_EQ(delay.Take(1.3), std::nullopt) << "more than a second after the first, and less after the last";
    EXPECT_EQ(delay.Take(2.1), std::nullopt);
    EXPECT_EQ(delay.Take(2.2), 80) << "only what was asked for last";
  }

  TEST(FrameLimitDelayTest, TheSameLimitAgainChangesNothing)
  {
    FrameLimitDelay delay;
    delay.Start(144);
    delay.Ask(144, 1.0);
    EXPECT_FALSE(delay.IsWaiting());

    // nor does it start the wait of another anew
    delay.Ask(60, 2.0);
    delay.Ask(60, 2.9);
    EXPECT_EQ(delay.Take(3.0), 60);
  }

  TEST(FrameLimitDelayTest, NoLimitIsWaitedForAsANumberIs)
  {
    FrameLimitDelay delay;
    delay.Start(60);
    delay.Ask(0, 0.0);

    EXPECT_EQ(delay.Take(0.9), std::nullopt);
    EXPECT_EQ(delay.Take(1.0), 0);
  }

  TEST(FrameLimitDelayTest, ALimitThatWaitsIsHandedOutAtOnceWhenTheSliderIsLeft)
  {
    FrameLimitDelay delay;
    delay.Start(0);
    EXPECT_EQ(delay.TakeNow(), std::nullopt) << "nothing waits";

    delay.Ask(60, 10.0);
    EXPECT_EQ(delay.TakeNow(), 60);
    EXPECT_FALSE(delay.IsWaiting());

    // once: neither way hands it out again
    EXPECT_EQ(delay.TakeNow(), std::nullopt);
    EXPECT_EQ(delay.Take(20.0), std::nullopt);
  }
}
