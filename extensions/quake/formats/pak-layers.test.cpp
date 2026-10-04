#include "pak-layers.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// The tests of PakLayers, and what makes the bytes of the paks they layer.
namespace
{
  using quake::PakEntry;
  using quake::PakFile;
  using quake::PakLayers;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  using Bytes = std::vector<std::uint8_t>;

  /// Adds a number of four bytes to the end, the lowest byte first.
  void AddI32(Bytes &bytes, const std::size_t value)
  {
    for (std::size_t i = 0; i < 4; i++)
    {
      bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
  }

  /// The bytes of a pak with these files: the header, the directory, and
  /// the bytes of each file in turn.
  Bytes MakePak(const std::vector<std::pair<std::string, Bytes>> &files)
  {
    const std::size_t directory_size = files.size() * PakEntry::size_in_bytes;

    Bytes bytes = {'P', 'A', 'C', 'K'};
    AddI32(bytes, 12);
    AddI32(bytes, directory_size);

    std::size_t offset = 12 + directory_size;
    for (const auto &[name, content] : files)
    {
      Bytes name_bytes(name.begin(), name.end());
      name_bytes.resize(PakEntry::name_size, 0);
      bytes.insert(bytes.end(), name_bytes.begin(), name_bytes.end());
      AddI32(bytes, offset);
      AddI32(bytes, content.size());
      offset += content.size();
    }
    for (const auto &[name, content] : files)
    {
      bytes.insert(bytes.end(), content.begin(), content.end());
    }
    return bytes;
  }

  /// The pak those bytes are. Fails the test when they are refused.
  PakFile ReadPak(const Bytes &bytes)
  {
    PakFile pak;
    std::string error;
    EXPECT_TRUE(pak.Read(bytes, error)) << error;
    return pak;
  }

  /// What the tests layer: two paks as the game has them, the second with
  /// one file of the first again and one of its own.
  class PakLayersTest : public ::testing::Test
  {
  protected:
    Bytes _pak0 = MakePak({
      {"maps/start.bsp", {1, 2, 3}},
      {"gfx/palette.lmp", {4}},
      {"progs.dat", {5, 6}},
    });
    Bytes _pak1 = MakePak({
      {"progs.dat", {7, 8, 9}},
      {"maps/e2m1.bsp", {10}},
    });
  };

  TEST_F(PakLayersTest, TakesAFileFromThePakAddedLast)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));
    ASSERT_EQ(layers.GetCount(), 2u);

    EXPECT_TRUE(layers.Has("progs.dat"));
    EXPECT_THAT(layers.GetBytes("progs.dat"), ElementsAre(7, 8, 9));

    // in the other order the other pak wins
    PakLayers other_way;
    other_way.Add(ReadPak(_pak1));
    other_way.Add(ReadPak(_pak0));
    EXPECT_THAT(other_way.GetBytes("progs.dat"), ElementsAre(5, 6));
  }

  TEST_F(PakLayersTest, FindsWhatOnlyOnePakHas)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    EXPECT_THAT(layers.GetBytes("maps/start.bsp"), ElementsAre(1, 2, 3));
    EXPECT_THAT(layers.GetBytes("maps/e2m1.bsp"), ElementsAre(10));
  }

  TEST_F(PakLayersTest, HandsOutBytesOfThePakThatWinsWithoutCopying)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    const auto progs = layers.GetBytes("progs.dat");
    ASSERT_EQ(progs.size(), 3u);
    EXPECT_EQ(progs.data(), _pak1.data() + 12 + 2 * PakEntry::size_in_bytes);

    const auto palette = layers.GetBytes("gfx/palette.lmp");
    ASSERT_EQ(palette.size(), 1u);
    EXPECT_EQ(palette.data(), _pak0.data() + 12 + 3 * PakEntry::size_in_bytes + 3);
  }

  TEST_F(PakLayersTest, TellsWhichPakAFileIsTakenFrom)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    const PakFile *of_progs = layers.FindPak("progs.dat");
    ASSERT_NE(of_progs, nullptr);
    EXPECT_TRUE(of_progs->Has("maps/e2m1.bsp"));

    const PakFile *of_palette = layers.FindPak("gfx/palette.lmp");
    ASSERT_NE(of_palette, nullptr);
    EXPECT_TRUE(of_palette->Has("maps/start.bsp"));

    EXPECT_EQ(layers.FindPak("maps/e1m1.bsp"), nullptr);
  }

  TEST_F(PakLayersTest, HasNoFileThatNoPakHas)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    EXPECT_FALSE(layers.Has("maps/e1m1.bsp"));
    EXPECT_TRUE(layers.GetBytes("maps/e1m1.bsp").empty());

    // names are compared exactly, as a single pak compares them
    EXPECT_FALSE(layers.Has("PROGS.DAT"));
    EXPECT_FALSE(layers.Has("maps\\start.bsp"));
  }

  TEST_F(PakLayersTest, AFileOfNoBytesInALaterPakHidesTheEarlierOne)
  {
    const Bytes emptied = MakePak({{"progs.dat", {}}});
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(emptied));

    EXPECT_TRUE(layers.Has("progs.dat"));
    EXPECT_TRUE(layers.GetBytes("progs.dat").empty());
  }

  TEST_F(PakLayersTest, ListsEveryNameOnceAndSorted)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    EXPECT_THAT(
      layers.ListNames(),
      ElementsAre("gfx/palette.lmp", "maps/e2m1.bsp", "maps/start.bsp", "progs.dat"));
  }

  TEST_F(PakLayersTest, ListsTheNamesUnderAFolderOfAllPaks)
  {
    PakLayers layers;
    layers.Add(ReadPak(_pak0));
    layers.Add(ReadPak(_pak1));

    EXPECT_THAT(layers.ListNames("maps"), ElementsAre("maps/e2m1.bsp", "maps/start.bsp"));
    EXPECT_THAT(layers.ListNames("gfx/"), ElementsAre("gfx/palette.lmp"));
    EXPECT_THAT(layers.ListNames("sound"), IsEmpty());
  }

  TEST(PakLayersEmptyTest, HasNothingWithoutPaks)
  {
    const PakLayers layers;

    EXPECT_EQ(layers.GetCount(), 0u);
    EXPECT_FALSE(layers.Has("progs.dat"));
    EXPECT_EQ(layers.FindPak("progs.dat"), nullptr);
    EXPECT_TRUE(layers.GetBytes("progs.dat").empty());
    EXPECT_THAT(layers.ListNames(), IsEmpty());
  }
}
