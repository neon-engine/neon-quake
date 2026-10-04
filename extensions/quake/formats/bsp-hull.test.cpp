#include "bsp-hull.hpp"

#include <cstdint>
#include <limits>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bsp-hull-builder.test.hpp"

namespace
{
  using quake::BspContents;
  using quake::BspHull;
  using quake::BspHullBuilder;
  using quake::BspTraceResult;
  using quake::BspVector;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  constexpr std::int16_t empty = BspHullBuilder::empty;
  constexpr std::int16_t solid = BspHullBuilder::solid;
  constexpr std::int16_t water = BspHullBuilder::water;
  constexpr float epsilon = BspHull::distance_epsilon;

  /// Makes hull 1 of the last model of the level, which must be accepted.
  BspHull MakeHull(const BspHullBuilder &builder)
  {
    BspHull hull;
    std::string error;
    EXPECT_TRUE(hull.Build(builder.file, builder.file.models.size() - 1, 1, error)) << error;
    return hull;
  }

  /// The sentence hull 1 of the last model of the level is refused with.
  std::string ReasonOfRefusal(const BspHullBuilder &builder)
  {
    BspHull hull;
    std::string error;
    EXPECT_FALSE(hull.Build(builder.file, builder.file.models.size() - 1, 1, error));
    return error;
  }

  /// A floor without an end: solid below a height of 0.
  BspHull MakeFloor()
  {
    BspHullBuilder builder;
    builder.Model(BspHullBuilder::empty_leaf, builder.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, solid), solid);
    return MakeHull(builder);
  }

  /// A room of 128 by 128 units and 128 high around the origin on the
  /// ground, with a pillar of 16 by 16 units from floor to ceiling in its
  /// middle, and water 32 deep in the corner of the least X and Y, 32 by 32
  /// units wide.
  BspHull MakeRoom()
  {
    BspHullBuilder builder;
    const std::int16_t pool = builder.Box({-64.0f, -64.0f, 0.0f}, {-32.0f, -32.0f, 32.0f}, water, empty);
    const std::int16_t pillar = builder.Box({-8.0f, -8.0f, 0.0f}, {8.0f, 8.0f, 128.0f}, solid, empty);

    // a plane between the two, so that the tree has one way to each
    const std::int16_t within = builder.ClipNode({0.0f, 1.0f, 0.0f}, -16.0f, pillar, pool);
    const std::int16_t room = builder.Box({-64.0f, -64.0f, 0.0f}, {64.0f, 64.0f, 128.0f}, within, solid);
    builder.Model(BspHullBuilder::empty_leaf, room, solid);
    return MakeHull(builder);
  }

  void ExpectPlace(const BspVector &place, const float x, const float y, const float z)
  {
    EXPECT_NEAR(place.x, x, 1.0e-4f);
    EXPECT_NEAR(place.y, y, 1.0e-4f);
    EXPECT_NEAR(place.z, z, 1.0e-4f);
  }

  TEST(BspHullTest, SaysWhatFillsAPlace)
  {
    const BspHull hull = MakeRoom();
    EXPECT_EQ(hull.GetPointContents({32.0f, 32.0f, 64.0f}), BspContents::Empty);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 64.0f}), BspContents::Solid);
    EXPECT_EQ(hull.GetPointContents({100.0f, 0.0f, 64.0f}), BspContents::Solid);
    EXPECT_EQ(hull.GetPointContents({0.0f, 32.0f, -1.0f}), BspContents::Solid);
    EXPECT_EQ(hull.GetPointContents({-48.0f, -48.0f, 16.0f}), BspContents::Water);
    EXPECT_EQ(hull.GetPointContents({-48.0f, -48.0f, 48.0f}), BspContents::Empty);
  }

  TEST(BspHullTest, CountsAPlaceOnAPlaneAsInFrontOfIt)
  {
    const BspHull hull = MakeFloor();
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 0.0f}), BspContents::Empty);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -0.001f}), BspContents::Solid);
  }

  TEST(BspHullTest, KnowsTheBoxEachHullWasGrownBy)
  {
    BspHullBuilder builder;
    builder.Model(BspHullBuilder::empty_leaf, empty, empty);

    BspHull hull;
    std::string error;
    ASSERT_TRUE(hull.Build(builder.file, 0, 0, error)) << error;
    ExpectPlace(hull.GetMins(), 0.0f, 0.0f, 0.0f);
    ExpectPlace(hull.GetMaxs(), 0.0f, 0.0f, 0.0f);

    ASSERT_TRUE(hull.Build(builder.file, 0, 1, error)) << error;
    ExpectPlace(hull.GetMins(), -16.0f, -16.0f, -24.0f);
    ExpectPlace(hull.GetMaxs(), 16.0f, 16.0f, 32.0f);

    ASSERT_TRUE(hull.Build(builder.file, 0, 2, error)) << error;
    ExpectPlace(hull.GetMins(), -32.0f, -32.0f, -24.0f);
    ExpectPlace(hull.GetMaxs(), 32.0f, 32.0f, 64.0f);
  }

  TEST(BspHullTest, MakesWaterOfWaterThatFlows)
  {
    for (std::int16_t current = -9; current >= -14; current--)
    {
      BspHullBuilder builder;
      builder.Model(BspHullBuilder::empty_leaf, builder.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, current), solid);
      const BspHull hull = MakeHull(builder);
      EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Water) << current;
    }
  }

  TEST(BspHullTest, MakesHullZeroOfTheNodesAndWhatFillsTheirLeaves)
  {
    BspHullBuilder builder;
    const std::int16_t lava = builder.Leaf(static_cast<std::int32_t>(BspContents::Lava));
    const std::int16_t below = builder.Node({0.0f, 0.0f, 1.0f}, -32.0f, lava, BspHullBuilder::solid_leaf);
    const std::int16_t head = builder.Node({0.0f, 0.0f, 1.0f}, 0.0f, BspHullBuilder::empty_leaf, below);
    builder.Model(head, solid, solid);

    BspHull hull;
    std::string error;
    ASSERT_TRUE(hull.Build(builder.file, 0, 0, error)) << error;
    EXPECT_EQ(hull.GetNodes().size(), 2u);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 8.0f}), BspContents::Empty);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Lava);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -40.0f}), BspContents::Solid);

    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 32.0f}, {0.0f, 0.0f, -96.0f});
    EXPECT_TRUE(result.in_open);
    EXPECT_TRUE(result.in_water);
    EXPECT_FALSE(result.start_solid);
    EXPECT_NEAR(result.fraction, (64.0f - epsilon) / 128.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 0.0f, 0.0f, -32.0f + epsilon);
  }

  TEST(BspHullTest, MakesAHullOfOneThingOfATreeWithoutAFork)
  {
    BspHullBuilder builder;
    builder.Model(BspHullBuilder::solid_leaf, water, empty);

    BspHull hull;
    std::string error;
    ASSERT_TRUE(hull.Build(builder.file, 0, 0, error)) << error;
    EXPECT_THAT(hull.GetNodes(), IsEmpty());
    EXPECT_EQ(hull.GetPointContents({1.0f, 2.0f, 3.0f}), BspContents::Solid);
    EXPECT_TRUE(hull.TraceLine({0.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f}).all_solid);

    ASSERT_TRUE(hull.Build(builder.file, 0, 1, error)) << error;
    EXPECT_EQ(hull.GetPointContents({1.0f, 2.0f, 3.0f}), BspContents::Water);
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f});
    EXPECT_TRUE(result.in_water);
    EXPECT_FALSE(result.in_open);
    EXPECT_FALSE(result.all_solid);
    EXPECT_EQ(result.fraction, 1.0f);
  }

  TEST(BspHullTest, ReadsTheNumberOfAForkAboveWhatASignedNumberHolds)
  {
    // clip node 40000 is -25536 where its parent names it
    BspHullBuilder builder;
    builder.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, static_cast<std::int16_t>(40000));
    builder.file.clip_nodes.resize(40000);
    builder.ClipNode({0.0f, 0.0f, 1.0f}, -32.0f, water, solid);
    builder.Model(BspHullBuilder::empty_leaf, 0, solid);

    const BspHull hull = MakeHull(builder);
    EXPECT_EQ(hull.GetNodes().size(), 2u);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 8.0f}), BspContents::Empty);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Water);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -40.0f}), BspContents::Solid);

    // the same in the tree a level is drawn by, where what is not a node
    // is a leaf
    BspHullBuilder drawn;
    drawn.Node({0.0f, 0.0f, 1.0f}, 0.0f, BspHullBuilder::empty_leaf, static_cast<std::int16_t>(40000));
    drawn.file.nodes.resize(40000);
    drawn.Node({0.0f, 0.0f, 1.0f}, -32.0f, drawn.Leaf(BspHullBuilder::water), BspHullBuilder::solid_leaf);
    drawn.Model(0, solid, solid);

    BspHull point;
    std::string error;
    ASSERT_TRUE(point.Build(drawn.file, 0, 0, error)) << error;
    EXPECT_EQ(point.GetNodes().size(), 2u);
    EXPECT_EQ(point.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Water);
    EXPECT_EQ(point.GetPointContents({0.0f, 0.0f, -40.0f}), BspContents::Solid);
  }

  TEST(BspHullTest, TakesOnlyTheForksOfItsOwnTree)
  {
    BspHullBuilder builder;
    builder.Box({0.0f, 0.0f, 0.0f}, {8.0f, 8.0f, 8.0f}, solid, empty);
    builder.Model(BspHullBuilder::empty_leaf, builder.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, solid), solid);
    EXPECT_EQ(MakeHull(builder).GetNodes().size(), 1u);
  }

  TEST(BspHullTest, StopsAMoveALittleBeforeTheFloor)
  {
    const BspHull hull = MakeFloor();
    const BspTraceResult result = hull.TraceLine({8.0f, 16.0f, 64.0f}, {8.0f, 16.0f, -64.0f});

    EXPECT_FALSE(result.all_solid);
    EXPECT_FALSE(result.start_solid);
    EXPECT_TRUE(result.in_open);
    EXPECT_FALSE(result.in_water);
    EXPECT_NEAR(result.fraction, (64.0f - epsilon) / 128.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 8.0f, 16.0f, epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(result.plane_distance, 0.0f);
  }

  TEST(BspHullTest, LetsAMoveThatMeetsNothingEndWhereItWasMeantTo)
  {
    const BspHull hull = MakeFloor();
    const BspVector end{100.0f, -50.0f, 1.0f};
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 64.0f}, end);

    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.all_solid);
    EXPECT_FALSE(result.start_solid);
    EXPECT_TRUE(result.in_open);
    EXPECT_EQ(result.end_position.x, end.x);
    EXPECT_EQ(result.end_position.y, end.y);
    EXPECT_EQ(result.end_position.z, end.z);
  }

  TEST(BspHullTest, LetsAMoveGrazeTheFloor)
  {
    const BspHull hull = MakeFloor();

    // on the plane itself, which counts as in front of it
    BspTraceResult result = hull.TraceLine({-100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f});
    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.start_solid);
    EXPECT_TRUE(result.in_open);

    // down to the plane and no further
    result = hull.TraceLine({-100.0f, 0.0f, 16.0f}, {100.0f, 0.0f, 0.0f});
    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.start_solid);
  }

  TEST(BspHullTest, StopsAMoveAtTheSameDistanceBeforeASlantedMoveMeetsTheFloor)
  {
    const BspHull hull = MakeFloor();
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 32.0f}, {64.0f, 0.0f, -32.0f});

    EXPECT_NEAR(result.fraction, (32.0f - epsilon) / 64.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 32.0f - epsilon, 0.0f, epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
  }

  TEST(BspHullTest, SaysThatAMoveStartedInWhatIsSolidAndLetsItOut)
  {
    const BspHull hull = MakeFloor();
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, -64.0f}, {0.0f, 0.0f, 64.0f});

    EXPECT_TRUE(result.start_solid);
    EXPECT_FALSE(result.all_solid);
    EXPECT_TRUE(result.in_open);
    EXPECT_EQ(result.fraction, 1.0f);
    ExpectPlace(result.end_position, 0.0f, 0.0f, 64.0f);
  }

  TEST(BspHullTest, SaysThatAMoveNeverLeftWhatIsSolid)
  {
    const BspHull hull = MakeFloor();
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, -64.0f}, {32.0f, 0.0f, -8.0f});

    EXPECT_TRUE(result.all_solid);
    EXPECT_TRUE(result.start_solid);
    EXPECT_FALSE(result.in_open);
    EXPECT_FALSE(result.in_water);
    EXPECT_EQ(result.fraction, 1.0f);
    ExpectPlace(result.end_position, 32.0f, 0.0f, -8.0f);
  }

  TEST(BspHullTest, StopsAMoveAtEveryWallOfARoomWithTheNormalLookingIn)
  {
    const BspHull hull = MakeRoom();
    const BspVector start{32.0f, 32.0f, 64.0f};

    BspTraceResult result = hull.TraceLine(start, {160.0f, 32.0f, 64.0f});
    EXPECT_NEAR(result.fraction, (32.0f - epsilon) / 128.0f, 1.0e-6f);
    ExpectPlace(result.end_position, 64.0f - epsilon, 32.0f, 64.0f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);
    EXPECT_EQ(result.plane_distance, -64.0f);

    result = hull.TraceLine(start, {-160.0f, 32.0f, 64.0f});
    ExpectPlace(result.end_position, -64.0f + epsilon, 32.0f, 64.0f);
    ExpectPlace(result.plane_normal, 1.0f, 0.0f, 0.0f);
    EXPECT_EQ(result.plane_distance, -64.0f);

    result = hull.TraceLine(start, {32.0f, 160.0f, 64.0f});
    ExpectPlace(result.end_position, 32.0f, 64.0f - epsilon, 64.0f);
    ExpectPlace(result.plane_normal, 0.0f, -1.0f, 0.0f);

    result = hull.TraceLine(start, {32.0f, 32.0f, 200.0f});
    ExpectPlace(result.end_position, 32.0f, 32.0f, 128.0f - epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, -1.0f);
    EXPECT_EQ(result.plane_distance, -128.0f);

    result = hull.TraceLine(start, {32.0f, 32.0f, -200.0f});
    ExpectPlace(result.end_position, 32.0f, 32.0f, epsilon);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
  }

  TEST(BspHullTest, StopsAMoveAtAPillarAndLetsOneByThatMissesIt)
  {
    const BspHull hull = MakeRoom();

    BspTraceResult result = hull.TraceLine({-40.0f, 0.0f, 16.0f}, {40.0f, 0.0f, 16.0f});
    EXPECT_FALSE(result.start_solid);
    EXPECT_NEAR(result.fraction, (32.0f - epsilon) / 80.0f, 1.0e-6f);
    ExpectPlace(result.end_position, -8.0f - epsilon, 0.0f, 16.0f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);
    EXPECT_EQ(result.plane_distance, 8.0f);

    result = hull.TraceLine({-40.0f, 12.0f, 16.0f}, {40.0f, 12.0f, 16.0f});
    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.start_solid);

    // the end lies inside the pillar
    result = hull.TraceLine({0.0f, 40.0f, 16.0f}, {0.0f, 0.0f, 16.0f});
    ExpectPlace(result.end_position, 0.0f, 8.0f + epsilon, 16.0f);
    ExpectPlace(result.plane_normal, 0.0f, 1.0f, 0.0f);
  }

  TEST(BspHullTest, StopsAMoveOutOfThePillarAtTheWallBehindIt)
  {
    const BspHull hull = MakeRoom();
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 16.0f}, {160.0f, 0.0f, 16.0f});

    EXPECT_TRUE(result.start_solid);
    EXPECT_FALSE(result.all_solid);
    ExpectPlace(result.end_position, 64.0f - epsilon, 0.0f, 16.0f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);
  }

  TEST(BspHullTest, SaysWhetherAMoveWentThroughWhatIsEmptyAndThroughWater)
  {
    const BspHull hull = MakeRoom();

    // from the air into the water
    BspTraceResult result = hull.TraceLine({-48.0f, -48.0f, 64.0f}, {-48.0f, -48.0f, 8.0f});
    EXPECT_TRUE(result.in_open);
    EXPECT_TRUE(result.in_water);
    EXPECT_EQ(result.fraction, 1.0f);

    // in the water alone
    result = hull.TraceLine({-56.0f, -48.0f, 8.0f}, {-40.0f, -48.0f, 24.0f});
    EXPECT_FALSE(result.in_open);
    EXPECT_TRUE(result.in_water);
    EXPECT_FALSE(result.all_solid);
    EXPECT_EQ(result.fraction, 1.0f);

    // in the air alone
    result = hull.TraceLine({32.0f, 32.0f, 64.0f}, {48.0f, 32.0f, 64.0f});
    EXPECT_TRUE(result.in_open);
    EXPECT_FALSE(result.in_water);

    // through the water to the floor under it
    result = hull.TraceLine({-48.0f, -48.0f, 64.0f}, {-48.0f, -48.0f, -64.0f});
    EXPECT_TRUE(result.in_open);
    EXPECT_TRUE(result.in_water);
    ExpectPlace(result.end_position, -48.0f, -48.0f, epsilon);
  }

  TEST(BspHullTest, RefusesAModelOrAHullThatIsNotThereAndStaysAsItWas)
  {
    BspHull hull = MakeFloor();
    BspHullBuilder builder;
    builder.Model(BspHullBuilder::empty_leaf, empty, empty);

    std::string error;
    EXPECT_FALSE(hull.Build(builder.file, 1, 1, error));
    EXPECT_EQ(error, "model 1 was asked for, and there are 1");
    EXPECT_FALSE(hull.Build(builder.file, 0, 3, error));
    EXPECT_EQ(error, "hull 3 was asked for, and there are 3");

    EXPECT_EQ(hull.GetNodes().size(), 1u);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -8.0f}), BspContents::Solid);
  }

  TEST(BspHullTest, RefusesAForkThatNamesWhatIsNotThere)
  {
    BspHullBuilder head_outside;
    head_outside.Model(BspHullBuilder::empty_leaf, 5, solid);
    EXPECT_EQ(ReasonOfRefusal(head_outside), "hull 1 of model 0 names clip node 5, and there are 0");

    BspHullBuilder child_outside;
    child_outside.Model(BspHullBuilder::empty_leaf, child_outside.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, 7, solid), solid);
    EXPECT_EQ(ReasonOfRefusal(child_outside), "clip node 0 names clip node 7, and there are 1");

    BspHullBuilder plane_outside;
    plane_outside.Model(
      BspHullBuilder::empty_leaf, plane_outside.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, solid), solid);
    plane_outside.file.clip_nodes[0].plane = 3;
    EXPECT_EQ(ReasonOfRefusal(plane_outside), "clip node 0 names plane 3, and there are 1");
    plane_outside.file.clip_nodes[0].plane = -1;
    EXPECT_EQ(ReasonOfRefusal(plane_outside), "clip node 0 names plane -1, and there are 1");
  }

  TEST(BspHullTest, RefusesANodeOrALeafOfHullZeroThatIsNotThere)
  {
    BspHull hull;
    std::string error;

    BspHullBuilder leaf_outside;
    leaf_outside.Model(leaf_outside.Node({0.0f, 0.0f, 1.0f}, 0.0f, BspHullBuilder::empty_leaf, -9), solid, solid);
    EXPECT_FALSE(hull.Build(leaf_outside.file, 0, 0, error));
    EXPECT_EQ(error, "node 0 names leaf 8, and there are 2");

    BspHullBuilder node_outside;
    node_outside.Model(node_outside.Node({0.0f, 0.0f, 1.0f}, 0.0f, BspHullBuilder::empty_leaf, 4), solid, solid);
    EXPECT_FALSE(hull.Build(node_outside.file, 0, 0, error));
    EXPECT_EQ(error, "node 0 names node 4, and there are 1");

    BspHullBuilder head_outside;
    head_outside.Model(-4, solid, solid);
    EXPECT_FALSE(hull.Build(head_outside.file, 0, 0, error));
    EXPECT_EQ(error, "hull 0 of model 0 names leaf 3, and there are 2");
  }

  TEST(BspHullTest, RefusesWhatTheGameDoesNotKnowToFillAPlace)
  {
    BspHullBuilder clip;
    clip.Model(BspHullBuilder::empty_leaf, clip.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, -8), solid);
    EXPECT_EQ(ReasonOfRefusal(clip), "the space at clip node 0 is filled with -8, which the game does not know");

    BspHullBuilder beyond;
    beyond.Model(BspHullBuilder::empty_leaf, -15, solid);
    EXPECT_EQ(ReasonOfRefusal(beyond), "hull 1 of model 0 is filled with -15, which the game does not know");

    BspHullBuilder leaf;
    leaf.Model(leaf.Leaf(0), solid, solid);
    BspHull hull;
    std::string error;
    EXPECT_FALSE(hull.Build(leaf.file, 0, 0, error));
    EXPECT_EQ(error, "leaf 2 is filled with 0, which the game does not know");
  }

  TEST(BspHullTest, RefusesATreeThatLoops)
  {
    // a fork that is its own child
    BspHullBuilder itself;
    itself.Model(BspHullBuilder::empty_leaf, itself.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, 0, solid), solid);
    EXPECT_EQ(ReasonOfRefusal(itself), "hull 1 of model 0 reaches clip node 0 twice");

    // a loop of three
    BspHullBuilder loop;
    loop.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, 1, solid);
    loop.ClipNode({0.0f, 0.0f, 1.0f}, 8.0f, empty, 2);
    loop.ClipNode({0.0f, 0.0f, 1.0f}, 16.0f, 0, solid);
    loop.Model(BspHullBuilder::empty_leaf, 0, solid);
    EXPECT_EQ(ReasonOfRefusal(loop), "hull 1 of model 0 reaches clip node 0 twice");
  }

  TEST(BspHullTest, RefusesAForkThatTwoForksShare)
  {
    // no loop, and all the same a move would walk the shared part once for
    // every way to it, which doubles with every fork of this kind
    BspHullBuilder builder;
    const std::int16_t shared = builder.ClipNode({0.0f, 0.0f, 1.0f}, 0.0f, empty, solid);
    builder.Model(BspHullBuilder::empty_leaf, builder.ClipNode({1.0f, 0.0f, 0.0f}, 0.0f, shared, shared), solid);
    EXPECT_EQ(ReasonOfRefusal(builder), "hull 1 of model 0 reaches clip node 0 twice");
  }

  /// A level whose hull 1 is this many forks, one under the other: floors
  /// that lie lower and lower, with the solid under the last.
  BspHullBuilder MakeDeep(const std::size_t forks)
  {
    BspHullBuilder builder;
    std::int16_t next = BspHullBuilder::solid;
    for (std::size_t i = 0; i < forks; i++)
    {
      next = builder.ClipNode({0.0f, 0.0f, 1.0f}, -static_cast<float>(forks - i), empty, next);
    }
    builder.Model(BspHullBuilder::empty_leaf, next, solid);
    return builder;
  }

  TEST(BspHullTest, TakesATreeAsDeepAsIsAllowedAndRefusesADeeperOne)
  {
    const BspHull hull = MakeHull(MakeDeep(BspHull::deepest_tree));
    EXPECT_EQ(hull.GetNodes().size(), BspHull::deepest_tree);

    // a move through every fork of it
    const BspTraceResult result = hull.TraceLine({0.0f, 0.0f, 8.0f}, {0.0f, 0.0f, -2000.0f});
    EXPECT_LT(result.fraction, 1.0f);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(result.end_position.z, -1024.0f + epsilon, 1.0e-3f);

    EXPECT_EQ(ReasonOfRefusal(MakeDeep(BspHull::deepest_tree + 1)), "hull 1 of model 0 is more than 1024 forks deep");
  }

  TEST(BspHullTest, EndsAMoveBetweenPlacesThatAreNoNumbers)
  {
    const BspHull hull = MakeRoom();
    const float no_number = std::numeric_limits<float>::quiet_NaN();
    const float endless = std::numeric_limits<float>::infinity();

    // what comes of them is not looked at: they come back, and that is all
    (void)hull.TraceLine({no_number, 0.0f, 16.0f}, {40.0f, no_number, 16.0f});
    (void)hull.TraceLine({-endless, 0.0f, 16.0f}, {endless, 0.0f, 16.0f});
    (void)hull.TraceLine({32.0f, 32.0f, 16.0f}, {endless, endless, -endless});
    (void)hull.GetPointContents({no_number, no_number, no_number});
    SUCCEED();
  }

  TEST(BspHullTest, MakesABoxThatIsSolidInsideAndStopsAMoveAtItsSides)
  {
    BspHull hull;
    hull.BuildBox({-16.0f, -8.0f, 0.0f}, {16.0f, 8.0f, 32.0f});
    EXPECT_EQ(hull.GetNodes().size(), 6u);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 16.0f}), BspContents::Solid);
    EXPECT_EQ(hull.GetPointContents({0.0f, 9.0f, 16.0f}), BspContents::Empty);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, -1.0f}), BspContents::Empty);

    // from the side, and from above
    BspTraceResult result = hull.TraceLine({-100.0f, 0.0f, 16.0f}, {100.0f, 0.0f, 16.0f});
    EXPECT_NEAR(result.end_position.x, -16.0f - epsilon, 1.0e-3f);
    ExpectPlace(result.plane_normal, -1.0f, 0.0f, 0.0f);
    EXPECT_EQ(result.plane_distance, 16.0f);
    EXPECT_TRUE(result.in_open);
    EXPECT_FALSE(result.start_solid);

    result = hull.TraceLine({0.0f, 0.0f, 100.0f}, {0.0f, 0.0f, 16.0f});
    EXPECT_NEAR(result.end_position.z, 32.0f + epsilon, 1.0e-3f);
    ExpectPlace(result.plane_normal, 0.0f, 0.0f, 1.0f);

    // past it, and from inside it
    EXPECT_EQ(hull.TraceLine({-100.0f, 9.0f, 16.0f}, {100.0f, 9.0f, 16.0f}).fraction, 1.0f);
    result = hull.TraceLine({0.0f, 0.0f, 16.0f}, {0.0f, 0.0f, 20.0f});
    EXPECT_TRUE(result.start_solid);
    EXPECT_TRUE(result.all_solid);

    // made anew, it is the new box alone
    hull.BuildBox({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    EXPECT_EQ(hull.GetNodes().size(), 6u);
    EXPECT_EQ(hull.GetPointContents({0.0f, 0.0f, 16.0f}), BspContents::Empty);
  }
}
