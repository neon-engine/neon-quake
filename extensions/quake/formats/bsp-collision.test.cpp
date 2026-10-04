#include "bsp-collision.hpp"

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "bsp-hull-builder.test.hpp"

namespace
{
  using quake::BspCollision;
  using quake::BspContents;
  using quake::BspHull;
  using quake::BspHullBuilder;
  using quake::BspTraceResult;
  using quake::BspVector;

  constexpr std::int16_t empty = BspHullBuilder::empty;
  constexpr std::int16_t solid = BspHullBuilder::solid;
  constexpr float epsilon = BspHull::distance_epsilon;

  constexpr BspVector nowhere{0.0f, 0.0f, 0.0f};
  constexpr BspVector point{0.0f, 0.0f, 0.0f};
  constexpr BspVector player_mins{-16.0f, -16.0f, -24.0f};
  constexpr BspVector player_maxs{16.0f, 16.0f, 32.0f};
  constexpr BspVector large_mins{-32.0f, -32.0f, -24.0f};
  constexpr BspVector large_maxs{32.0f, 32.0f, 64.0f};

  /// A level with a floor at a height of 0, as a compiler would write it:
  /// the floor of each hull lies higher by what the box of the hull reaches
  /// down, 24 units for both, so that the origin of what stands on the
  /// floor is stopped there. To tell the two apart in a test, the large
  /// hull has a wall too, at an X of 100.
  BspCollision MakeFloor()
  {
    BspHullBuilder builder;
    const std::int16_t point_floor =
      builder.Node({0.0f, 0.0f, 1.0f}, 0.0f, BspHullBuilder::empty_leaf, BspHullBuilder::solid_leaf);
    const std::int16_t player_floor = builder.ClipNode({0.0f, 0.0f, 1.0f}, 24.0f, empty, solid);
    const std::int16_t large_floor = builder.ClipNode({0.0f, 0.0f, 1.0f}, 24.0f, empty, solid);
    const std::int16_t large_wall = builder.ClipNode({1.0f, 0.0f, 0.0f}, 100.0f, solid, large_floor);
    builder.Model(point_floor, player_floor, large_wall);

    BspCollision collision;
    std::string error;
    EXPECT_TRUE(collision.Build(builder.file, 0, error)) << error;
    return collision;
  }

  void ExpectPlace(const BspVector &place, const float x, const float y, const float z)
  {
    EXPECT_NEAR(place.x, x, 1.0e-4f);
    EXPECT_NEAR(place.y, y, 1.0e-4f);
    EXPECT_NEAR(place.z, z, 1.0e-4f);
  }

  TEST(BspCollisionTest, ChoosesTheHullByTheWidthOfTheBoxAlongX)
  {
    EXPECT_EQ(BspCollision::ChooseHull(point, point), 0u);
    EXPECT_EQ(BspCollision::ChooseHull({-1.0f, -50.0f, -50.0f}, {1.9f, 50.0f, 50.0f}), 0u);
    EXPECT_EQ(BspCollision::ChooseHull({-1.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f}), 1u);
    EXPECT_EQ(BspCollision::ChooseHull({-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f}), 1u);
    EXPECT_EQ(BspCollision::ChooseHull(player_mins, player_maxs), 1u);
    EXPECT_EQ(BspCollision::ChooseHull({-16.0f, -16.0f, -24.0f}, {16.5f, 16.0f, 32.0f}), 2u);
    EXPECT_EQ(BspCollision::ChooseHull(large_mins, large_maxs), 2u);
  }

  TEST(BspCollisionTest, MakesTheThreeHullsOfAModel)
  {
    const BspCollision collision = MakeFloor();
    EXPECT_EQ(collision.GetHull(0).GetNodes().size(), 1u);
    EXPECT_EQ(collision.GetHull(1).GetNodes().size(), 1u);
    EXPECT_EQ(collision.GetHull(2).GetNodes().size(), 2u);
    EXPECT_EQ(collision.GetHull(1).GetMins().z, -24.0f);
    EXPECT_EQ(collision.GetHull(2).GetMaxs().z, 64.0f);
  }

  TEST(BspCollisionTest, SaysWhatFillsAPlaceByHullZero)
  {
    const BspCollision collision = MakeFloor();
    EXPECT_EQ(collision.GetPointContents({0.0f, 0.0f, 8.0f}), BspContents::Empty);
    EXPECT_EQ(collision.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Solid);
  }

  TEST(BspCollisionTest, MovesAPointThroughHullZero)
  {
    const BspCollision collision = MakeFloor();
    const BspTraceResult result = collision.TraceBox(nowhere, {0.0f, 0.0f, 64.0f}, point, point, {0.0f, 0.0f, -64.0f});

    EXPECT_NEAR(result.fraction, (64.0f - epsilon) / 128.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 0.0f, 0.0f, epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(result.plane_distance, 0.0f, 1.0e-4f);
  }

  TEST(BspCollisionTest, StopsThePlayerWithItsFeetOnTheFloor)
  {
    const BspCollision collision = MakeFloor();
    const BspTraceResult result =
      collision.TraceBox(nowhere, {0.0f, 0.0f, 88.0f}, player_mins, player_maxs, {0.0f, 0.0f, -40.0f});

    // the origin of the player is 24 units above its feet
    EXPECT_FALSE(result.start_solid);
    EXPECT_NEAR(result.fraction, (64.0f - epsilon) / 128.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 0.0f, 0.0f, 24.0f + epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(result.plane_distance, 24.0f, 1.0e-4f);
  }

  TEST(BspCollisionTest, CountsASmallerBoxFromItsLowestCorner)
  {
    const BspCollision collision = MakeFloor();
    const BspTraceResult result = collision.TraceBox(
      nowhere, {0.0f, 0.0f, 64.0f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f}, {0.0f, 0.0f, -64.0f});

    // its underside is 8 units below its origin and comes to rest on the
    // floor, as the feet of the player do
    ExpectPlace(result.end_position, 0.0f, 0.0f, 8.0f + epsilon);
    EXPECT_NEAR(result.plane_distance, 8.0f, 1.0e-4f);
  }

  TEST(BspCollisionTest, MovesALargeBoxThroughHullTwo)
  {
    const BspCollision collision = MakeFloor();

    // the wall is in the large hull alone
    BspTraceResult result =
      collision.TraceBox(nowhere, {0.0f, 0.0f, 64.0f}, large_mins, large_maxs, {200.0f, 0.0f, 64.0f});
    ExpectPlace(result.end_position, 100.0f - epsilon, 0.0f, 64.0f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);

    result = collision.TraceBox(nowhere, {0.0f, 0.0f, 64.0f}, player_mins, player_maxs, {200.0f, 0.0f, 64.0f});
    EXPECT_EQ(result.fraction, 1.0f);
  }

  TEST(BspCollisionTest, EndsAMoveThatMeetsNothingExactlyWhereItWasMeantTo)
  {
    const BspCollision collision = MakeFloor();
    const BspVector origin{0.1f, 0.2f, 0.3f};
    const BspVector end{123.456f, -65.4321f, 77.7f};
    const BspTraceResult result =
      collision.TraceBox(origin, {1.1f, 2.2f, 90.9f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f}, end);

    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_EQ(result.end_position.x, end.x);
    EXPECT_EQ(result.end_position.y, end.y);
    EXPECT_EQ(result.end_position.z, end.z);
  }

  TEST(BspCollisionTest, MovesAgainstAModelWhereItStands)
  {
    // a door of 16 by 64 units and 64 high, with a corner at its origin
    BspHullBuilder builder;
    builder.Model(
      BspHullBuilder::empty_leaf, builder.Box({0.0f, 0.0f, 0.0f}, {16.0f, 64.0f, 64.0f}, solid, empty), empty);
    BspCollision door;
    std::string error;
    ASSERT_TRUE(door.Build(builder.file, 0, error)) << error;

    const BspVector start{-100.0f, 532.0f, 32.0f};
    const BspVector end{1100.0f, 532.0f, 32.0f};

    // where the level was compiled, the door is not in the way
    EXPECT_EQ(door.TraceBox(nowhere, start, player_mins, player_maxs, end).fraction, 1.0f);

    // where its entity stands, it is
    const BspTraceResult result = door.TraceBox({1000.0f, 500.0f, 0.0f}, start, player_mins, player_maxs, end);
    EXPECT_NEAR(result.fraction, (1100.0f - epsilon) / 1200.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 1000.0f - epsilon, 532.0f, 32.0f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);
    EXPECT_NEAR(result.plane_distance, -1000.0f, 1.0e-3f);
  }

  TEST(BspCollisionTest, RefusesAModelWithAHullThatIsRefusedAndStaysAsItWas)
  {
    BspCollision collision = MakeFloor();

    BspHullBuilder builder;
    builder.Model(BspHullBuilder::empty_leaf, empty, 9);
    std::string error;
    EXPECT_FALSE(collision.Build(builder.file, 0, error));
    EXPECT_EQ(error, "hull 2 of model 0 names clip node 9, and there are 0");
    EXPECT_FALSE(collision.Build(builder.file, 1, error));
    EXPECT_EQ(error, "model 1 was asked for, and there are 1");

    EXPECT_EQ(collision.GetHull(2).GetNodes().size(), 2u);
  }
}
