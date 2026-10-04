#include "sprite-file.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::SpriteFile;
  using quake::SpriteFrame;
  using quake::SpriteHeader;
  using quake::SpriteOrientation;
  using quake::SpritePicture;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// Where things are in the bytes of a sprite, for a test that changes
  /// one after the bytes were made.
  constexpr std::size_t orientation_at = 8;
  constexpr std::size_t width_at = 16;
  constexpr std::size_t height_at = 20;
  constexpr std::size_t frame_count_at = 24;
  constexpr std::size_t header_size = 36;

  void PutU32(std::vector<std::uint8_t> &bytes, const std::uint32_t value)
  {
    for (int shift = 0; shift < 32; shift += 8)
    {
      bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
  }

  void PutI32(std::vector<std::uint8_t> &bytes, const std::int32_t value)
  {
    PutU32(bytes, static_cast<std::uint32_t>(value));
  }

  void PutF32(std::vector<std::uint8_t> &bytes, const float value)
  {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    PutU32(bytes, bits);
  }

  /// Writes a number over the four bytes at a place.
  void Overwrite(std::vector<std::uint8_t> &bytes, const std::size_t at, const std::int32_t value)
  {
    for (std::size_t i = 0; i < 4; i++)
    {
      bytes[at + i] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> (8 * i));
    }
  }

  /// A picture around its origin whose pixels count up from `first`.
  SpritePicture MakePicture(const std::int32_t width, const std::int32_t height, const std::uint8_t first)
  {
    SpritePicture picture;
    picture.left = -width / 2;
    picture.up = height / 2;
    picture.width = width;
    picture.height = height;
    for (int i = 0; i < width * height; i++)
    {
      picture.pixels.push_back(static_cast<std::uint8_t>(first + i));
    }
    return picture;
  }

  /// The bytes of the file of a sprite. The count of frames is written as
  /// the header has it, whatever the list holds.
  std::vector<std::uint8_t> MakeBytes(
    const SpriteHeader &header,
    const std::vector<SpriteFrame> &frames,
    const std::uint32_t magic = SpriteFile::magic,
    const std::int32_t version = SpriteFile::version)
  {
    std::vector<std::uint8_t> bytes;
    PutU32(bytes, magic);
    PutI32(bytes, version);
    PutI32(bytes, static_cast<std::int32_t>(header.orientation));
    PutF32(bytes, header.bounding_radius);
    PutI32(bytes, header.width);
    PutI32(bytes, header.height);
    PutI32(bytes, header.frame_count);
    PutF32(bytes, header.beam_length);
    PutI32(bytes, header.sync_type);

    for (const SpriteFrame &frame : frames)
    {
      PutI32(bytes, frame.is_group ? 1 : 0);
      if (frame.is_group)
      {
        PutI32(bytes, static_cast<std::int32_t>(frame.pictures.size()));
        for (const float time : frame.times) { PutF32(bytes, time); }
      }
      for (const SpritePicture &picture : frame.pictures)
      {
        PutI32(bytes, picture.left);
        PutI32(bytes, picture.up);
        PutI32(bytes, picture.width);
        PutI32(bytes, picture.height);
        bytes.insert(bytes.end(), picture.pixels.begin(), picture.pixels.end());
      }
    }
    return bytes;
  }

  /// The header of the sprites of these tests, which have two frames.
  SpriteHeader MakeHeader()
  {
    SpriteHeader header;
    header.orientation = SpriteOrientation::Oriented;
    header.bounding_radius = 4.5f;
    header.width = 4;
    header.height = 6;
    header.frame_count = 2;
    header.beam_length = 1.5f;
    header.sync_type = 1;
    return header;
  }

  /// A picture of 4 by 2, and a group of two pictures of sizes of their
  /// own, 2 by 6 and 3 by 1.
  std::vector<SpriteFrame> MakeFrames()
  {
    SpriteFrame single;
    single.pictures = {MakePicture(4, 2, 0)};

    SpriteFrame group;
    group.is_group = true;
    group.pictures = {MakePicture(2, 6, 100), MakePicture(3, 1, 200)};
    group.times = {0.1f, 0.3f};

    return {single, group};
  }

  std::vector<std::uint8_t> MakeSprite()
  {
    return MakeBytes(MakeHeader(), MakeFrames());
  }

  /// What reading the bytes says is wrong with them, and nothing when they
  /// are read.
  std::string ErrorOf(const std::vector<std::uint8_t> &bytes)
  {
    SpriteFile file;
    std::string error;
    if (file.Read(bytes, error)) { return ""; }

    EXPECT_FALSE(error.empty());
    return error;
  }

  TEST(SpriteFileTest, ReadsTheHeader)
  {
    SpriteFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeSprite(), error)) << error;
    EXPECT_TRUE(error.empty());

    const SpriteHeader &header = file.GetHeader();
    EXPECT_EQ(header.orientation, SpriteOrientation::Oriented);
    EXPECT_EQ(header.bounding_radius, 4.5f);
    EXPECT_EQ(header.width, 4);
    EXPECT_EQ(header.height, 6);
    EXPECT_EQ(header.frame_count, 2);
    EXPECT_EQ(header.beam_length, 1.5f);
    EXPECT_EQ(header.sync_type, 1);
  }

  TEST(SpriteFileTest, ReadsAPictureWithWhereItHangsAndItsSize)
  {
    SpriteFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeSprite(), error)) << error;

    ASSERT_EQ(file.GetFrames().size(), 2u);
    const SpriteFrame &frame = file.GetFrames()[0];
    EXPECT_FALSE(frame.is_group);
    EXPECT_TRUE(frame.times.empty());
    ASSERT_EQ(frame.pictures.size(), 1u);

    const SpritePicture &picture = frame.pictures[0];
    EXPECT_EQ(picture.left, -2);
    EXPECT_EQ(picture.up, 1);
    EXPECT_EQ(picture.width, 4);
    EXPECT_EQ(picture.height, 2);
    EXPECT_THAT(picture.pixels, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
  }

  TEST(SpriteFileTest, ReadsAGroupOfPicturesWithItsTimesAndTheirOwnSizes)
  {
    SpriteFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeSprite(), error)) << error;

    const SpriteFrame &frame = file.GetFrames()[1];
    EXPECT_TRUE(frame.is_group);
    EXPECT_THAT(frame.times, ElementsAre(0.1f, 0.3f));
    ASSERT_EQ(frame.pictures.size(), 2u);

    EXPECT_EQ(frame.pictures[0].width, 2);
    EXPECT_EQ(frame.pictures[0].height, 6);
    EXPECT_EQ(frame.pictures[0].left, -1);
    EXPECT_EQ(frame.pictures[0].up, 3);
    ASSERT_EQ(frame.pictures[0].pixels.size(), 12u);
    EXPECT_EQ(frame.pictures[0].pixels[11], 111);

    EXPECT_EQ(frame.pictures[1].width, 3);
    EXPECT_EQ(frame.pictures[1].height, 1);
    EXPECT_THAT(frame.pictures[1].pixels, ElementsAre(200, 201, 202));
  }

  TEST(SpriteFileTest, ReadsEveryWayOfTurning)
  {
    for (std::int32_t orientation = 0; orientation <= 4; orientation++)
    {
      std::vector<std::uint8_t> bytes = MakeSprite();
      Overwrite(bytes, orientation_at, orientation);

      SpriteFile file;
      std::string error;
      ASSERT_TRUE(file.Read(bytes, error)) << error;
      EXPECT_EQ(static_cast<std::int32_t>(file.GetHeader().orientation), orientation);
    }
  }

  TEST(SpriteFileTest, FindsAPictureByItsFrameAndItsPlaceInTheGroup)
  {
    SpriteFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeSprite(), error)) << error;

    ASSERT_NE(file.FindPicture(0), nullptr);
    EXPECT_EQ(file.FindPicture(0)->width, 4);
    ASSERT_NE(file.FindPicture(1, 1), nullptr);
    EXPECT_EQ(file.FindPicture(1, 1)->width, 3);

    EXPECT_EQ(file.FindPicture(2), nullptr);
    EXPECT_EQ(file.FindPicture(0, 1), nullptr);
    EXPECT_EQ(file.FindPicture(1, 2), nullptr);
    EXPECT_EQ(SpriteFile().FindPicture(0), nullptr);
  }

  TEST(SpriteFileTest, LeavesAloneWhatFollowsTheLastFrame)
  {
    std::vector<std::uint8_t> bytes = MakeSprite();
    bytes.insert(bytes.end(), {1, 2, 3});
    EXPECT_EQ(ErrorOf(bytes), "");
  }

  TEST(SpriteFileTest, RefusesAnotherMagic)
  {
    // IDPO, which a model starts with
    EXPECT_THAT(ErrorOf(MakeBytes(MakeHeader(), MakeFrames(), 0x4f504449)), HasSubstr("IDSP"));
  }

  TEST(SpriteFileTest, RefusesAnotherVersion)
  {
    EXPECT_EQ(ErrorOf(MakeBytes(MakeHeader(), MakeFrames(), SpriteFile::magic, 2)), "The version is 2, not 1");
  }

  TEST(SpriteFileTest, RefusesAWayOfTurningThereIsNot)
  {
    for (const std::int32_t orientation : {-1, 5, INT32_MAX})
    {
      std::vector<std::uint8_t> bytes = MakeSprite();
      Overwrite(bytes, orientation_at, orientation);
      EXPECT_EQ(
        ErrorOf(bytes),
        "The way the sprite turns, " + std::to_string(orientation) + ", is not between 0 and 4");
    }
  }

  TEST(SpriteFileTest, RefusesNoBytesAndAHeaderCutShort)
  {
    EXPECT_EQ(ErrorOf({}), "The file ends before its header");

    std::vector<std::uint8_t> bytes = MakeSprite();
    bytes.resize(header_size - 1);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before its header");
  }

  TEST(SpriteFileTest, RefusesACountOfTheHeaderThatIsNothingNegativeOrPastTheMost)
  {
    struct Count
    {
      std::size_t at;
      std::int32_t most;
      std::string what;
    };
    const std::vector<Count> counts = {
      {width_at, SpriteFile::most_picture_side, "width of the sprite"},
      {height_at, SpriteFile::most_picture_side, "height of the sprite"},
      {frame_count_at, SpriteFile::most_frames, "number of frames"},
    };

    for (const Count &count : counts)
    {
      for (const std::int32_t value : {0, -1, INT32_MIN, count.most + 1, INT32_MAX})
      {
        std::vector<std::uint8_t> bytes = MakeSprite();
        Overwrite(bytes, count.at, value);

        const std::string expected = "The " + count.what + ", " + std::to_string(value) +
          ", is not between 1 and " + std::to_string(count.most);
        EXPECT_EQ(ErrorOf(bytes), expected);
      }
    }
  }

  TEST(SpriteFileTest, RefusesAPictureWithASizeThatCannotBe)
  {
    // after the header: the kind of the first frame, where its picture
    // hangs, then its width and its height
    const std::size_t picture_width_at = header_size + 4 + 8;
    const std::size_t picture_height_at = picture_width_at + 4;

    for (const std::int32_t value : {0, -1, INT32_MIN, SpriteFile::most_picture_side + 1, INT32_MAX})
    {
      std::vector<std::uint8_t> bytes = MakeSprite();
      Overwrite(bytes, picture_width_at, value);
      EXPECT_THAT(ErrorOf(bytes), HasSubstr("The width of frame 0, " + std::to_string(value)));

      bytes = MakeSprite();
      Overwrite(bytes, picture_height_at, value);
      EXPECT_THAT(ErrorOf(bytes), HasSubstr("The height of frame 0, " + std::to_string(value)));
    }

    // allowed by itself, and far more than the file has
    std::vector<std::uint8_t> bytes = MakeSprite();
    Overwrite(bytes, picture_width_at, SpriteFile::most_picture_side);
    Overwrite(bytes, picture_height_at, SpriteFile::most_picture_side);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of frame 0");
  }

  TEST(SpriteFileTest, RefusesAGroupWithACountThatCannotBe)
  {
    // after the header: the first frame with its kind, its picture of 4 by
    // 2, then the kind of the second, then its count
    const std::size_t count_at = header_size + 4 + 16 + 8 + 4;

    for (const std::int32_t value : {0, -1, SpriteFile::most_in_group + 1})
    {
      std::vector<std::uint8_t> bytes = MakeSprite();
      Overwrite(bytes, count_at, value);
      EXPECT_THAT(ErrorOf(bytes), HasSubstr("The number of pictures of frame 1, " + std::to_string(value)));
    }

    std::vector<std::uint8_t> bytes = MakeSprite();
    Overwrite(bytes, count_at, SpriteFile::most_in_group);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of frame 1");
  }

  TEST(SpriteFileTest, RefusesMoreFramesThanTheBytesHold)
  {
    std::vector<std::uint8_t> bytes = MakeSprite();
    Overwrite(bytes, frame_count_at, 3);
    EXPECT_EQ(ErrorOf(bytes), "The file ends before the end of frame 2");
  }

  TEST(SpriteFileTest, RefusesAFileCutShortAnywhere)
  {
    const std::vector<std::uint8_t> whole = MakeSprite();
    ASSERT_EQ(ErrorOf(whole), "");

    for (std::size_t size = 0; size < whole.size(); size++)
    {
      // a copy of its own size, so that a tool that watches memory sees a
      // read past its end
      const std::vector<std::uint8_t> cut(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size));
      EXPECT_THAT(ErrorOf(cut), HasSubstr("The file ends before")) << "with " << size << " bytes";
    }
  }

  TEST(SpriteFileTest, StaysAsItWasWhenTheBytesAreRefused)
  {
    SpriteFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeSprite(), error)) << error;

    std::vector<std::uint8_t> wrong = MakeSprite();
    wrong.pop_back();
    EXPECT_FALSE(file.Read(wrong, error));
    EXPECT_FALSE(error.empty());

    EXPECT_EQ(file.GetHeader().frame_count, 2);
    ASSERT_EQ(file.GetFrames().size(), 2u);
    EXPECT_EQ(file.GetFrames()[1].pictures.size(), 2u);
  }

  TEST(SpriteFileTest, ReadsEverySpriteOfRealData)
  {
    const std::filesystem::path folder = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "progs";
    std::error_code ignored;
    std::vector<std::filesystem::path> paths;
    if (std::filesystem::is_directory(folder, ignored))
    {
      for (const auto &entry : std::filesystem::directory_iterator(folder, ignored))
      {
        if (entry.path().extension() == ".spr") { paths.push_back(entry.path()); }
      }
    }
    if (paths.empty())
    {
      GTEST_SKIP() << "No sprites in " << QUAKE_TEST_DATA_DIRECTORY << "/progs";
    }

    for (const std::filesystem::path &path : paths)
    {
      SCOPED_TRACE(path.filename().string());

      std::ifstream stream(path, std::ios::binary);
      const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};

      SpriteFile file;
      std::string error;
      ASSERT_TRUE(file.Read(bytes, error)) << error;

      const SpriteHeader &header = file.GetHeader();
      ASSERT_EQ(file.GetFrames().size(), static_cast<std::size_t>(header.frame_count));
      for (const SpriteFrame &frame : file.GetFrames())
      {
        ASSERT_FALSE(frame.pictures.empty());
        EXPECT_EQ(frame.times.size(), frame.is_group ? frame.pictures.size() : 0u);
        for (const SpritePicture &picture : frame.pictures)
        {
          EXPECT_LE(picture.width, header.width);
          EXPECT_LE(picture.height, header.height);
          EXPECT_EQ(
            picture.pixels.size(),
            static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height));
        }
      }
    }
  }
}
