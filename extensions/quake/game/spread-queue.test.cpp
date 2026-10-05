#include "spread-queue.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::SpreadQueue;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  SpreadQueue make_queue(const std::size_t count)
  {
    SpreadQueue queue;
    for (std::size_t i = 0; i < count; i++) { queue.Add({0, i}); }
    return queue;
  }

  TEST(SpreadQueueTest, HandsOnAShareOfWhatCameDueInEachFrame)
  {
    // 40 came due; a frame of a 120th of a second is about a tenth of the
    // 80 ms they are spread over
    SpreadQueue queue = make_queue(40);

    std::size_t frames = 0;
    std::size_t handed_on = 0;
    while (queue.GetWaiting() > 0)
    {
      const std::size_t taken = queue.Take(1.0 / 120.0).size();
      EXPECT_LE(taken, 5u) << "no frame carries more than its share";
      handed_on += taken;
      frames++;
    }
    EXPECT_EQ(handed_on, 40u);
    EXPECT_LE(frames, 10u) << "all within 80 ms";
    EXPECT_GE(frames, 8u);
  }

  TEST(SpreadQueueTest, HandsOnTheOldestFirst)
  {
    SpreadQueue queue;
    queue.Add({3, 0});
    queue.Add({1, 2});
    queue.Add({1, 0});

    EXPECT_THAT(queue.Take(0.001), ElementsAre(SpreadQueue::Item{3, 0}, SpreadQueue::Item{1, 2}));
    EXPECT_THAT(queue.Take(0.001), ElementsAre(SpreadQueue::Item{1, 0}));
    EXPECT_THAT(queue.Take(0.001), IsEmpty());
  }

  TEST(SpreadQueueTest, HandsOnEverythingInAFrameAsLongAsTheWhile)
  {
    SpreadQueue queue = make_queue(40);
    EXPECT_EQ(queue.Take(0.1).size(), 40u);
    EXPECT_EQ(queue.GetWaiting(), 0u);
  }

  TEST(SpreadQueueTest, EndsAlsoWhenNoTimePasses)
  {
    SpreadQueue queue = make_queue(5);
    EXPECT_EQ(queue.Take(0.0).size(), 2u);
    EXPECT_EQ(queue.Take(-1.0).size(), 2u);
    EXPECT_EQ(queue.Take(0.0).size(), 1u);
  }

  TEST(SpreadQueueTest, WhatWaitsAlreadyWaitsOnce)
  {
    SpreadQueue queue;
    queue.Add({0, 1});
    queue.Add({0, 1});
    queue.Add({2, 1});
    EXPECT_EQ(queue.GetWaiting(), 2u);

    // and what was handed on can come due again
    (void) queue.Take(1.0);
    queue.Add({0, 1});
    EXPECT_EQ(queue.GetWaiting(), 1u);
  }

  TEST(SpreadQueueTest, ASecondTickBeforeTheFirstIsDoneAddsOnlyWhatIsNew)
  {
    SpreadQueue queue = make_queue(40);
    (void) queue.Take(1.0 / 120.0);
    const std::size_t left = queue.GetWaiting();

    for (std::size_t i = 0; i < 40; i++) { queue.Add({0, i}); }
    EXPECT_EQ(queue.GetWaiting(), 40u) << left << " waited on, and the ones handed on wait again";
  }

  TEST(SpreadQueueTest, IsEmptiedWhenTheLevelEnds)
  {
    SpreadQueue queue = make_queue(7);
    queue.Clear();
    EXPECT_EQ(queue.GetWaiting(), 0u);
    EXPECT_THAT(queue.Take(1.0), IsEmpty());
  }
}
