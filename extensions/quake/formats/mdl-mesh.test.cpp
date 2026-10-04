#include "mdl-mesh.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mdl-test-model.hpp"

namespace
{
  using quake::MdlFile;
  using quake::MdlMesh;
  using quake::MdlMeshVertex;
  using quake::MdlTestModel;
  using quake::MdlVector;
  using ::testing::ElementsAre;

  MdlFile ReadModel(const MdlTestModel &model)
  {
    MdlFile file;
    std::string error;
    EXPECT_TRUE(file.Read(model.ToBytes(), error)) << error;
    return file;
  }

  void ExpectVector(const MdlVector &vector, const float x, const float y, const float z)
  {
    EXPECT_NEAR(vector.x, x, 1e-5f);
    EXPECT_NEAR(vector.y, y, 1e-5f);
    EXPECT_NEAR(vector.z, z, 1e-5f);
  }

  TEST(MdlMeshTest, HasTheVerticesOfTheFileAndASecondOneForTheSeam)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);

    // the corner at the origin is on the seam and a triangle of the back
    // uses it, so it is there twice
    EXPECT_EQ(mesh.GetVertexCount(), 5u);
    EXPECT_THAT(mesh.GetFileVertices(), ElementsAre(0u, 1u, 2u, 3u, 0u));

    // only the triangle of the back takes the second one
    EXPECT_THAT(mesh.GetIndices(), ElementsAre(0u, 1u, 2u, 0u, 3u, 1u, 4u, 2u, 3u, 1u, 3u, 2u));
  }

  TEST(MdlMeshTest, AddsHalfTheWidthOfTheSkinForAVertexOfTheSeamOnTheBack)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);
    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(0));
    ASSERT_EQ(vertices.size(), 5u);

    // the skin is 8 by 4, and a place is the middle of its pixel
    EXPECT_FLOAT_EQ(vertices[0].u, 3.5f / 8.0f);
    EXPECT_FLOAT_EQ(vertices[0].v, 0.5f / 4.0f);
    EXPECT_FLOAT_EQ(vertices[4].u, 7.5f / 8.0f);
    EXPECT_FLOAT_EQ(vertices[4].v, 0.5f / 4.0f);

    EXPECT_FLOAT_EQ(vertices[1].u, 0.5f / 8.0f);
    EXPECT_FLOAT_EQ(vertices[1].v, 3.5f / 4.0f);
    EXPECT_FLOAT_EQ(vertices[3].u, 1.5f / 8.0f);
    EXPECT_FLOAT_EQ(vertices[3].v, 1.5f / 4.0f);
  }

  TEST(MdlMeshTest, LeavesAVertexOfTheSeamAloneWhenNoTriangleOfTheBackUsesIt)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.triangles[2].faces_front = true;
    const MdlFile file = ReadModel(model);
    const MdlMesh mesh(file);

    EXPECT_EQ(mesh.GetVertexCount(), 4u);
    EXPECT_THAT(mesh.GetIndices(), ElementsAre(0u, 1u, 2u, 0u, 3u, 1u, 0u, 2u, 3u, 1u, 3u, 2u));
  }

  TEST(MdlMeshTest, LeavesAVertexOfTheBackAloneWhenItIsNotOnTheSeam)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.texture_coordinates[0].on_seam = false;
    const MdlFile file = ReadModel(model);
    const MdlMesh mesh(file);

    EXPECT_EQ(mesh.GetVertexCount(), 4u);
    EXPECT_FLOAT_EQ(mesh.MakeVertices(*file.FindPose(0))[0].u, 3.5f / 8.0f);
  }

  TEST(MdlMeshTest, MakesTheSecondVertexOfTheSeamOnceForAllTrianglesOfTheBack)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.triangles[1].faces_front = false;
    const MdlFile file = ReadModel(model);
    const MdlMesh mesh(file);

    EXPECT_EQ(mesh.GetVertexCount(), 5u);
    EXPECT_THAT(mesh.GetIndices(), ElementsAre(0u, 1u, 2u, 4u, 3u, 1u, 4u, 2u, 3u, 1u, 3u, 2u));
  }

  TEST(MdlMeshTest, UnpacksThePlacesWithTheScaleAndTheTranslateOfTheHeader)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.header.scale = {0.5f, 2.0f, 0.25f};
    model.header.translate = {-1.0f, -2.0f, 3.0f};
    const MdlFile file = ReadModel(model);
    const MdlMesh mesh(file);

    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(1));
    ASSERT_EQ(vertices.size(), 5u);
    ExpectVector(vertices[0].position, -1.0f, -2.0f, 3.0f);
    ExpectVector(vertices[1].position, 4.0f, -2.0f, 3.0f);
    ExpectVector(vertices[2].position, -1.0f, 18.0f, 3.0f);
    // z stays up: the top of the second frame is 20 up, a quarter of it
    ExpectVector(vertices[3].position, -1.0f, -2.0f, 8.0f);
    // the second one of the seam is at the same place
    ExpectVector(vertices[4].position, -1.0f, -2.0f, 3.0f);
  }

  TEST(MdlMeshTest, ComputesNormalsThatPointOutsideFromTheTriangles)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);
    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(0));
    ASSERT_EQ(vertices.size(), 5u);

    // The three triangles at the origin face down the three axes, and are
    // as large as one another.
    const float third = 1.0f / std::sqrt(3.0f);
    ExpectVector(vertices[0].normal, -third, -third, -third);

    // At the top, the two upright triangles and the slanted one, which is
    // larger by as much as it takes to leave straight up.
    ExpectVector(vertices[3].normal, 0.0f, 0.0f, 1.0f);
    ExpectVector(vertices[1].normal, 1.0f, 0.0f, 0.0f);
    ExpectVector(vertices[2].normal, 0.0f, 1.0f, 0.0f);

    // the two of the seam have the same, so that no edge shows there
    ExpectVector(vertices[4].normal, -third, -third, -third);
  }

  TEST(MdlMeshTest, HasTrianglesThatGoAroundClockwiseSeenFromOutside)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);
    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(0));
    const auto &indices = mesh.GetIndices();

    // The middle of the tetrahedron is inside it. For corners that go
    // around counter-clockwise seen from outside, the cross product of the
    // first two edges points outside; here it points inside for every one.
    const MdlVector middle = {2.5f, 2.5f, 2.5f};
    for (std::size_t i = 0; i < indices.size(); i += 3)
    {
      const MdlVector &a = vertices[indices[i]].position;
      const MdlVector &b = vertices[indices[i + 1]].position;
      const MdlVector &c = vertices[indices[i + 2]].position;

      const MdlVector ab = {b.x - a.x, b.y - a.y, b.z - a.z};
      const MdlVector ac = {c.x - a.x, c.y - a.y, c.z - a.z};
      const MdlVector counter_clockwise = {
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x,
      };
      const MdlVector outwards = {a.x - middle.x, a.y - middle.y, a.z - middle.z};

      const float facing =
        counter_clockwise.x * outwards.x + counter_clockwise.y * outwards.y + counter_clockwise.z * outwards.z;
      EXPECT_LT(facing, 0.0f) << "triangle " << i / 3;
    }
  }

  TEST(MdlMeshTest, ComputesTheNormalsOfEachPoseFromItsOwnPlaces)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);

    // In the second frame the top is twice as far up, which tilts the
    // slanted triangle and makes the upright ones larger.
    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(1));
    ASSERT_EQ(vertices.size(), 5u);

    // slanted (200, 200, 100), upright (-200, 0, 0) and (0, -200, 0)
    ExpectVector(vertices[3].normal, 0.0f, 0.0f, 1.0f);
    // the same and the one below, (0, 0, -100): (200, 0, 0) is left
    ExpectVector(vertices[1].normal, 1.0f, 0.0f, 0.0f);
    // below and the two upright ones: (-200, -200, -100)
    ExpectVector(vertices[0].normal, -2.0f / 3.0f, -2.0f / 3.0f, -1.0f / 3.0f);
  }

  TEST(MdlMeshTest, KeepsTheNumberOfTheNormalOfTheFile)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);

    const std::vector<MdlMeshVertex> first = mesh.MakeVertices(*file.FindPose(0));
    EXPECT_EQ(first[0].normal_index, 0);
    EXPECT_EQ(first[3].normal_index, 3);
    EXPECT_EQ(first[4].normal_index, 0);

    const std::vector<MdlMeshVertex> second = mesh.MakeVertices(*file.FindPose(1));
    EXPECT_EQ(second[0].normal_index, 100);
    EXPECT_EQ(second[3].normal_index, 103);
    EXPECT_EQ(second[4].normal_index, 100);
  }

  TEST(MdlMeshTest, MakesTheVerticesOfAPoseOfAGroup)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeWithGroups());
    const MdlMesh mesh(file);

    const std::vector<MdlMeshVertex> first = mesh.MakeVertices(*file.FindPose(2, 0));
    const std::vector<MdlMeshVertex> second = mesh.MakeVertices(*file.FindPose(2, 1));
    ASSERT_EQ(first.size(), mesh.GetVertexCount());
    ASSERT_EQ(second.size(), mesh.GetVertexCount());
    ExpectVector(first[3].position, 0.0f, 0.0f, 30.0f);
    ExpectVector(second[3].position, 0.0f, 0.0f, 40.0f);

    // where on the skin a vertex is does not change with the pose
    for (std::size_t i = 0; i < first.size(); i++)
    {
      EXPECT_EQ(first[i].u, second[i].u);
      EXPECT_EQ(first[i].v, second[i].v);
    }
  }

  TEST(MdlMeshTest, GivesAVertexWithoutATriangleTheNormalStraightUp)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    // every triangle is the one below, so nothing uses the top
    for (quake::MdlTriangle &triangle : model.triangles)
    {
      triangle.vertices = {0, 1, 2};
    }
    // and one has no area at all
    model.triangles[3].vertices = {1, 1, 1};

    const MdlFile file = ReadModel(model);
    const MdlMesh mesh(file);
    const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(*file.FindPose(0));
    ASSERT_GE(vertices.size(), 4u);
    ExpectVector(vertices[3].normal, 0.0f, 0.0f, 1.0f);
    ExpectVector(vertices[1].normal, 0.0f, 0.0f, -1.0f);
  }

  TEST(MdlMeshTest, MakesNothingOfAPoseOfAnotherModel)
  {
    const MdlFile file = ReadModel(MdlTestModel::MakeTetrahedron());
    const MdlMesh mesh(file);

    quake::MdlPose pose = *file.FindPose(0);
    pose.vertices.pop_back();
    EXPECT_TRUE(mesh.MakeVertices(pose).empty());
  }

  TEST(MdlMeshTest, IsEmptyWhenMadeOfNothing)
  {
    const MdlMesh mesh;
    EXPECT_EQ(mesh.GetVertexCount(), 0u);
    EXPECT_TRUE(mesh.GetIndices().empty());
    EXPECT_TRUE(mesh.MakeVertices(quake::MdlPose()).empty());

    const MdlMesh of_no_model{MdlFile()};
    EXPECT_EQ(of_no_model.GetVertexCount(), 0u);
    EXPECT_TRUE(of_no_model.GetIndices().empty());
  }

  TEST(MdlMeshTest, MakesAMeshOfEveryPoseOfEveryModelOfRealData)
  {
    const std::filesystem::path folder = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "progs";
    std::error_code ignored;
    std::vector<std::filesystem::path> paths;
    if (std::filesystem::is_directory(folder, ignored))
    {
      for (const auto &entry : std::filesystem::directory_iterator(folder, ignored))
      {
        if (entry.path().extension() == ".mdl") { paths.push_back(entry.path()); }
      }
    }
    if (paths.empty())
    {
      GTEST_SKIP() << "No models in " << QUAKE_TEST_DATA_DIRECTORY << "/progs";
    }

    for (const std::filesystem::path &path : paths)
    {
      SCOPED_TRACE(path.filename().string());

      std::ifstream stream(path, std::ios::binary);
      const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};

      MdlFile file;
      std::string error;
      ASSERT_TRUE(file.Read(bytes, error)) << error;

      const MdlMesh mesh(file);
      const auto file_vertex_count = static_cast<std::size_t>(file.GetHeader().vertex_count);
      ASSERT_GE(mesh.GetVertexCount(), file_vertex_count);
      ASSERT_LE(mesh.GetVertexCount(), 2 * file_vertex_count);
      ASSERT_EQ(mesh.GetIndices().size(), file.GetTriangles().size() * 3);
      ASSERT_EQ(mesh.GetFileVertices().size(), mesh.GetVertexCount());

      bool indices_in_range = true;
      for (const std::uint32_t index : mesh.GetIndices())
      {
        indices_in_range = indices_in_range && index < mesh.GetVertexCount();
      }
      EXPECT_TRUE(indices_in_range);

      bool file_vertices_in_range = true;
      for (const std::uint32_t file_vertex : mesh.GetFileVertices())
      {
        file_vertices_in_range = file_vertices_in_range && file_vertex < file_vertex_count;
      }
      EXPECT_TRUE(file_vertices_in_range);

      for (const quake::MdlFrame &frame : file.GetFrames())
      {
        for (const quake::MdlPose &pose : frame.poses)
        {
          const std::vector<MdlMeshVertex> vertices = mesh.MakeVertices(pose);
          ASSERT_EQ(vertices.size(), mesh.GetVertexCount());

          bool sound = true;
          for (const MdlMeshVertex &vertex : vertices)
          {
            const MdlVector &normal = vertex.normal;
            const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            sound = sound && std::abs(length - 1.0f) < 1e-3f &&
              std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
              std::isfinite(vertex.position.z) &&
              vertex.u >= 0.0f && vertex.u <= 1.0f && vertex.v >= 0.0f && vertex.v <= 1.0f;
          }
          EXPECT_TRUE(sound) << "pose " << pose.name;
        }
      }
    }
  }
}
