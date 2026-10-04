#include "mdl-file.hpp"

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
  using quake::MdlTestModel;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// What reading the bytes says is wrong with them, and nothing when they
  /// are read.
  std::string ErrorOf(const std::vector<std::uint8_t> &bytes)
  {
    MdlFile file;
    std::string error;
    if (file.Read(bytes, error)) { return ""; }

    EXPECT_FALSE(error.empty());
    return error;
  }

  /// The bytes of every model of the data to test with, by the name of its
  /// file. Empty when the data is not on this machine.
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> ReadRealModels()
  {
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> models;

    const std::filesystem::path folder = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "progs";
    std::error_code ignored;
    if (!std::filesystem::is_directory(folder, ignored)) { return models; }

    for (const auto &entry : std::filesystem::directory_iterator(folder, ignored))
    {
      if (entry.path().extension() != ".mdl") { continue; }

      std::ifstream stream(entry.path(), std::ios::binary);
      std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
      models.emplace_back(entry.path().filename().string(), std::move(bytes));
    }
    return models;
  }

  TEST(MdlFileTest, ReadsTheHeader)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.header.scale = {0.5f, 1.0f, 2.0f};
    model.header.translate = {-1.0f, -2.0f, -3.0f};

    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(model.ToBytes(), error)) << error;
    EXPECT_TRUE(error.empty());

    const quake::MdlHeader &header = file.GetHeader();
    EXPECT_EQ(header.scale.x, 0.5f);
    EXPECT_EQ(header.scale.y, 1.0f);
    EXPECT_EQ(header.scale.z, 2.0f);
    EXPECT_EQ(header.translate.x, -1.0f);
    EXPECT_EQ(header.translate.y, -2.0f);
    EXPECT_EQ(header.translate.z, -3.0f);
    EXPECT_EQ(header.bounding_radius, 20.0f);
    EXPECT_EQ(header.eye_position.x, 1.0f);
    EXPECT_EQ(header.eye_position.y, 2.0f);
    EXPECT_EQ(header.eye_position.z, 3.0f);
    EXPECT_EQ(header.skin_count, 1);
    EXPECT_EQ(header.skin_width, 8);
    EXPECT_EQ(header.skin_height, 4);
    EXPECT_EQ(header.vertex_count, 4);
    EXPECT_EQ(header.triangle_count, 4);
    EXPECT_EQ(header.frame_count, 2);
    EXPECT_EQ(header.sync_type, 1);
    EXPECT_EQ(header.flags, 8u);
    EXPECT_EQ(header.size, 2.5f);
  }

  TEST(MdlFileTest, ReadsASkinAsTheNumbersOfItsColours)
  {
    const MdlTestModel model = MdlTestModel::MakeTetrahedron();
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(model.ToBytes(), error)) << error;

    ASSERT_EQ(file.GetSkins().size(), 1u);
    const quake::MdlSkin &skin = file.GetSkins()[0];
    EXPECT_FALSE(skin.is_group);
    EXPECT_TRUE(skin.times.empty());
    ASSERT_EQ(skin.pictures.size(), 1u);
    EXPECT_EQ(skin.pictures[0], model.MakePicture(0));
    EXPECT_EQ(skin.pictures[0].size(), 32u);
  }

  TEST(MdlFileTest, ReadsTheTextureCoordinatesAndTheTriangles)
  {
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MdlTestModel::MakeTetrahedron().ToBytes(), error)) << error;

    const auto &coordinates = file.GetTextureCoordinates();
    ASSERT_EQ(coordinates.size(), 4u);
    EXPECT_TRUE(coordinates[0].on_seam);
    EXPECT_EQ(coordinates[0].s, 3);
    EXPECT_EQ(coordinates[0].t, 0);
    EXPECT_FALSE(coordinates[2].on_seam);
    EXPECT_EQ(coordinates[2].s, 2);
    EXPECT_EQ(coordinates[2].t, 3);

    const auto &triangles = file.GetTriangles();
    ASSERT_EQ(triangles.size(), 4u);
    EXPECT_TRUE(triangles[0].faces_front);
    EXPECT_THAT(triangles[0].vertices, ElementsAre(0, 1, 2));
    EXPECT_FALSE(triangles[2].faces_front);
    EXPECT_THAT(triangles[2].vertices, ElementsAre(0, 2, 3));
    EXPECT_THAT(triangles[3].vertices, ElementsAre(1, 3, 2));
  }

  TEST(MdlFileTest, ReadsTheFramesWithAPlaceForEveryVertex)
  {
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MdlTestModel::MakeTetrahedron().ToBytes(), error)) << error;

    ASSERT_EQ(file.GetFrames().size(), 2u);
    const quake::MdlFrame &frame = file.GetFrames()[1];
    EXPECT_FALSE(frame.is_group);
    EXPECT_TRUE(frame.times.empty());
    EXPECT_THAT(frame.minimum.position, ElementsAre(0, 0, 0));
    EXPECT_THAT(frame.maximum.position, ElementsAre(10, 10, 20));
    ASSERT_EQ(frame.poses.size(), 1u);

    const quake::MdlPose &pose = frame.poses[0];
    EXPECT_EQ(pose.name, "stand2");
    EXPECT_THAT(pose.maximum.position, ElementsAre(10, 10, 20));
    ASSERT_EQ(pose.vertices.size(), 4u);
    EXPECT_THAT(pose.vertices[1].position, ElementsAre(10, 0, 0));
    EXPECT_EQ(pose.vertices[1].normal_index, 101);
    EXPECT_THAT(pose.vertices[3].position, ElementsAre(0, 0, 20));
    EXPECT_EQ(pose.vertices[3].normal_index, 103);

    EXPECT_EQ(file.GetFrames()[0].poses[0].name, "stand1");
  }

  TEST(MdlFileTest, ReadsAGroupOfSkinsWithItsTimes)
  {
    const MdlTestModel model = MdlTestModel::MakeWithGroups();
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(model.ToBytes(), error)) << error;

    ASSERT_EQ(file.GetSkins().size(), 2u);
    EXPECT_FALSE(file.GetSkins()[0].is_group);

    const quake::MdlSkin &skin = file.GetSkins()[1];
    EXPECT_TRUE(skin.is_group);
    EXPECT_THAT(skin.times, ElementsAre(0.25f, 0.5f));
    ASSERT_EQ(skin.pictures.size(), 2u);
    EXPECT_EQ(skin.pictures[0], model.MakePicture(50));
    EXPECT_EQ(skin.pictures[1], model.MakePicture(100));
  }

  TEST(MdlFileTest, ReadsAGroupOfFramesWithItsTimes)
  {
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MdlTestModel::MakeWithGroups().ToBytes(), error)) << error;

    ASSERT_EQ(file.GetFrames().size(), 3u);
    const quake::MdlFrame &frame = file.GetFrames()[2];
    EXPECT_TRUE(frame.is_group);
    EXPECT_THAT(frame.times, ElementsAre(0.1f, 0.2f));
    EXPECT_THAT(frame.maximum.position, ElementsAre(10, 10, 40));
    ASSERT_EQ(frame.poses.size(), 2u);
    EXPECT_EQ(frame.poses[0].name, "flame1");
    EXPECT_EQ(frame.poses[1].name, "flame2");
    EXPECT_THAT(frame.poses[0].vertices[3].position, ElementsAre(0, 0, 30));
    EXPECT_THAT(frame.poses[1].vertices[3].position, ElementsAre(0, 0, 40));
    EXPECT_EQ(frame.poses[1].vertices[3].normal_index, 23);
  }

  TEST(MdlFileTest, ReadsAGroupOfOne)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.skins[0].is_group = true;
    model.skins[0].times = {0.1f};
    model.frames[0].is_group = true;
    model.frames[0].times = {0.1f};

    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(model.ToBytes(), error)) << error;
    EXPECT_TRUE(file.GetSkins()[0].is_group);
    EXPECT_EQ(file.GetSkins()[0].pictures.size(), 1u);
    EXPECT_TRUE(file.GetFrames()[0].is_group);
    EXPECT_EQ(file.GetFrames()[0].poses.size(), 1u);
    EXPECT_EQ(file.GetFrames()[1].poses[0].name, "stand2");
  }

  TEST(MdlFileTest, FindsAPoseByItsFrameAndItsPlaceInTheGroup)
  {
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MdlTestModel::MakeWithGroups().ToBytes(), error)) << error;

    ASSERT_NE(file.FindPose(0), nullptr);
    EXPECT_EQ(file.FindPose(0)->name, "stand1");
    ASSERT_NE(file.FindPose(2, 1), nullptr);
    EXPECT_EQ(file.FindPose(2, 1)->name, "flame2");

    EXPECT_EQ(file.FindPose(3), nullptr);
    EXPECT_EQ(file.FindPose(0, 1), nullptr);
    EXPECT_EQ(file.FindPose(2, 2), nullptr);
    EXPECT_EQ(MdlFile().FindPose(0), nullptr);
  }

  TEST(MdlFileTest, LeavesAloneWhatFollowsTheLastFrame)
  {
    std::vector<std::uint8_t> bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    bytes.insert(bytes.end(), {1, 2, 3});
    EXPECT_EQ(ErrorOf(bytes), "");
  }

  TEST(MdlFileTest, RefusesAnotherMagic)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    // IDSP, which a sprite starts with
    model.magic = 0x50534449;
    EXPECT_THAT(ErrorOf(model.ToBytes()), HasSubstr("IDPO"));
  }

  TEST(MdlFileTest, RefusesAnotherVersion)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.version = 5;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "The version is 5, not 6");
  }

  TEST(MdlFileTest, RefusesNoBytesAndAHeaderCutShort)
  {
    EXPECT_EQ(ErrorOf({}), "The file ends before its header");

    std::vector<std::uint8_t> bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    bytes.resize(MdlTestModel::header_size - 1);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before its header");
  }

  TEST(MdlFileTest, RefusesACountThatIsNothingNegativeOrPastTheMost)
  {
    struct Count
    {
      std::size_t at;
      std::int32_t most;
      std::string what;
    };
    const std::vector<Count> counts = {
      {MdlTestModel::skin_count_at, MdlFile::most_skins, "number of skins"},
      {MdlTestModel::skin_width_at, MdlFile::most_skin_side, "width of the skins"},
      {MdlTestModel::skin_height_at, MdlFile::most_skin_side, "height of the skins"},
      {MdlTestModel::vertex_count_at, MdlFile::most_vertices, "number of vertices"},
      {MdlTestModel::triangle_count_at, MdlFile::most_triangles, "number of triangles"},
      {MdlTestModel::frame_count_at, MdlFile::most_frames, "number of frames"},
    };

    for (const Count &count : counts)
    {
      for (const std::int32_t value : {0, -1, INT32_MIN, count.most + 1, INT32_MAX})
      {
        std::vector<std::uint8_t> bytes = MdlTestModel::MakeTetrahedron().ToBytes();
        MdlTestModel::Overwrite(bytes, count.at, value);

        const std::string expected = "The " + count.what + ", " + std::to_string(value) +
          ", is not between 1 and " + std::to_string(count.most);
        EXPECT_EQ(ErrorOf(bytes), expected);
      }
    }
  }

  TEST(MdlFileTest, RefusesACountTheBytesDoNotHold)
  {
    // Each is allowed by itself, and far more than the file has. With more
    // skins than there are, what follows the skins is taken for skins, and
    // what is wrong with those is what is said.
    std::vector<std::uint8_t> bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::skin_count_at, MdlFile::most_skins);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("skin"));

    bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::skin_width_at, MdlFile::most_skin_side);
    MdlTestModel::Overwrite(bytes, MdlTestModel::skin_height_at, MdlFile::most_skin_side);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of skin 0");

    bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::vertex_count_at, MdlFile::most_vertices);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of its texture coordinates");

    bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::triangle_count_at, MdlFile::most_triangles);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of its triangles");

    bytes = MdlTestModel::MakeTetrahedron().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::frame_count_at, 3);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of frame 2");
  }

  TEST(MdlFileTest, RefusesAFileCutShortAnywhere)
  {
    const std::vector<std::uint8_t> whole = MdlTestModel::MakeWithGroups().ToBytes();
    ASSERT_EQ(ErrorOf(whole), "");

    for (std::size_t size = 0; size < whole.size(); size++)
    {
      // a copy of its own size, so that a tool that watches memory sees a
      // read past its end
      const std::vector<std::uint8_t> cut(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size));
      EXPECT_THAT(ErrorOf(cut), HasSubstr("The file ends before")) << "with " << size << " bytes";
    }
  }

  TEST(MdlFileTest, RefusesAGroupOfSkinsWithACountThatCannotBe)
  {
    for (const std::int32_t value : {0, -1, MdlFile::most_in_group + 1})
    {
      std::vector<std::uint8_t> bytes = MdlTestModel::MakeWithGroups().ToBytes();
      // after the header: the first skin with its kind, then the kind of
      // the second, then its count
      MdlTestModel::Overwrite(bytes, MdlTestModel::header_size + 4 + 32 + 4, value);
      EXPECT_THAT(ErrorOf(bytes), HasSubstr("The number of pictures of skin 1, " + std::to_string(value)));
    }

    std::vector<std::uint8_t> bytes = MdlTestModel::MakeWithGroups().ToBytes();
    MdlTestModel::Overwrite(bytes, MdlTestModel::header_size + 4 + 32 + 4, MdlFile::most_in_group);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of skin 1");
  }

  TEST(MdlFileTest, RefusesAGroupOfFramesWithACountThatCannotBe)
  {
    const MdlTestModel model = MdlTestModel::MakeWithGroups();
    const std::vector<std::uint8_t> whole = model.ToBytes();

    // the group is the last frame: its kind, its count, its box, two times,
    // and two poses of a box, a name, and four vertices each
    const std::size_t count_at = whole.size() - 2 * (8 + 16 + 16) - 8 - 8 - 4;

    for (const std::int32_t value : {0, -1, MdlFile::most_in_group + 1})
    {
      std::vector<std::uint8_t> bytes = whole;
      MdlTestModel::Overwrite(bytes, count_at, value);
      EXPECT_THAT(ErrorOf(bytes), HasSubstr("The number of poses of frame 2, " + std::to_string(value)));
    }

    std::vector<std::uint8_t> bytes = whole;
    MdlTestModel::Overwrite(bytes, count_at, MdlFile::most_in_group);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of frame 2");

    bytes = whole;
    MdlTestModel::Overwrite(bytes, count_at, 3);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("The file ends before the end of"));
  }

  TEST(MdlFileTest, RefusesATriangleThatNamesAVertexThatIsNotThere)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.triangles[1].vertices[2] = 4;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "Triangle 1 names vertex 4 of 4");

    model.triangles[1].vertices[2] = -1;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "Triangle 1 names vertex -1 of 4");

    model.triangles[1].vertices[2] = 3;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "");
  }

  TEST(MdlFileTest, RefusesAVertexThatNamesANormalThatIsNotThere)
  {
    MdlTestModel model = MdlTestModel::MakeTetrahedron();
    model.frames[1].poses[0].vertices[2].normal_index = 162;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "Vertex 2 of frame 1 names normal 162 of 162");

    model.frames[1].poses[0].vertices[2].normal_index = 161;
    EXPECT_EQ(ErrorOf(model.ToBytes()), "");

    MdlTestModel grouped = MdlTestModel::MakeWithGroups();
    grouped.frames[2].poses[1].vertices[0].normal_index = 255;
    EXPECT_EQ(ErrorOf(grouped.ToBytes()), "Vertex 0 of pose 1 of frame 2 names normal 255 of 162");
  }

  TEST(MdlFileTest, StaysAsItWasWhenTheBytesAreRefused)
  {
    MdlFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MdlTestModel::MakeWithGroups().ToBytes(), error)) << error;

    MdlTestModel wrong = MdlTestModel::MakeTetrahedron();
    wrong.triangles[3].vertices[0] = 9;
    EXPECT_FALSE(file.Read(wrong.ToBytes(), error));
    EXPECT_FALSE(error.empty());

    EXPECT_EQ(file.GetHeader().frame_count, 3);
    EXPECT_EQ(file.GetFrames().size(), 3u);
    EXPECT_EQ(file.GetSkins().size(), 2u);
    EXPECT_EQ(file.GetTriangles().size(), 4u);
    EXPECT_EQ(file.GetTextureCoordinates().size(), 4u);
  }

  TEST(MdlFileTest, ReadsEveryModelOfRealData)
  {
    const auto models = ReadRealModels();
    if (models.empty())
    {
      GTEST_SKIP() << "No models in " << QUAKE_TEST_DATA_DIRECTORY << "/progs";
    }

    for (const auto &[name, bytes] : models)
    {
      SCOPED_TRACE(name);

      MdlFile file;
      std::string error;
      ASSERT_TRUE(file.Read(bytes, error)) << error;

      const quake::MdlHeader &header = file.GetHeader();
      const auto vertex_count = static_cast<std::size_t>(header.vertex_count);
      const std::size_t picture_size =
        static_cast<std::size_t>(header.skin_width) * static_cast<std::size_t>(header.skin_height);

      ASSERT_EQ(file.GetSkins().size(), static_cast<std::size_t>(header.skin_count));
      for (const quake::MdlSkin &skin : file.GetSkins())
      {
        ASSERT_FALSE(skin.pictures.empty());
        EXPECT_EQ(skin.times.size(), skin.is_group ? skin.pictures.size() : 0u);
        for (const auto &picture : skin.pictures)
        {
          EXPECT_EQ(picture.size(), picture_size);
        }
      }

      EXPECT_EQ(file.GetTextureCoordinates().size(), vertex_count);

      ASSERT_EQ(file.GetTriangles().size(), static_cast<std::size_t>(header.triangle_count));
      for (const quake::MdlTriangle &triangle : file.GetTriangles())
      {
        for (const std::int32_t vertex : triangle.vertices)
        {
          EXPECT_GE(vertex, 0);
          EXPECT_LT(vertex, header.vertex_count);
        }
      }

      ASSERT_EQ(file.GetFrames().size(), static_cast<std::size_t>(header.frame_count));
      for (const quake::MdlFrame &frame : file.GetFrames())
      {
        ASSERT_FALSE(frame.poses.empty());
        EXPECT_EQ(frame.times.size(), frame.is_group ? frame.poses.size() : 0u);
        for (const quake::MdlPose &pose : frame.poses)
        {
          ASSERT_EQ(pose.vertices.size(), vertex_count);
          for (const quake::MdlPackedVertex &vertex : pose.vertices)
          {
            EXPECT_LT(vertex.normal_index, MdlFile::normal_count);
          }
        }
      }
    }
  }
}
