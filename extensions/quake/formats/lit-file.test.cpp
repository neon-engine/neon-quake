#include "lit-file.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::LitFile;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  /// The bytes of a file with a start, a version, and these bytes after.
  std::vector<std::uint8_t> MakeFile(
    const std::string &start,
    const std::uint8_t version,
    const std::vector<std::uint8_t> &colours)
  {
    std::vector<std::uint8_t> bytes(start.begin(), start.end());
    bytes.insert(bytes.end(), {version, 0, 0, 0});
    bytes.insert(bytes.end(), colours.begin(), colours.end());
    return bytes;
  }

  /// What reading says is wrong. Empty when nothing is.
  std::string ReasonOfRefusal(const std::vector<std::uint8_t> &bytes, const std::size_t lighting_size)
  {
    LitFile file;
    std::string error;
    if (file.Read(bytes, lighting_size, error)) { return {}; }
    EXPECT_FALSE(error.empty());
    return error;
  }

  TEST(LitFileTest, ReadsThreeBytesOfColourForEveryByteOfTheLighting)
  {
    LitFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeFile("QLIT", 1, {1, 2, 3, 250, 128, 0}), 2, error)) << error;

    EXPECT_EQ(file.GetSampleCount(), 2u);
    EXPECT_THAT(file.GetColours(), ElementsAre(1, 2, 3, 250, 128, 0));
  }

  TEST(LitFileTest, ReadsTheLightOfALevelWithoutAnyLighting)
  {
    LitFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeFile("QLIT", 1, {}), 0, error)) << error;

    EXPECT_EQ(file.GetSampleCount(), 0u);
    EXPECT_THAT(file.GetColours(), IsEmpty());
  }

  TEST(LitFileTest, RefusesWhatDoesNotStartAsItShould)
  {
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIX", 1, {1, 2, 3}), 1), "The file does not start with QLIT");
    EXPECT_EQ(ReasonOfRefusal(MakeFile("IDSP", 1, {1, 2, 3}), 1), "The file does not start with QLIT");
  }

  TEST(LitFileTest, RefusesAnotherVersion)
  {
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 2, {1, 2, 3}), 1), "The version is 2, not 1");
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 0, {1, 2, 3}), 1), "The version is 0, not 1");
  }

  TEST(LitFileTest, RefusesAFileThatEndsBeforeItsHeader)
  {
    EXPECT_EQ(ReasonOfRefusal({}, 0), "The file ends before its header");
    EXPECT_EQ(ReasonOfRefusal({'Q', 'L', 'I', 'T', 1, 0, 0}, 0), "The file ends before its header");
  }

  TEST(LitFileTest, RefusesColoursThatAreNotThreeForEveryByteOfTheLighting)
  {
    // too few, too many, and not whole
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 1, {1, 2, 3}), 2),
      "The file has 3 bytes of colours, which are not three for each of the 2 bytes of the lighting of the level");
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 1, {1, 2, 3, 4, 5, 6}), 1),
      "The file has 6 bytes of colours, which are not three for each of the 1 bytes of the lighting of the level");
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 1, {1, 2, 3, 4}), 1),
      "The file has 4 bytes of colours, which are not three for each of the 1 bytes of the lighting of the level");
    EXPECT_EQ(ReasonOfRefusal(MakeFile("QLIT", 1, {1, 2, 3}), 0),
      "The file has 3 bytes of colours, which are not three for each of the 0 bytes of the lighting of the level");
  }

  TEST(LitFileTest, StaysAsItWasWhenItRefuses)
  {
    LitFile file;
    std::string error;
    ASSERT_TRUE(file.Read(MakeFile("QLIT", 1, {7, 8, 9}), 1, error)) << error;

    EXPECT_FALSE(file.Read(MakeFile("QLIT", 3, {1, 2, 3}), 1, error));
    EXPECT_FALSE(file.Read(MakeFile("QLIT", 1, {1, 2, 3}), 5, error));
    EXPECT_THAT(file.GetColours(), ElementsAre(7, 8, 9));
  }
}
