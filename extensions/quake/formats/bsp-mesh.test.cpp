#include "bsp-mesh.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "real-data.test.hpp"

namespace
{
  using quake::BspEdge;
  using quake::BspFace;
  using quake::BspFile;
  using quake::BspFormat;
  using quake::BspMesh;
  using quake::BspMeshFace;
  using quake::BspMeshVertex;
  using quake::BspModel;
  using quake::BspPlane;
  using quake::BspTextureInfo;
  using quake::BspVector;
  using quake::MipTexture;
  using quake::RealData;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  MipTexture MakeTexture(const std::string &name, const std::uint32_t width, const std::uint32_t height)
  {
    MipTexture texture;
    texture.name = name;
    texture.width = width;
    texture.height = height;
    return texture;
  }

  /// A level with nothing in it but what faces need: a plane that looks up,
  /// a texture `wall` of 64 by 32 pixels, and a texture info that lays it
  /// along X, shifted by 8 pixels, and against Y: `s = x + 8`, `t = -y`.
  /// Edge 0 is there and never used, as in every level.
  BspFile MakeEmpty()
  {
    BspFile file;

    BspPlane plane;
    plane.normal = {0.0f, 0.0f, 1.0f};
    file.planes.push_back(plane);

    file.textures.emplace_back(MakeTexture("wall", 64, 32));

    BspTextureInfo info;
    info.s_axis = {1.0f, 0.0f, 0.0f};
    info.s_offset = 8.0f;
    info.t_axis = {0.0f, -1.0f, 0.0f};
    info.t_offset = 0.0f;
    info.texture = 0;
    file.texture_infos.push_back(info);

    file.edges.emplace_back();
    file.models.emplace_back();
    return file;
  }

  /// Adds a face with these corners to the level and to its last model. The
  /// face has no lightmap. Every second edge is kept the other way around
  /// and walked backwards, so that both ways are read.
  BspFace &AddFace(BspFile &file, const std::vector<BspVector> &corners, const std::uint32_t texture_info = 0)
  {
    const auto first_vertex = static_cast<std::uint32_t>(file.vertices.size());
    const auto count = static_cast<std::uint32_t>(corners.size());
    file.vertices.insert(file.vertices.end(), corners.begin(), corners.end());

    BspFace face;
    face.plane = 0;
    face.first_edge = static_cast<std::int32_t>(file.face_edges.size());
    face.edge_count = count;
    face.texture_info = texture_info;
    face.styles = {0, 255, 255, 255};
    face.light_offset = -1;

    for (std::uint32_t i = 0; i < count; i++)
    {
      const auto from = static_cast<std::uint32_t>(first_vertex + i);
      const auto to = static_cast<std::uint32_t>(first_vertex + (i + 1) % count);
      const auto edge = static_cast<std::int32_t>(file.edges.size());

      BspEdge made;
      if (i % 2 == 0)
      {
        made.vertices = {from, to};
        file.face_edges.push_back(edge);
      } else
      {
        made.vertices = {to, from};
        file.face_edges.push_back(-edge);
      }
      file.edges.push_back(made);
    }

    file.faces.push_back(face);
    file.models.back().face_count++;
    return file.faces.back();
  }

  /// The level of `MakeEmpty()` with a floor of four corners, 100 by 48
  /// units, whose lightmap of 8 by 4 samples starts at byte 4 of the
  /// lighting and counts up from 0.
  BspFile MakeQuad()
  {
    BspFile file = MakeEmpty();
    BspFace &face = AddFace(
      file, {{0.0f, 0.0f, 0.0f}, {0.0f, 48.0f, 0.0f}, {100.0f, 48.0f, 0.0f}, {100.0f, 0.0f, 0.0f}});
    face.light_offset = 4;

    file.lighting.assign(4, 0xaa);
    for (int i = 0; i < 32; i++) { file.lighting.push_back(static_cast<std::uint8_t>(i)); }
    return file;
  }

  /// What making the mesh of the world of a level says is wrong. Empty when
  /// nothing is.
  std::string ReasonOfRefusal(const BspFile &file, const std::size_t model = 0)
  {
    BspMesh mesh;
    std::string error;
    if (mesh.Build(file, model, error)) { return {}; }
    EXPECT_FALSE(error.empty());
    return error;
  }

  TEST(BspMeshTest, MakesAFanOfTwoTrianglesOfAFaceWithFourCorners)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeQuad(), 0, error)) << error;

    ASSERT_EQ(mesh.vertices.size(), 4u);
    ASSERT_EQ(mesh.groups.size(), 1u);
    EXPECT_EQ(mesh.groups[0].texture, 0);
    EXPECT_FALSE(mesh.groups[0].is_sky);
    EXPECT_FALSE(mesh.groups[0].is_liquid);
    EXPECT_THAT(mesh.groups[0].indices, ElementsAre(0, 1, 2, 0, 2, 3));

    ASSERT_EQ(mesh.faces.size(), 1u);
    EXPECT_EQ(mesh.faces[0].face, 0u);
    EXPECT_EQ(mesh.faces[0].texture, 0);
    EXPECT_EQ(mesh.faces[0].first_vertex, 0u);
    EXPECT_EQ(mesh.faces[0].vertex_count, 4u);
    EXPECT_FALSE(mesh.faces[0].is_sky);
    EXPECT_FALSE(mesh.faces[0].is_liquid);
  }

  TEST(BspMeshTest, KeepsThePlacesOfTheCornersInTheAxesOfTheGame)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeQuad(), 0, error)) << error;
    ASSERT_EQ(mesh.vertices.size(), 4u);

    // the corners in the order of the file, the second and fourth of which
    // come from edges walked backwards
    EXPECT_EQ(mesh.vertices[0].position.x, 0.0f);
    EXPECT_EQ(mesh.vertices[0].position.y, 0.0f);
    EXPECT_EQ(mesh.vertices[1].position.x, 0.0f);
    EXPECT_EQ(mesh.vertices[1].position.y, 48.0f);
    EXPECT_EQ(mesh.vertices[2].position.x, 100.0f);
    EXPECT_EQ(mesh.vertices[2].position.y, 48.0f);
    EXPECT_EQ(mesh.vertices[3].position.x, 100.0f);
    EXPECT_EQ(mesh.vertices[3].position.y, 0.0f);

    for (const BspMeshVertex &vertex : mesh.vertices)
    {
      EXPECT_EQ(vertex.position.z, 0.0f);
      EXPECT_EQ(vertex.normal.x, 0.0f);
      EXPECT_EQ(vertex.normal.y, 0.0f);
      EXPECT_EQ(vertex.normal.z, 1.0f);
    }
  }

  TEST(BspMeshTest, TurnsTheNormalOfAFaceOnTheBackOfItsPlane)
  {
    BspFile file = MakeQuad();
    file.planes[0].normal = {0.0f, 0.6f, -0.8f};
    file.faces[0].side = 1;

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    for (const BspMeshVertex &vertex : mesh.vertices)
    {
      EXPECT_EQ(vertex.normal.x, 0.0f);
      EXPECT_EQ(vertex.normal.y, -0.6f);
      EXPECT_EQ(vertex.normal.z, 0.8f);
    }
  }

  TEST(BspMeshTest, CountsTextureCoordinatesInWidthsAndHeightsOfTheTexture)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeQuad(), 0, error)) << error;
    ASSERT_EQ(mesh.vertices.size(), 4u);

    // s = x + 8 over a width of 64, t = -y over a height of 32
    EXPECT_FLOAT_EQ(mesh.vertices[0].texture_u, 8.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[0].texture_v, 0.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[1].texture_u, 8.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[1].texture_v, -48.0f / 32.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].texture_u, 108.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].texture_v, -48.0f / 32.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[3].texture_u, 108.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[3].texture_v, 0.0f);
  }

  TEST(BspMeshTest, CountsTheTextureCoordinatesOfATextureThatIsNotCarriedInTheStandIn)
  {
    BspFile file = MakeQuad();
    file.textures[0].reset();

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_FLOAT_EQ(mesh.vertices[2].texture_u, 108.0f / 16.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].texture_v, -48.0f / 16.0f);
    EXPECT_FALSE(mesh.faces[0].is_sky);
    EXPECT_FALSE(mesh.faces[0].is_liquid);
  }

  TEST(BspMeshTest, FindsTheSizeOfTheLightmapOfAFaceAndHandsOutItsBytes)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeQuad(), 0, error)) << error;
    ASSERT_EQ(mesh.faces.size(), 1u);
    const BspMeshFace &face = mesh.faces[0];

    // s runs from 8 to 108: lines 0 to 7. t runs from -48 to 0: lines -3 to 0
    EXPECT_EQ(face.lightmap_width, 8u);
    EXPECT_EQ(face.lightmap_height, 4u);
    EXPECT_THAT(face.light_styles, ElementsAre(0, 255, 255, 255));
    EXPECT_EQ(face.CountLightStyles(), 1u);
    EXPECT_EQ(face.light_offset, 4);

    ASSERT_EQ(face.lightmap.size(), 32u);
    EXPECT_EQ(face.lightmap[0], 0);
    EXPECT_EQ(face.lightmap[31], 31);
  }

  TEST(BspMeshTest, CountsLightmapCoordinatesWithinTheLightmapOfTheFace)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeQuad(), 0, error)) << error;
    ASSERT_EQ(mesh.vertices.size(), 4u);

    // (s - 0 + 8) / (8 * 16) and (t + 48 + 8) / (4 * 16)
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_u, 16.0f / 128.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_v, 56.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[1].lightmap_u, 16.0f / 128.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[1].lightmap_v, 8.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_u, 116.0f / 128.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_v, 8.0f / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[3].lightmap_u, 116.0f / 128.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[3].lightmap_v, 56.0f / 64.0f);
  }

  TEST(BspMeshTest, PutsCornersOnLinesOfSixteenOnTheFirstAndLastSample)
  {
    // s runs from 0 to 32 and t from -32 to 0, both exactly on lines
    BspFile file = MakeEmpty();
    file.texture_infos[0].s_offset = 0.0f;
    AddFace(file, {{0.0f, 0.0f, 0.0f}, {0.0f, 32.0f, 0.0f}, {32.0f, 32.0f, 0.0f}, {32.0f, 0.0f, 0.0f}});

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_EQ(mesh.faces[0].lightmap_width, 3u);
    EXPECT_EQ(mesh.faces[0].lightmap_height, 3u);
    // the middle of the first sample and the middle of the last
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_u, 8.0f / 48.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_u, 40.0f / 48.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_v, 8.0f / 48.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_v, 40.0f / 48.0f);
  }

  TEST(BspMeshTest, RoundsTheLeastCornerDownAndTheGreatestUpBelowZeroToo)
  {
    // s runs from -24 to -8: lines -2 to 0. t runs from -40 to -4: lines -3 to 0
    BspFile file = MakeEmpty();
    file.texture_infos[0].s_offset = 0.0f;
    AddFace(file, {{-24.0f, 4.0f, 0.0f}, {-24.0f, 40.0f, 0.0f}, {-8.0f, 40.0f, 0.0f}, {-8.0f, 4.0f, 0.0f}});

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_EQ(mesh.faces[0].lightmap_width, 3u);
    EXPECT_EQ(mesh.faces[0].lightmap_height, 4u);
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_u, (-24.0f + 32.0f + 8.0f) / 48.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_u, (-8.0f + 32.0f + 8.0f) / 48.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_v, (-4.0f + 48.0f + 8.0f) / 64.0f);
    EXPECT_FLOAT_EQ(mesh.vertices[2].lightmap_v, (-40.0f + 48.0f + 8.0f) / 64.0f);
  }

  TEST(BspMeshTest, HandsOutOneLightmapForEveryStyleOfAFace)
  {
    BspFile file = MakeQuad();
    file.faces[0].styles = {0, 3, 255, 255};
    file.faces[0].light_offset = 0;
    file.lighting.assign(32, 10);
    file.lighting.insert(file.lighting.end(), 32, 20);

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_THAT(mesh.faces[0].light_styles, ElementsAre(0, 3, 255, 255));
    EXPECT_EQ(mesh.faces[0].CountLightStyles(), 2u);
    ASSERT_EQ(mesh.faces[0].lightmap.size(), 64u);
    EXPECT_EQ(mesh.faces[0].lightmap[31], 10);
    EXPECT_EQ(mesh.faces[0].lightmap[32], 20);
  }

  TEST(BspMeshTest, KeepsTheSizeOfAFaceWithoutALightmapAndHandsOutNoBytes)
  {
    BspFile file = MakeQuad();
    file.faces[0].light_offset = -1;

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_EQ(mesh.faces[0].light_offset, -1);
    EXPECT_EQ(mesh.faces[0].lightmap_width, 8u);
    EXPECT_EQ(mesh.faces[0].lightmap_height, 4u);
    EXPECT_THAT(mesh.faces[0].lightmap, IsEmpty());
    EXPECT_FLOAT_EQ(mesh.vertices[0].lightmap_u, 16.0f / 128.0f);
  }

  TEST(BspMeshTest, MarksTheSkyAndGivesItTrianglesAndNoLightmap)
  {
    BspFile file = MakeQuad();
    file.textures[0] = MakeTexture("sky4", 256, 128);
    file.texture_infos[0].flags = BspTextureInfo::special;
    file.faces[0].light_offset = -1;

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    ASSERT_EQ(mesh.faces.size(), 1u);
    EXPECT_TRUE(mesh.faces[0].is_sky);
    EXPECT_FALSE(mesh.faces[0].is_liquid);
    EXPECT_EQ(mesh.faces[0].lightmap_width, 0u);
    EXPECT_EQ(mesh.faces[0].lightmap_height, 0u);
    EXPECT_THAT(mesh.faces[0].lightmap, IsEmpty());
    EXPECT_EQ(mesh.vertices[2].lightmap_u, 0.0f);
    EXPECT_EQ(mesh.vertices[2].lightmap_v, 0.0f);

    ASSERT_EQ(mesh.groups.size(), 1u);
    EXPECT_TRUE(mesh.groups[0].is_sky);
    EXPECT_THAT(mesh.groups[0].indices, ElementsAre(0, 1, 2, 0, 2, 3));
  }

  TEST(BspMeshTest, MarksALiquidWhetherItHasALightmapOrNot)
  {
    // the old tools make a liquid special, the new ones may light it
    BspFile special = MakeQuad();
    special.textures[0] = MakeTexture("*water1", 64, 64);
    special.texture_infos[0].flags = BspTextureInfo::special;
    special.faces[0].light_offset = -1;

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(special, 0, error)) << error;
    EXPECT_TRUE(mesh.faces[0].is_liquid);
    EXPECT_FALSE(mesh.faces[0].is_sky);
    EXPECT_TRUE(mesh.groups[0].is_liquid);
    EXPECT_EQ(mesh.faces[0].lightmap_width, 0u);
    EXPECT_EQ(mesh.groups[0].indices.size(), 6u);

    BspFile lit = MakeQuad();
    lit.textures[0] = MakeTexture("*water1", 64, 64);
    ASSERT_TRUE(mesh.Build(lit, 0, error)) << error;
    EXPECT_TRUE(mesh.faces[0].is_liquid);
    EXPECT_EQ(mesh.faces[0].lightmap_width, 8u);
    EXPECT_EQ(mesh.faces[0].lightmap.size(), 32u);
  }

  TEST(BspMeshTest, GroupsTheTrianglesByTextureInTheOrderOfTheTextures)
  {
    BspFile file = MakeEmpty();
    file.textures.emplace_back(MakeTexture("*lava1", 64, 64));
    file.textures.emplace_back(MakeTexture("unused", 64, 64));
    BspTextureInfo lava = file.texture_infos[0];
    lava.texture = 1;
    file.texture_infos.push_back(lava);

    // a triangle of lava, then a wall of five corners, then another triangle of lava
    AddFace(file, {{0.0f, 0.0f, 0.0f}, {0.0f, 16.0f, 0.0f}, {16.0f, 0.0f, 0.0f}}, 1);
    AddFace(
      file, {{0.0f, 0.0f, 0.0f}, {0.0f, 16.0f, 0.0f}, {8.0f, 24.0f, 0.0f}, {16.0f, 16.0f, 0.0f}, {16.0f, 0.0f, 0.0f}});
    AddFace(file, {{32.0f, 0.0f, 0.0f}, {32.0f, 16.0f, 0.0f}, {48.0f, 0.0f, 0.0f}}, 1);

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    ASSERT_EQ(mesh.vertices.size(), 11u);
    ASSERT_EQ(mesh.faces.size(), 3u);
    EXPECT_EQ(mesh.faces[1].first_vertex, 3u);
    EXPECT_EQ(mesh.faces[1].vertex_count, 5u);
    EXPECT_EQ(mesh.faces[2].first_vertex, 8u);

    ASSERT_EQ(mesh.groups.size(), 2u);
    EXPECT_EQ(mesh.groups[0].texture, 0);
    EXPECT_FALSE(mesh.groups[0].is_liquid);
    EXPECT_THAT(mesh.groups[0].indices, ElementsAre(3, 4, 5, 3, 5, 6, 3, 6, 7));
    EXPECT_EQ(mesh.groups[1].texture, 1);
    EXPECT_TRUE(mesh.groups[1].is_liquid);
    EXPECT_THAT(mesh.groups[1].indices, ElementsAre(0, 1, 2, 8, 9, 10));
  }

  TEST(BspMeshTest, MakesTheMeshOfOneModelOfItsFacesAlone)
  {
    BspFile file = MakeQuad();
    file.models.emplace_back();
    file.models[1].first_face = 1;
    AddFace(file, {{0.0f, 0.0f, 64.0f}, {0.0f, 16.0f, 64.0f}, {16.0f, 0.0f, 64.0f}});

    BspMesh world;
    BspMesh door;
    std::string error;
    ASSERT_TRUE(world.Build(file, 0, error)) << error;
    ASSERT_TRUE(door.Build(file, 1, error)) << error;

    ASSERT_EQ(world.faces.size(), 1u);
    EXPECT_EQ(world.faces[0].face, 0u);
    EXPECT_EQ(world.vertices.size(), 4u);

    ASSERT_EQ(door.faces.size(), 1u);
    EXPECT_EQ(door.faces[0].face, 1u);
    EXPECT_EQ(door.faces[0].first_vertex, 0u);
    ASSERT_EQ(door.vertices.size(), 3u);
    EXPECT_EQ(door.vertices[0].position.z, 64.0f);
    EXPECT_THAT(door.groups[0].indices, ElementsAre(0, 1, 2));
  }

  TEST(BspMeshTest, MakesAnEmptyMeshOfAModelWithoutFaces)
  {
    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(MakeEmpty(), 0, error)) << error;
    EXPECT_THAT(mesh.vertices, IsEmpty());
    EXPECT_THAT(mesh.groups, IsEmpty());
    EXPECT_THAT(mesh.faces, IsEmpty());
  }

  TEST(BspMeshTest, RefusesAModelThatIsNotThereAndStaysAsItWas)
  {
    const BspFile file = MakeQuad();

    BspMesh mesh;
    std::string error;
    ASSERT_TRUE(mesh.Build(file, 0, error)) << error;

    EXPECT_FALSE(mesh.Build(file, 1, error));
    EXPECT_EQ(error, "model 1 was asked for, and there are 1");
    EXPECT_EQ(mesh.vertices.size(), 4u);
    EXPECT_EQ(mesh.faces.size(), 1u);
  }

  TEST(BspMeshTest, RefusesAModelWhoseFacesAreNotThere)
  {
    BspFile many = MakeQuad();
    many.models[0].face_count = 2;
    EXPECT_EQ(ReasonOfRefusal(many), "model 0 names 2 faces from 0, and there are 1");

    BspFile negative = MakeQuad();
    negative.models[0].first_face = -1;
    EXPECT_EQ(ReasonOfRefusal(negative), "model 0 names 1 faces from -1, and there are 1");
  }

  TEST(BspMeshTest, RefusesAFaceThatNamesWhatIsNotThere)
  {
    BspFile plane = MakeQuad();
    plane.faces[0].plane = 1;
    EXPECT_EQ(ReasonOfRefusal(plane), "face 0 names a plane that is not there");

    BspFile info = MakeQuad();
    info.faces[0].texture_info = 1;
    EXPECT_EQ(ReasonOfRefusal(info), "face 0 names a texture info that is not there");

    BspFile texture = MakeQuad();
    texture.texture_infos[0].texture = 1;
    EXPECT_EQ(ReasonOfRefusal(texture), "face 0 names a texture that is not there");

    BspFile no_texture = MakeQuad();
    no_texture.texture_infos[0].texture = -1;
    EXPECT_EQ(ReasonOfRefusal(no_texture), "face 0 names a texture that is not there");

    BspFile face_edges = MakeQuad();
    face_edges.faces[0].edge_count = 5;
    EXPECT_EQ(ReasonOfRefusal(face_edges), "face 0 names edges of faces that are not there");

    BspFile first_edge = MakeQuad();
    first_edge.faces[0].first_edge = -1;
    EXPECT_EQ(ReasonOfRefusal(first_edge), "face 0 names edges of faces that are not there");

    BspFile edge = MakeQuad();
    edge.face_edges[2] = 5;
    EXPECT_EQ(ReasonOfRefusal(edge), "face 0 names an edge that is not there");

    BspFile backwards = MakeQuad();
    backwards.face_edges[1] = std::numeric_limits<std::int32_t>::min();
    EXPECT_EQ(ReasonOfRefusal(backwards), "face 0 names an edge that is not there");

    BspFile vertex = MakeQuad();
    vertex.edges[3].vertices[0] = 4;
    EXPECT_EQ(ReasonOfRefusal(vertex), "face 0 names a vertex that is not there");
  }

  TEST(BspMeshTest, RefusesAFaceWithFewerThanThreeCorners)
  {
    BspFile file = MakeQuad();
    file.faces[0].edge_count = 2;
    EXPECT_EQ(ReasonOfRefusal(file), "face 0 has 2 corners, fewer than three");
  }

  TEST(BspMeshTest, RefusesALightmapThatDoesNotLieInsideTheLighting)
  {
    // one byte short of the 32 samples
    BspFile cut = MakeQuad();
    cut.lighting.pop_back();
    EXPECT_EQ(ReasonOfRefusal(cut),
      "face 0 has 1 lightmaps of 8 by 4 samples from byte 4 of the lighting, which has 35");

    // a second style, for which there are no samples
    BspFile styles = MakeQuad();
    styles.faces[0].styles = {0, 1, 255, 255};
    EXPECT_THAT(ReasonOfRefusal(styles), HasSubstr("face 0 has 2 lightmaps of 8 by 4 samples"));

    BspFile far = MakeQuad();
    far.faces[0].light_offset = 1000;
    EXPECT_THAT(ReasonOfRefusal(far), HasSubstr("from byte 1000 of the lighting"));

    BspFile before = MakeQuad();
    before.faces[0].light_offset = -2;
    EXPECT_THAT(ReasonOfRefusal(before), HasSubstr("from byte -2 of the lighting"));
  }

  TEST(BspMeshTest, RefusesACornerThatLiesNowhereOnItsTexture)
  {
    BspFile endless = MakeQuad();
    endless.texture_infos[0].s_offset = std::numeric_limits<float>::infinity();
    EXPECT_EQ(ReasonOfRefusal(endless), "face 0 has a corner that lies nowhere on its texture");

    BspFile no_number = MakeQuad();
    no_number.vertices[2].y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(ReasonOfRefusal(no_number), "face 0 has a corner that lies nowhere on its texture");

    BspFile huge = MakeQuad();
    huge.texture_infos[0].t_axis.y = 1.0e30f;
    EXPECT_EQ(ReasonOfRefusal(huge), "face 0 has a corner that lies nowhere on its texture");
  }

  /// Makes the mesh of every model of a real level, and looks at what must
  /// hold for each.
  void CheckTheMeshesOf(const BspFile &file, const std::string &name)
  {
    for (std::size_t model = 0; model < file.models.size(); model++)
    {
      BspMesh mesh;
      std::string error;
      ASSERT_TRUE(mesh.Build(file, model, error)) << name << ", model " << model << ": " << error;
      EXPECT_EQ(mesh.faces.size(), static_cast<std::size_t>(file.models[model].face_count));

      // every corner of a lit face lies within its lightmap
      std::size_t outside = 0;
      for (const BspMeshFace &face : mesh.faces)
      {
        if (face.lightmap_width == 0) { continue; }
        for (std::uint32_t i = 0; i < face.vertex_count; i++)
        {
          const BspMeshVertex &vertex = mesh.vertices[face.first_vertex + i];
          if (!(vertex.lightmap_u >= 0.0f && vertex.lightmap_u <= 1.0f &&
            vertex.lightmap_v >= 0.0f && vertex.lightmap_v <= 1.0f))
          {
            outside++;
          }
        }
      }
      EXPECT_EQ(outside, 0u) << name << ", model " << model;
    }
  }

  TEST(BspMeshTest, MakesTheMeshesOfRealLevelsWhenTheyAreThere)
  {
    const std::filesystem::path maps = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "maps";

    std::size_t made = 0;
    std::error_code ignored;
    for (std::filesystem::directory_iterator entry(maps, ignored), end; !ignored && entry != end;
      entry.increment(ignored))
    {
      if (entry->path().extension() != ".bsp") { continue; }

      std::ifstream stream(entry->path(), std::ios::binary);
      const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

      BspFile file;
      std::string error;
      ASSERT_TRUE(file.Read(bytes, error)) << entry->path().string() << ": " << error;
      CheckTheMeshesOf(file, entry->path().string());
      made++;
    }
    if (made == 0) { GTEST_SKIP() << "No compiled level in " << maps.string(); }
  }

  TEST(BspMeshTest, MakesTheMeshesOfEveryLevelOfThePaksWhateverItsForm)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    std::size_t wide = 0;
    for (const std::string &name : data.ListNames("maps"))
    {
      if (!name.ends_with(".bsp")) { continue; }

      BspFile file;
      std::string error;
      ASSERT_TRUE(file.Read(data.GetBytes(name), error)) << name << ": " << error;
      CheckTheMeshesOf(file, name);
      if (file.format != BspFormat::Version29) { wide++; }
    }

    // the one level that is too large for the original's form
    EXPECT_GT(wide, 0u);
  }
}
