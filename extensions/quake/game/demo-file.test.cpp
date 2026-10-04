#include "demo-file.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "demo-bytes.test.hpp"

// The tests of DemoFile with bytes made here.
namespace
{
  using quake::DemoBlock;
  using quake::DemoBytes;
  using quake::DemoFile;
  using quake::LevelVector;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  TEST(DemoFileTest, ReadsTheTrackAndEveryBlockWithItsAnglesAndBytes)
  {
    DemoBytes first;
    first.Byte(1).Byte(2).Byte(3);
    DemoBytes second;
    second.Byte(9);
    DemoBytes bytes;
    bytes.Line("-1").Block({10.0f, -90.5f, 2.0f}, first).Block({0.0f, 180.0f, 0.0f}, second);

    DemoFile file;
    std::string problem;
    ASSERT_TRUE(file.Read(bytes.Get(), problem)) << problem;

    EXPECT_EQ(file.GetForcedTrack(), DemoFile::no_forced_track);
    EXPECT_FALSE(file.IsCutShort());
    ASSERT_EQ(file.GetBlocks().size(), 2u);
    EXPECT_EQ(file.GetBlocks()[0].view_angles, (LevelVector{10.0f, -90.5f, 2.0f}));
    EXPECT_THAT(file.GetBlocks()[0].message, ElementsAre(1, 2, 3));
    EXPECT_EQ(file.GetBlocks()[1].view_angles, (LevelVector{0.0f, 180.0f, 0.0f}));
    EXPECT_THAT(file.GetBlocks()[1].message, ElementsAre(9));

    // the blocks are bytes of what was read, not copies
    EXPECT_EQ(file.GetBlocks()[1].message.data(), &bytes.bytes.back());
  }

  TEST(DemoFileTest, ReadsATrackThatIsForced)
  {
    DemoBytes bytes;
    bytes.Line("12");

    DemoFile file;
    std::string problem;
    ASSERT_TRUE(file.Read(bytes.Get(), problem)) << problem;
    EXPECT_EQ(file.GetForcedTrack(), 12);
    EXPECT_THAT(file.GetBlocks(), IsEmpty());
  }

  TEST(DemoFileTest, ReadsABlockWithoutBytes)
  {
    DemoBytes bytes;
    bytes.Line("0").Block({1.0f, 2.0f, 3.0f}, {});

    DemoFile file;
    std::string problem;
    ASSERT_TRUE(file.Read(bytes.Get(), problem)) << problem;
    ASSERT_EQ(file.GetBlocks().size(), 1u);
    EXPECT_THAT(file.GetBlocks()[0].message, IsEmpty());
  }

  TEST(DemoFileTest, RefusesBytesWithoutTheLineOfTheTrack)
  {
    DemoFile file;
    std::string problem;

    EXPECT_FALSE(file.Read({}, problem));
    EXPECT_THAT(problem, HasSubstr("music track"));

    for (const std::string_view start : {"\n", "-\n", "abc\n", "12", "12 \n", "-", "1x\n"})
    {
      DemoBytes bytes;
      for (const char letter : start) { bytes.Byte(static_cast<std::uint8_t>(letter)); }
      problem.clear();
      EXPECT_FALSE(file.Read(bytes.Get(), problem)) << start;
      EXPECT_THAT(problem, HasSubstr("music track")) << start;
    }

    DemoBytes long_number;
    long_number.Line("1234567890123456789012345678901234567890");
    EXPECT_FALSE(file.Read(long_number.Get(), problem));
    EXPECT_THAT(problem, HasSubstr("too long"));
  }

  TEST(DemoFileTest, RefusesABlockWithALengthThatCannotBe)
  {
    DemoBytes message;
    message.Byte(1);
    DemoFile file;
    std::string problem;

    DemoBytes negative;
    negative.Line("-1").Block({}, message).Long(-5).Float(0.0f).Float(0.0f).Float(0.0f).Byte(1);
    EXPECT_FALSE(file.Read(negative.Get(), problem));
    EXPECT_THAT(problem, HasSubstr("Block 1"));
    EXPECT_THAT(problem, HasSubstr("-5 bytes"));
    EXPECT_THAT(file.GetBlocks(), IsEmpty());

    DemoBytes huge;
    huge.Line("-1").Long(static_cast<std::int64_t>(DemoFile::max_message_size) + 1);
    huge.Float(0.0f).Float(0.0f).Float(0.0f).Byte(1);
    EXPECT_FALSE(file.Read(huge.Get(), problem));
    EXPECT_THAT(problem, HasSubstr("Block 0"));
  }

  TEST(DemoFileTest, KeepsTheWholeBlocksOfAFileThatEndsInTheMiddleOfOne)
  {
    DemoBytes message;
    message.Byte(1).Byte(2).Byte(3).Byte(4);
    DemoBytes whole;
    whole.Line("-1").Block({}, message).Block({}, message);

    // cut anywhere in the second block: in its length, its angles, its bytes
    const std::size_t first_end = 3 + 16 + 4;
    for (std::size_t size = first_end + 1; size < whole.bytes.size(); size++)
    {
      const std::vector<std::uint8_t> cut(whole.bytes.begin(), whole.bytes.begin() + static_cast<std::ptrdiff_t>(size));
      DemoFile file;
      std::string problem;
      ASSERT_TRUE(file.Read(cut, problem)) << size << ": " << problem;
      EXPECT_TRUE(file.IsCutShort()) << size;
      EXPECT_EQ(file.GetBlocks().size(), 1u) << size;
    }
  }

  TEST(DemoFileTest, ForgetsWhatItReadBefore)
  {
    DemoBytes message;
    message.Byte(1);
    DemoBytes first;
    first.Line("5").Block({}, message).Block({}, message);
    DemoBytes second;
    second.Line("-1").Block({}, message);

    DemoFile file;
    std::string problem;
    ASSERT_TRUE(file.Read(first.Get(), problem));
    ASSERT_TRUE(file.Read(second.Get(), problem));
    EXPECT_EQ(file.GetForcedTrack(), -1);
    EXPECT_EQ(file.GetBlocks().size(), 1u);
  }
}
