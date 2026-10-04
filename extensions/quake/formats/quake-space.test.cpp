#include "quake-space.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::BspVector;
  using quake::QuakeSpace;

  TEST(QuakeSpaceTest, MakesThePlayerOfTheGameAManOfOrdinaryHeight)
  {
    EXPECT_FLOAT_EQ(56.0f * QuakeSpace::metres_per_unit, 1.75f);
  }

  TEST(QuakeSpaceTest, TurnsUpIntoYAndNorthIntoTheNegativeZInMetres)
  {
    const BspVector east = QuakeSpace::ToEnginePosition({32.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(east.x, 1.0f);
    EXPECT_FLOAT_EQ(east.y, 0.0f);
    EXPECT_FLOAT_EQ(east.z, 0.0f);

    const BspVector north = QuakeSpace::ToEnginePosition({0.0f, 64.0f, 0.0f});
    EXPECT_FLOAT_EQ(north.x, 0.0f);
    EXPECT_FLOAT_EQ(north.y, 0.0f);
    EXPECT_FLOAT_EQ(north.z, -2.0f);

    const BspVector up = QuakeSpace::ToEnginePosition({0.0f, 0.0f, 16.0f});
    EXPECT_FLOAT_EQ(up.y, 0.5f);
    EXPECT_FLOAT_EQ(up.z, 0.0f);
  }

  TEST(QuakeSpaceTest, KeepsADirectionAsLongAsItWas)
  {
    const BspVector up = QuakeSpace::ToEngineDirection({0.0f, 0.0f, 1.0f});
    EXPECT_FLOAT_EQ(up.x, 0.0f);
    EXPECT_FLOAT_EQ(up.y, 1.0f);
    EXPECT_FLOAT_EQ(up.z, 0.0f);

    const BspVector north = QuakeSpace::ToEngineDirection({0.0f, 1.0f, 0.0f});
    EXPECT_FLOAT_EQ(north.z, -1.0f);
  }

  TEST(QuakeSpaceTest, TurnsTheAxesWithoutMirroringThem)
  {
    // east cross north is up in the game, and still is in the engine
    const BspVector x = QuakeSpace::ToEngineDirection({1.0f, 0.0f, 0.0f});
    const BspVector y = QuakeSpace::ToEngineDirection({0.0f, 1.0f, 0.0f});
    const BspVector z = QuakeSpace::ToEngineDirection({0.0f, 0.0f, 1.0f});

    const BspVector cross = {x.y * y.z - x.z * y.y, x.z * y.x - x.x * y.z, x.x * y.y - x.y * y.x};
    EXPECT_FLOAT_EQ(cross.x, z.x);
    EXPECT_FLOAT_EQ(cross.y, z.y);
    EXPECT_FLOAT_EQ(cross.z, z.z);
  }

  TEST(QuakeSpaceTest, LooksNorthAtTheYawTheEngineLooksDownItsNegativeZ)
  {
    EXPECT_FLOAT_EQ(QuakeSpace::ToEngineYaw(90.0f), 0.0f);
    EXPECT_FLOAT_EQ(QuakeSpace::ToEngineYaw(0.0f), -90.0f);
    EXPECT_FLOAT_EQ(QuakeSpace::ToEngineYaw(180.0f), 90.0f);
  }

  TEST(QuakeSpaceTest, TurnsAPlaceOfTheEngineBackIntoTheOneOfTheGame)
  {
    const BspVector there = QuakeSpace::ToEnginePosition({96.0f, -48.0f, 24.0f});
    const BspVector back = QuakeSpace::ToGamePosition(there);
    EXPECT_FLOAT_EQ(back.x, 96.0f);
    EXPECT_FLOAT_EQ(back.y, -48.0f);
    EXPECT_FLOAT_EQ(back.z, 24.0f);
  }
}
