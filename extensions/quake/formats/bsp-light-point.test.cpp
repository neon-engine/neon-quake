#include "bsp-light-point.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::BspEdge;
  using quake::BspFace;
  using quake::BspFile;
  using quake::BspLightFilter;
  using quake::BspLightPoint;
  using quake::BspLightSample;
  using quake::BspNode;
  using quake::BspPlane;
  using quake::BspTextureInfo;
  using quake::BspVector;
  using ::testing::HasSubstr;

  /// What a node names a leaf by. The level of the tests has one leaf.
  constexpr std::int32_t leaf = -1;

  /// A level with no floor yet: a world, one leaf, and two ways to lay a
  /// texture, both with `s = x` and `t = y`, so that a sample of a floor
  /// lies every sixteen units along X and Y. Texture info 0 is that of a
  /// wall, texture info 1 is special, as the sky and the liquids are.
  BspFile MakeEmpty()
  {
    BspFile file;
    file.leaves.emplace_back();
    file.edges.emplace_back();

    BspTextureInfo info;
    info.s_axis = {1.0f, 0.0f, 0.0f};
    info.t_axis = {0.0f, 1.0f, 0.0f};
    file.texture_infos.push_back(info);
    info.flags = BspTextureInfo::special;
    file.texture_infos.push_back(info);

    file.models.emplace_back();
    file.models[0].head_nodes[0] = leaf;
    return file;
  }

  /// Adds a floor to the level: a square face from 0 0 to 32 32 at a
  /// height, which looks up, on a node of its own. Its lightmap is 3 by 3
  /// samples, and `samples` are added to the lighting for it; without any
  /// it has no lightmap. Floors are added from the highest down: each new
  /// node is what lies behind the one before.
  BspFace &AddFloor(
    BspFile &file,
    const float height,
    const std::vector<std::uint8_t> &samples,
    const std::uint16_t texture_info = 0)
  {
    const auto first_vertex = static_cast<std::uint16_t>(file.vertices.size());
    file.vertices.push_back({0.0f, 0.0f, height});
    file.vertices.push_back({0.0f, 32.0f, height});
    file.vertices.push_back({32.0f, 32.0f, height});
    file.vertices.push_back({32.0f, 0.0f, height});

    BspFace face;
    face.plane = static_cast<std::uint16_t>(file.planes.size());
    face.first_edge = static_cast<std::int32_t>(file.face_edges.size());
    face.edge_count = 4;
    face.texture_info = texture_info;
    face.styles = {0, 255, 255, 255};
    face.light_offset = samples.empty() ? -1 : static_cast<std::int32_t>(file.lighting.size());
    file.lighting.insert(file.lighting.end(), samples.begin(), samples.end());

    // every second edge is kept the other way around and walked backwards
    for (std::uint16_t i = 0; i < 4; i++)
    {
      const auto from = static_cast<std::uint16_t>(first_vertex + i);
      const auto to = static_cast<std::uint16_t>(first_vertex + (i + 1) % 4);
      const auto edge = static_cast<std::int32_t>(file.edges.size());
      BspEdge made;
      made.vertices = i % 2 == 0 ? std::array<std::uint32_t, 2>{from, to} : std::array<std::uint32_t, 2>{to, from};
      file.face_edges.push_back(i % 2 == 0 ? edge : -edge);
      file.edges.push_back(made);
    }

    BspPlane plane;
    plane.normal = {0.0f, 0.0f, 1.0f};
    plane.distance = height;
    file.planes.push_back(plane);

    BspNode node;
    node.plane = static_cast<std::int32_t>(file.planes.size() - 1);
    node.children = {leaf, leaf};
    node.first_face = static_cast<std::uint16_t>(file.faces.size());
    node.face_count = 1;

    const auto number = static_cast<std::int32_t>(file.nodes.size());
    if (file.nodes.empty()) { file.models[0].head_nodes[0] = number; }
    else { file.nodes.back().children[1] = number; }
    file.nodes.push_back(node);

    file.faces.push_back(face);
    file.models[0].face_count++;
    return file.faces.back();
  }

  /// The samples of a floor: 10 at 0 0, 20 at 16 0, 30 at 32 0, 40 at
  /// 0 16, and so on to 90 at 32 32.
  const std::vector<std::uint8_t> tens = {10, 20, 30, 40, 50, 60, 70, 80, 90};

  BspLightPoint Make(const BspFile &file, const std::vector<std::uint8_t> &coloured = {})
  {
    BspLightPoint light;
    std::string error;
    EXPECT_TRUE(light.Build(file, error, coloured)) << error;
    return light;
  }

  /// What making it of a level says is wrong. Empty when nothing is.
  std::string ReasonOfRefusal(const BspFile &file, const std::vector<std::uint8_t> &coloured = {})
  {
    BspLightPoint light;
    std::string error;
    if (light.Build(file, error, coloured)) { return {}; }
    EXPECT_FALSE(error.empty());
    return error;
  }

  /// The light of a place where it is white: it checks that a face was
  /// found and that the three colours are the same.
  float WhiteAt(
    const BspLightPoint &light,
    const BspVector &point,
    const BspLightFilter filter = BspLightFilter::Nearest,
    const std::vector<float> &styles = {})
  {
    const BspLightSample sample = light.Sample(point, styles, filter);
    EXPECT_TRUE(sample.is_found);
    EXPECT_EQ(sample.green, sample.red);
    EXPECT_EQ(sample.blue, sample.red);
    return sample.red;
  }

  TEST(BspLightPointTest, GivesTheSampleUnderAPlaceAboveAFloor)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    const BspLightPoint light = Make(file);

    EXPECT_TRUE(light.HasLight());
    EXPECT_FLOAT_EQ(WhiteAt(light, {0.0f, 0.0f, 24.0f}), 10.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {32.0f, 0.0f, 24.0f}), 30.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {0.0f, 32.0f, 24.0f}), 70.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {32.0f, 32.0f, 24.0f}), 90.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, 24.0f}), 50.0f);

    // between samples it is the one at or before the place, as in the game
    EXPECT_FLOAT_EQ(WhiteAt(light, {8.0f, 8.0f, 24.0f}), 10.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {31.0f, 15.0f, 24.0f}), 20.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {15.0f, 31.0f, 24.0f}), 40.0f);
  }

  TEST(BspLightPointTest, BlendsTheFourSamplesAroundAPlaceWhenAsked)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    const BspLightPoint light = Make(file);
    constexpr BspLightFilter blend = BspLightFilter::Bilinear;

    // on a sample it is that sample
    EXPECT_FLOAT_EQ(WhiteAt(light, {0.0f, 0.0f, 24.0f}, blend), 10.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {32.0f, 32.0f, 24.0f}, blend), 90.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, 24.0f}, blend), 50.0f);

    // in the middle of 10, 20, 40, and 50
    EXPECT_FLOAT_EQ(WhiteAt(light, {8.0f, 8.0f, 24.0f}, blend), 30.0f);
    // a quarter of the way from 50 to 60, on the row itself
    EXPECT_FLOAT_EQ(WhiteAt(light, {20.0f, 16.0f, 24.0f}, blend), 52.5f);
    // three quarters of the way from 30 to 60, on the last column
    EXPECT_FLOAT_EQ(WhiteAt(light, {32.0f, 12.0f, 24.0f}, blend), 52.5f);
  }

  TEST(BspLightPointTest, SaysWhereTheFaceWasMet)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 8.0f, tens);
    const BspLightPoint light = Make(file);

    const BspLightSample sample = light.Sample({16.0f, 4.0f, 100.0f});
    ASSERT_TRUE(sample.is_found);
    EXPECT_EQ(sample.face, 0u);
    EXPECT_FLOAT_EQ(sample.place.x, 16.0f);
    EXPECT_FLOAT_EQ(sample.place.y, 4.0f);
    EXPECT_FLOAT_EQ(sample.place.z, 8.0f);

    // a thing that lies on the floor itself is over it
    EXPECT_TRUE(light.Sample({16.0f, 4.0f, 8.0f}).is_found);
  }

  TEST(BspLightPointTest, IsDarkWhereNoFaceLiesBelow)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    const BspLightPoint light = Make(file);

    const std::vector<BspVector> places = {
      {100.0f, 16.0f, 24.0f},   // next to the floor
      {16.0f, -1.0f, 24.0f},
      {16.0f, 16.0f, -1.0f},    // under it
      {16.0f, 16.0f, 9000.0f},  // further above it than is looked down
    };
    for (const BspVector &place : places)
    {
      const BspLightSample sample = light.Sample(place);
      EXPECT_FALSE(sample.is_found) << place.x << " " << place.y << " " << place.z;
      EXPECT_EQ(sample.red, 0.0f);
      EXPECT_EQ(sample.green, 0.0f);
      EXPECT_EQ(sample.blue, 0.0f);
    }

    EXPECT_TRUE(light.Sample({16.0f, 16.0f, 8000.0f}).is_found);
  }

  TEST(BspLightPointTest, IsFullyBrightEverywhereInALevelWithoutAnyLight)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, {});
    const BspLightPoint light = Make(file);

    EXPECT_FALSE(light.HasLight());
    for (const BspVector &place : {BspVector{16.0f, 16.0f, 24.0f}, BspVector{500.0f, 0.0f, 0.0f}})
    {
      const BspLightSample sample = light.Sample(place);
      EXPECT_FALSE(sample.is_found);
      EXPECT_EQ(sample.red, 255.0f);
      EXPECT_EQ(sample.green, 255.0f);
      EXPECT_EQ(sample.blue, 255.0f);
    }
  }

  TEST(BspLightPointTest, LooksThroughWaterAndSkyToTheFaceUnderIt)
  {
    // water, which is special and has no lightmap, over the floor
    BspFile under_water = MakeEmpty();
    AddFloor(under_water, 0.0f, {}, 1);
    AddFloor(under_water, -64.0f, tens);
    BspLightSample sample = Make(under_water).Sample({16.0f, 16.0f, 24.0f});
    ASSERT_TRUE(sample.is_found);
    EXPECT_EQ(sample.face, 1u);
    EXPECT_FLOAT_EQ(sample.red, 50.0f);
    EXPECT_FLOAT_EQ(sample.place.z, -64.0f);

    // a special face is looked through even when it names bytes of light
    BspFile lit_water = MakeEmpty();
    AddFloor(lit_water, 0.0f, std::vector<std::uint8_t>(9, 200), 1);
    AddFloor(lit_water, -64.0f, tens);
    sample = Make(lit_water).Sample({16.0f, 16.0f, 24.0f});
    ASSERT_TRUE(sample.is_found);
    EXPECT_FLOAT_EQ(sample.red, 50.0f);

    // the upper of two floors with light is the one that is found
    BspFile two_floors = MakeEmpty();
    AddFloor(two_floors, 0.0f, std::vector<std::uint8_t>(9, 200));
    AddFloor(two_floors, -64.0f, tens);
    const BspLightPoint light = Make(two_floors);
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, 24.0f}), 200.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, -8.0f}), 50.0f);
  }

  TEST(BspLightPointTest, FindsAFloorNoLightReachesAndCallsItDark)
  {
    // the tools that light a level write no lightmap for such a floor, and
    // the light of the room under it does not come through
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, {});
    AddFloor(file, -64.0f, tens);
    const BspLightPoint light = Make(file);

    for (const BspLightFilter filter : {BspLightFilter::Nearest, BspLightFilter::Bilinear})
    {
      const BspLightSample sample = light.Sample({16.0f, 16.0f, 24.0f}, {}, filter);
      ASSERT_TRUE(sample.is_found);
      EXPECT_EQ(sample.face, 0u);
      EXPECT_FLOAT_EQ(sample.place.z, 0.0f);
      EXPECT_EQ(sample.red, 0.0f);
      EXPECT_EQ(sample.green, 0.0f);
      EXPECT_EQ(sample.blue, 0.0f);
    }

    // next to it nothing is found, as next to any floor
    EXPECT_FALSE(light.Sample({100.0f, 16.0f, 24.0f}).is_found);

    // a floor with light right under the dark one is taken in its place
    BspFile near = MakeEmpty();
    AddFloor(near, 0.0f, {});
    AddFloor(near, -7.0f, tens);
    BspLightSample close = Make(near).Sample({16.0f, 16.0f, 24.0f});
    ASSERT_TRUE(close.is_found);
    EXPECT_EQ(close.face, 1u);
    EXPECT_FLOAT_EQ(close.red, 50.0f);

    BspFile far = MakeEmpty();
    AddFloor(far, 0.0f, {});
    AddFloor(far, -9.0f, tens);
    close = Make(far).Sample({16.0f, 16.0f, 24.0f});
    ASSERT_TRUE(close.is_found);
    EXPECT_EQ(close.face, 0u);
    EXPECT_EQ(close.red, 0.0f);

    // and so is one on the same plane, whichever the node names first
    for (const bool dark_first : {true, false})
    {
      BspFile side_by_side = MakeEmpty();
      AddFloor(side_by_side, 0.0f, dark_first ? std::vector<std::uint8_t>() : tens);
      AddFloor(side_by_side, 0.0f, dark_first ? tens : std::vector<std::uint8_t>());
      // both on the first node
      side_by_side.nodes.pop_back();
      side_by_side.nodes[0].children = {leaf, leaf};
      side_by_side.nodes[0].face_count = 2;
      close = Make(side_by_side).Sample({16.0f, 16.0f, 24.0f});
      ASSERT_TRUE(close.is_found);
      EXPECT_EQ(close.face, dark_first ? 1u : 0u);
      EXPECT_FLOAT_EQ(close.red, 50.0f);
    }

    // a face that names no style has no sample either
    BspFile without_style = MakeEmpty();
    AddFloor(without_style, 0.0f, tens).styles = {255, 255, 255, 255};
    const BspLightSample sample = Make(without_style).Sample({16.0f, 16.0f, 24.0f});
    EXPECT_TRUE(sample.is_found);
    EXPECT_EQ(sample.red, 0.0f);
  }

  TEST(BspLightPointTest, AddsTheStylesOfAFaceEachTimesHowBrightItIs)
  {
    BspFile file = MakeEmpty();
    std::vector<std::uint8_t> samples = tens;
    samples.insert(samples.end(), 9, 200);
    AddFloor(file, 0.0f, samples).styles = {0, 3, 255, 255};
    const BspLightPoint light = Make(file);
    const BspVector middle{16.0f, 16.0f, 24.0f};

    // without any, every style is as bright as the level was lit
    EXPECT_FLOAT_EQ(WhiteAt(light, middle), 250.0f);

    std::vector<float> styles(BspLightPoint::style_count, 1.0f);
    styles[3] = 0.25f;
    EXPECT_FLOAT_EQ(WhiteAt(light, middle, BspLightFilter::Nearest, styles), 100.0f);
    EXPECT_FLOAT_EQ(WhiteAt(light, {8.0f, 8.0f, 24.0f}, BspLightFilter::Bilinear, styles), 80.0f);

    styles[0] = 2.0f;
    styles[3] = 0.0f;
    EXPECT_FLOAT_EQ(WhiteAt(light, middle, BspLightFilter::Nearest, styles), 100.0f);

    // nothing is cut off at 255
    styles[3] = 2.0f;
    EXPECT_FLOAT_EQ(WhiteAt(light, middle, BspLightFilter::Nearest, styles), 500.0f);

    // a style that the values do not reach counts as 1
    EXPECT_FLOAT_EQ(WhiteAt(light, middle, BspLightFilter::Nearest, {0.5f}), 225.0f);
  }

  TEST(BspLightPointTest, ReadsColouredLightInPlaceOfTheWhite)
  {
    BspFile file = MakeEmpty();
    // four bytes of another face first, so that the place of the floor in
    // the coloured light is three times its place in the white
    file.lighting.assign(4, 0);
    AddFloor(file, 0.0f, tens);

    std::vector<std::uint8_t> coloured(4 * 3, 0);
    for (int i = 0; i < 9; i++)
    {
      coloured.push_back(static_cast<std::uint8_t>(100 + i));
      coloured.push_back(static_cast<std::uint8_t>(10 * i));
      coloured.push_back(static_cast<std::uint8_t>(200 - i));
    }
    const BspLightPoint light = Make(file, coloured);

    BspLightSample sample = light.Sample({32.0f, 0.0f, 24.0f});
    ASSERT_TRUE(sample.is_found);
    EXPECT_FLOAT_EQ(sample.red, 102.0f);
    EXPECT_FLOAT_EQ(sample.green, 20.0f);
    EXPECT_FLOAT_EQ(sample.blue, 198.0f);

    // in the middle of samples 0, 1, 3, and 4
    sample = light.Sample({8.0f, 8.0f, 24.0f}, {}, BspLightFilter::Bilinear);
    ASSERT_TRUE(sample.is_found);
    EXPECT_FLOAT_EQ(sample.red, 102.0f);
    EXPECT_FLOAT_EQ(sample.green, 20.0f);
    EXPECT_FLOAT_EQ(sample.blue, 198.0f);
  }

  TEST(BspLightPointTest, KeepsNothingOfTheLevelAndOfTheColouredLight)
  {
    BspLightPoint light;
    {
      BspFile file = MakeEmpty();
      AddFloor(file, 0.0f, tens);
      std::vector<std::uint8_t> coloured(27, 77);
      std::string error;
      ASSERT_TRUE(light.Build(file, error, coloured)) << error;
      coloured.assign(27, 0);
    }
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, 24.0f}), 77.0f);
  }

  TEST(BspLightPointTest, FindsNothingForAPlaceThatIsNoNumber)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    const BspLightPoint light = Make(file);

    constexpr float no_number = std::numeric_limits<float>::quiet_NaN();
    constexpr float endless = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(light.Sample({no_number, 16.0f, 24.0f}).is_found);
    EXPECT_FALSE(light.Sample({16.0f, 16.0f, no_number}).is_found);
    EXPECT_FALSE(light.Sample({endless, 16.0f, 24.0f}).is_found);
    EXPECT_FALSE(light.Sample({16.0f, 16.0f, endless}).is_found);
    EXPECT_FALSE(light.Sample({16.0f, 16.0f, -endless}).is_found);
  }

  TEST(BspLightPointTest, IsMadeOfAWorldThatIsASingleLeaf)
  {
    BspFile file = MakeEmpty();
    file.lighting.assign(4, 0);
    const BspLightPoint light = Make(file);
    EXPECT_FALSE(light.Sample({0.0f, 0.0f, 0.0f}).is_found);
  }

  TEST(BspLightPointTest, RefusesALevelThatNamesWhatIsNotThere)
  {
    BspFile file;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("no model"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    EXPECT_THAT(ReasonOfRefusal(file, std::vector<std::uint8_t>(26)), HasSubstr("coloured light has 26 bytes"));

    // the lightmap of 3 by 3 samples ends past the lighting
    file.lighting.pop_back();
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("face 0 has 1 lightmaps of 3 by 3 samples from byte 0"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens).styles = {0, 1, 255, 255};
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("face 0 has 2 lightmaps"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens).light_offset = -2;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("from byte -2"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens).texture_info = 2;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("face 0 names texture info 2"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens).first_edge = 2;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("names edges of faces that are not there"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.face_edges[1] = 99;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("face 0 names edge 99"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.edges[2].vertices = {50, 50};
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("face 0 names vertex 50"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.vertices[2].x = std::numeric_limits<float>::infinity();
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("lies nowhere on its texture"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.nodes[0].plane = 7;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("node 0 names plane 7"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.nodes[0].face_count = 2;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("node 0 names 2 faces from 0"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.nodes[0].children[0] = 5;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("node 0 names node 5"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.models[0].head_nodes[0] = 3;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("model 0 names node 3"));
  }

  TEST(BspLightPointTest, RefusesATreeThatLoopsOrIsTooDeep)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    AddFloor(file, -64.0f, tens);
    file.nodes[1].children[0] = 0;
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("reached twice"));

    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.nodes[0].children = {0, leaf};
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("reached twice"));

    // a chain of forks, each behind the one before, one longer than allowed
    file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    file.nodes[0].face_count = 0;
    file.nodes.resize(BspLightPoint::deepest_tree + 1, file.nodes[0]);
    for (std::size_t i = 0; i + 1 < file.nodes.size(); i++)
    {
      file.nodes[i].children = {leaf, static_cast<std::int32_t>(i + 1)};
    }
    EXPECT_THAT(ReasonOfRefusal(file), HasSubstr("deeper than"));

    file.nodes.pop_back();
    file.nodes.back().children = {leaf, leaf};
    EXPECT_THAT(ReasonOfRefusal(file), testing::IsEmpty());
  }

  TEST(BspLightPointTest, StaysAsItWasWhenALevelIsRefused)
  {
    BspFile file = MakeEmpty();
    AddFloor(file, 0.0f, tens);
    BspLightPoint light = Make(file);

    file.lighting.clear();
    std::string error;
    EXPECT_FALSE(light.Build(file, error));
    EXPECT_FLOAT_EQ(WhiteAt(light, {16.0f, 16.0f, 24.0f}), 50.0f);
  }
} // namespace
