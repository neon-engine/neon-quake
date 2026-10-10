#include "bsp-light-visibility.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bsp-face.hpp"
#include "bsp-hull-builder.test.hpp"
#include "bsp-moment-light.hpp"

namespace
{
  using quake::BspFace;
  using quake::BspHullBuilder;
  using quake::BspLightVisibility;
  using quake::BspMomentLight;
  using quake::BspVector;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// Two rooms side by side along X, a wall at x = 0 between them: the
  /// west room is leaf 1 with faces 0 and 1, the east room leaf 2 with
  /// faces 2 and 3. The wall is solid, 16 units thick, leaf 0. Each room
  /// sees itself alone, as a level with a wall between two rooms says.
  class TwoRooms
  {
  public:
    BspHullBuilder builder;

    TwoRooms()
    {
      quake::BspFile &file = builder.file;
      // the builder starts with leaf 0 solid and leaf 1 empty: leaf 1 is
      // the west room, and a second empty leaf the east room
      const std::int32_t west = BspHullBuilder::empty_leaf;
      const std::int32_t east = builder.Leaf(BspHullBuilder::empty);

      file.faces.resize(4);
      file.leaf_faces = {0, 1, 2, 3};
      file.leaves[1].first_leaf_face = 0;
      file.leaves[1].leaf_face_count = 2;
      file.leaves[1].mins = {-256.0f, -128.0f, 0.0f};
      file.leaves[1].maxs = {-8.0f, 128.0f, 128.0f};
      file.leaves[2].first_leaf_face = 2;
      file.leaves[2].leaf_face_count = 2;
      file.leaves[2].mins = {8.0f, -128.0f, 0.0f};
      file.leaves[2].maxs = {256.0f, 128.0f, 128.0f};

      // each room sees itself: one byte a row, bit 0 is leaf 1, bit 1 leaf 2
      file.visibility = {0x01, 0x02};
      file.leaves[1].visibility_offset = 0;
      file.leaves[2].visibility_offset = 1;

      // the wall: what is beyond x = 8 is east, what is before x = -8 west
      const std::int32_t wall = builder.Node({1.0f, 0.0f, 0.0f}, -8.0f, BspHullBuilder::solid_leaf, west);
      const std::int32_t split = builder.Node({1.0f, 0.0f, 0.0f}, 8.0f, east, wall);
      builder.Model(split, BspHullBuilder::solid_leaf, BspHullBuilder::solid_leaf);
    }

    BspLightVisibility Make() const
    {
      BspLightVisibility visibility;
      std::string error;
      EXPECT_TRUE(visibility.Build(builder.file, error)) << error;
      return visibility;
    }
  };

  std::vector<std::uint32_t> MasksOf(const BspLightVisibility &visibility, const std::vector<BspMomentLight> &lights)
  {
    std::vector<std::uint32_t> masks;
    visibility.Mark(lights, masks);
    return masks;
  }

  TEST(BspLightVisibilityTest, FindsTheLeafAPlaceLiesIn)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    EXPECT_EQ(visibility.FindLeaf({-100.0f, 0.0f, 32.0f}), 1u);
    EXPECT_EQ(visibility.FindLeaf({100.0f, 0.0f, 32.0f}), 2u);
    EXPECT_EQ(visibility.FindLeaf({0.0f, 0.0f, 32.0f}), 0u);
  }

  TEST(BspLightVisibilityTest, ALightReachesTheFacesOfTheRoomItStandsInAndNotThoseBeyondTheWall)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 0u, 0u));
    EXPECT_THAT(MasksOf(visibility, {{{100.0f, 0.0f, 32.0f}, 200.0f, 3}}), ElementsAre(0u, 0u, 8u, 8u));
  }

  TEST(BspLightVisibilityTest, EachLightHasItsOwnBitAndTheMasksAddUp)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    const std::vector<BspMomentLight> lights = {
      {{-100.0f, 0.0f, 32.0f}, 200.0f, 0},
      {{-50.0f, 0.0f, 32.0f}, 200.0f, 5},
      {{100.0f, 0.0f, 32.0f}, 200.0f, 1},
    };
    EXPECT_THAT(MasksOf(visibility, lights), ElementsAre(33u, 33u, 2u, 2u));
  }

  TEST(BspLightVisibilityTest, ALightInTheSolidReachesNothing)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    EXPECT_THAT(MasksOf(visibility, {{{0.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(0u, 0u, 0u, 0u));
  }

  TEST(BspLightVisibilityTest, ABitBeyondTheMaskIsLeftOut)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 32}}), ElementsAre(0u, 0u, 0u, 0u));
  }

  TEST(BspLightVisibilityTest, WithoutVisibilityALightReachesEveryRoomWithinItsReach)
  {
    TwoRooms rooms;
    rooms.builder.file.visibility.clear();
    const BspLightVisibility visibility = rooms.Make();

    // the east room starts at x = 8, 108 units from the light
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 1u, 1u));
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 100.0f, 0}}), ElementsAre(1u, 1u, 0u, 0u));
  }

  TEST(BspLightVisibilityTest, ALeafThatSeesEverythingReachesEveryRoomToo)
  {
    TwoRooms rooms;
    rooms.builder.file.leaves[1].visibility_offset = -1;
    const BspLightVisibility visibility = rooms.Make();
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 1u, 1u));
  }

  TEST(BspLightVisibilityTest, ReadsARunOfUnseenLeavesAsTheGamePacksIt)
  {
    TwoRooms rooms;
    // a run of one byte of 0 for the west room: it sees nothing but itself,
    // which is always reached; and the east room sees both
    rooms.builder.file.visibility = {0x00, 0x01, 0x03};
    rooms.builder.file.leaves[1].visibility_offset = 0;
    rooms.builder.file.leaves[2].visibility_offset = 2;
    const BspLightVisibility visibility = rooms.Make();
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 0u, 0u));
    EXPECT_THAT(MasksOf(visibility, {{{100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 1u, 1u));
  }

  TEST(BspLightVisibilityTest, VisibilityThatEndsShortLeavesTheRestUnseen)
  {
    TwoRooms rooms;
    rooms.builder.file.visibility = {0x02};
    rooms.builder.file.leaves[1].visibility_offset = 0;
    rooms.builder.file.leaves[2].visibility_offset = 1;
    const BspLightVisibility visibility = rooms.Make();
    EXPECT_THAT(MasksOf(visibility, {{{-100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(1u, 1u, 1u, 1u));
    EXPECT_THAT(MasksOf(visibility, {{{100.0f, 0.0f, 32.0f}, 200.0f, 0}}), ElementsAre(0u, 0u, 1u, 1u));
  }

  TEST(BspLightVisibilityTest, HasAsManyMasksAsTheLevelHasFacesAndNoneWithoutLights)
  {
    const BspLightVisibility visibility = TwoRooms().Make();
    EXPECT_EQ(visibility.GetFaceCount(), 4u);
    EXPECT_THAT(MasksOf(visibility, {}), ElementsAre(0u, 0u, 0u, 0u));
  }

  TEST(BspLightVisibilityTest, RefusesALevelWithoutAWorld)
  {
    BspLightVisibility visibility;
    std::string error;
    EXPECT_FALSE(visibility.Build(quake::BspFile{}, error));
    EXPECT_THAT(error, HasSubstr("no world"));
  }

  TEST(BspLightVisibilityTest, RefusesAForkThatNamesALeafThatIsNotThere)
  {
    TwoRooms rooms;
    rooms.builder.file.nodes[0].children[1] = -50;
    BspLightVisibility visibility;
    std::string error;
    EXPECT_FALSE(visibility.Build(rooms.builder.file, error));
    EXPECT_THAT(error, HasSubstr("leaf 49"));
  }

  TEST(BspLightVisibilityTest, RefusesALeafThatNamesAFaceThatIsNotThere)
  {
    TwoRooms rooms;
    rooms.builder.file.leaf_faces[3] = 9;
    BspLightVisibility visibility;
    std::string error;
    EXPECT_FALSE(visibility.Build(rooms.builder.file, error));
    EXPECT_THAT(error, HasSubstr("face 9"));
  }

  TEST(BspLightVisibilityTest, EndsOnATreeThatGoesRound)
  {
    TwoRooms rooms;
    rooms.builder.file.nodes[1].children[0] = 1;
    const BspLightVisibility visibility = rooms.Make();
    EXPECT_EQ(visibility.FindLeaf({100.0f, 0.0f, 32.0f}), 0u);
  }
}
