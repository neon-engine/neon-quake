#include "pak-file.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// The tests of PakFile, and what makes the bytes of the paks they read.
namespace
{
  using quake::PakEntry;
  using quake::PakFile;
  using quake::PakHeader;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  using Bytes = std::vector<std::uint8_t>;

  /// Where in the header the offset and the size of the directory are.
  constexpr std::size_t directory_offset_at = 4;
  constexpr std::size_t directory_size_at = 8;

  /// Where in an entry its offset and its size are.
  constexpr std::size_t entry_offset_at = 56;
  constexpr std::size_t entry_size_at = 60;

  /// Writes a number of four bytes at a place, the lowest byte first.
  void PutI32(Bytes &bytes, const std::size_t at, const std::int32_t value)
  {
    const auto bits = static_cast<std::uint32_t>(value);
    for (std::size_t i = 0; i < 4; i++)
    {
      bytes[at + i] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
  }

  /// The bytes of a pak with these files: the header, the bytes of each
  /// file in turn, and the directory last.
  Bytes MakePak(const std::vector<std::pair<std::string, Bytes>> &files)
  {
    Bytes bytes = {'P', 'A', 'C', 'K', 0, 0, 0, 0, 0, 0, 0, 0};

    std::vector<std::size_t> offsets;
    for (const auto &[name, content] : files)
    {
      offsets.push_back(bytes.size());
      bytes.insert(bytes.end(), content.begin(), content.end());
    }

    const std::size_t directory_offset = bytes.size();
    bytes.resize(directory_offset + files.size() * PakEntry::size_in_bytes, 0);
    PutI32(bytes, directory_offset_at, static_cast<std::int32_t>(directory_offset));
    PutI32(bytes, directory_size_at, static_cast<std::int32_t>(files.size() * PakEntry::size_in_bytes));

    for (std::size_t i = 0; i < files.size(); i++)
    {
      const std::size_t entry = directory_offset + i * PakEntry::size_in_bytes;
      const std::string &name = files[i].first;
      for (std::size_t letter = 0; letter < name.size() && letter < PakEntry::name_size; letter++)
      {
        bytes[entry + letter] = static_cast<std::uint8_t>(name[letter]);
      }
      PutI32(bytes, entry + entry_offset_at, static_cast<std::int32_t>(offsets[i]));
      PutI32(bytes, entry + entry_size_at, static_cast<std::int32_t>(files[i].second.size()));
    }
    return bytes;
  }

  /// A pak of three files: 12 bytes of header, 3 + 2 + 4 of files, and the
  /// directory at 21.
  Bytes MakeThreeFiles()
  {
    return MakePak({
      {"gfx/palette.lmp", {1, 2, 3}},
      {"maps/e1m1.bsp", {4, 5}},
      {"maps/start.bsp", {6, 7, 8, 9}},
    });
  }

  /// Where entry `index` of the pak of three files is.
  std::size_t EntryAt(const std::size_t index)
  {
    return 21 + index * PakEntry::size_in_bytes;
  }

  /// What reading the bytes says is wrong with them. Fails the test when
  /// they are read after all.
  std::string ErrorOf(const Bytes &bytes)
  {
    PakFile pak;
    std::string error;
    EXPECT_FALSE(pak.Read(bytes, error));
    EXPECT_THAT(pak.GetEntries(), IsEmpty());
    return error;
  }

  TEST(PakFileTest, ReadsTheHeaderAndTheDirectory)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;
    EXPECT_THAT(error, IsEmpty());

    EXPECT_EQ(pak.GetHeader().directory_offset, 21u);
    EXPECT_EQ(pak.GetHeader().directory_size, 3 * PakEntry::size_in_bytes);

    const std::vector<PakEntry> &entries = pak.GetEntries();
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].name, "gfx/palette.lmp");
    EXPECT_EQ(entries[0].offset, 12u);
    EXPECT_EQ(entries[0].size, 3u);
    EXPECT_EQ(entries[1].name, "maps/e1m1.bsp");
    EXPECT_EQ(entries[1].offset, 15u);
    EXPECT_EQ(entries[1].size, 2u);
    EXPECT_EQ(entries[2].name, "maps/start.bsp");
    EXPECT_EQ(entries[2].offset, 17u);
    EXPECT_EQ(entries[2].size, 4u);
  }

  TEST(PakFileTest, ReadsAPakWithoutFiles)
  {
    const Bytes bytes = MakePak({});
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_THAT(pak.GetEntries(), IsEmpty());
    EXPECT_THAT(pak.ListNames(), IsEmpty());
    EXPECT_FALSE(pak.Has("gfx/palette.lmp"));
  }

  TEST(PakFileTest, HasNothingBeforeItIsRead)
  {
    const PakFile pak;

    EXPECT_THAT(pak.GetEntries(), IsEmpty());
    EXPECT_EQ(pak.Find("maps/e1m1.bsp"), nullptr);
    EXPECT_TRUE(pak.GetBytes("maps/e1m1.bsp").empty());
    EXPECT_TRUE(pak.GetBytes(PakEntry{.name = "maps/e1m1.bsp", .offset = 0, .size = 1}).empty());
  }

  TEST(PakFileTest, FindsAFileByItsName)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    const PakEntry *entry = pak.Find("maps/e1m1.bsp");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry, &pak.GetEntries()[1]);
    EXPECT_TRUE(pak.Has("maps/e1m1.bsp"));

    EXPECT_EQ(pak.Find("maps/e1m2.bsp"), nullptr);
    EXPECT_FALSE(pak.Has("maps/e1m2.bsp"));
    EXPECT_FALSE(pak.Has(""));
  }

  TEST(PakFileTest, ComparesNamesExactly)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_FALSE(pak.Has("MAPS/E1M1.BSP"));
    EXPECT_FALSE(pak.Has("maps/E1M1.bsp"));
    EXPECT_FALSE(pak.Has("maps\\e1m1.bsp"));
    EXPECT_FALSE(pak.Has("/maps/e1m1.bsp"));
    EXPECT_FALSE(pak.Has("maps/e1m1"));
  }

  TEST(PakFileTest, HandsOutTheBytesOfAFileWithoutCopying)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    const auto by_name = pak.GetBytes("maps/start.bsp");
    EXPECT_THAT(by_name, ElementsAre(6, 7, 8, 9));
    EXPECT_EQ(by_name.data(), bytes.data() + 17);

    const auto by_entry = pak.GetBytes(pak.GetEntries()[0]);
    EXPECT_THAT(by_entry, ElementsAre(1, 2, 3));
    EXPECT_EQ(by_entry.data(), bytes.data() + 12);

    EXPECT_TRUE(pak.GetBytes("maps/e1m2.bsp").empty());
  }

  TEST(PakFileTest, HasAFileOfNoBytes)
  {
    const Bytes bytes = MakePak({{"empty.txt", {}}, {"after.txt", {1}}});
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_TRUE(pak.Has("empty.txt"));
    EXPECT_TRUE(pak.GetBytes("empty.txt").empty());
    EXPECT_THAT(pak.GetBytes("after.txt"), ElementsAre(1));
  }

  TEST(PakFileTest, ReadsAFileThatEndsWhereThePakEnds)
  {
    // the directory first and the file last, so that its last byte is the last of the pak
    Bytes bytes = MakePak({{"last.txt", {}}});
    PutI32(bytes, 12 + entry_offset_at, static_cast<std::int32_t>(bytes.size()));
    PutI32(bytes, 12 + entry_size_at, 2);
    bytes.push_back(8);
    bytes.push_back(9);

    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;
    EXPECT_THAT(pak.GetBytes("last.txt"), ElementsAre(8, 9));
  }

  TEST(PakFileTest, ReadsANameThatFillsItsBytesWithoutAZero)
  {
    const std::string long_name(PakEntry::name_size, 'a');
    const Bytes bytes = MakePak({{long_name, {1}}, {"b", {2}}});
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_EQ(pak.GetEntries()[0].name, long_name);
    EXPECT_THAT(pak.GetBytes(long_name), ElementsAre(1));
    EXPECT_THAT(pak.GetBytes("b"), ElementsAre(2));
  }

  TEST(PakFileTest, FindsTheFirstOfTwoEntriesWithOneName)
  {
    const Bytes bytes = MakePak({{"twice.txt", {1}}, {"twice.txt", {2}}});
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_THAT(pak.GetBytes("twice.txt"), ElementsAre(1));
    EXPECT_THAT(pak.ListNames(), ElementsAre("twice.txt", "twice.txt"));
  }

  TEST(PakFileTest, ListsTheNamesInTheOrderOfTheDirectory)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_THAT(pak.ListNames(), ElementsAre("gfx/palette.lmp", "maps/e1m1.bsp", "maps/start.bsp"));
    EXPECT_THAT(pak.ListNames("/"), ElementsAre("gfx/palette.lmp", "maps/e1m1.bsp", "maps/start.bsp"));
  }

  TEST(PakFileTest, ListsTheNamesUnderAFolder)
  {
    const Bytes bytes = MakePak({
      {"maps/e1m1.bsp", {}},
      {"mapsource/e1m1.map", {}},
      {"maps", {}},
      {"maps/deep/b.bsp", {}},
      {"gfx/maps/c.lmp", {}},
    });
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_THAT(pak.ListNames("maps"), ElementsAre("maps/e1m1.bsp", "maps/deep/b.bsp"));
    EXPECT_THAT(pak.ListNames("maps/"), ElementsAre("maps/e1m1.bsp", "maps/deep/b.bsp"));
    EXPECT_THAT(pak.ListNames("maps/deep"), ElementsAre("maps/deep/b.bsp"));
    EXPECT_THAT(pak.ListNames("MAPS"), IsEmpty());
    EXPECT_THAT(pak.ListNames("sound"), IsEmpty());
  }

  TEST(PakFileTest, GivesNoBytesForAnEntryThatLiesOutsideIt)
  {
    const Bytes bytes = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(bytes, error)) << error;

    EXPECT_TRUE(pak.GetBytes(PakEntry{.name = "other", .offset = bytes.size() - 1, .size = 2}).empty());
    EXPECT_TRUE(pak.GetBytes(PakEntry{.name = "other", .offset = bytes.size() + 1, .size = 0}).empty());
    EXPECT_TRUE(pak.GetBytes(PakEntry{.name = "other", .offset = 2, .size = static_cast<std::size_t>(-1)}).empty());
  }

  TEST(PakFileTest, RefusesBytesTooFewForAHeader)
  {
    EXPECT_THAT(ErrorOf({}), HasSubstr("header"));

    Bytes bytes = MakePak({});
    bytes.pop_back();
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("only 11"));
  }

  TEST(PakFileTest, RefusesBytesThatDoNotStartWithPack)
  {
    Bytes bytes = MakeThreeFiles();
    bytes[3] = 'k';
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("PACK"));

    // the archive of another game of the same makers
    bytes = MakeThreeFiles();
    bytes[0] = 'W';
    bytes[1] = 'A';
    bytes[2] = 'D';
    bytes[3] = '2';
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("PACK"));
  }

  TEST(PakFileTest, RefusesADirectoryThatStartsBeforeTheFile)
  {
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, directory_offset_at, -64);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("directory starts at -64"));
  }

  TEST(PakFileTest, RefusesADirectoryOfLessThanNoBytes)
  {
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, directory_size_at, -64);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("size of -64"));
  }

  TEST(PakFileTest, RefusesADirectoryThatIsNotANumberOfEntries)
  {
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, directory_size_at, 3 * 64 - 1);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("not a number of entries"));

    PutI32(bytes, directory_size_at, 1);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("not a number of entries"));
  }

  TEST(PakFileTest, RefusesADirectoryThatLiesOutsideTheFile)
  {
    // one entry more than there is
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, directory_size_at, 4 * 64);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));

    // starting one byte late
    bytes = MakeThreeFiles();
    PutI32(bytes, directory_offset_at, 22);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));

    // starting past the end, even with nothing in it
    bytes = MakeThreeFiles();
    PutI32(bytes, directory_offset_at, static_cast<std::int32_t>(bytes.size() + 1));
    PutI32(bytes, directory_size_at, 0);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));

    // the largest numbers the file can say, whose sum does not fit in its four bytes
    bytes = MakeThreeFiles();
    PutI32(bytes, directory_offset_at, 0x7fffffff);
    PutI32(bytes, directory_size_at, 0x7fffffc0);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));

    // the file cut short in the middle of its directory
    bytes = MakeThreeFiles();
    bytes.resize(bytes.size() - 10);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));
  }

  TEST(PakFileTest, RefusesAnEntryThatStartsBeforeTheFile)
  {
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, EntryAt(1) + entry_offset_at, -1);

    const std::string error = ErrorOf(bytes);
    EXPECT_THAT(error, HasSubstr("Entry 1"));
    EXPECT_THAT(error, HasSubstr("maps/e1m1.bsp"));
    EXPECT_THAT(error, HasSubstr("starts at -1"));
  }

  TEST(PakFileTest, RefusesAnEntryOfLessThanNoBytes)
  {
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, EntryAt(2) + entry_size_at, -4);

    const std::string error = ErrorOf(bytes);
    EXPECT_THAT(error, HasSubstr("Entry 2"));
    EXPECT_THAT(error, HasSubstr("maps/start.bsp"));
    EXPECT_THAT(error, HasSubstr("size of -4"));
  }

  TEST(PakFileTest, RefusesAnEntryWhoseBytesLieOutsideTheFile)
  {
    // one byte more than the file has left
    Bytes bytes = MakeThreeFiles();
    PutI32(bytes, EntryAt(0) + entry_size_at, static_cast<std::int32_t>(bytes.size() - 12 + 1));
    std::string error = ErrorOf(bytes);
    EXPECT_THAT(error, HasSubstr("Entry 0"));
    EXPECT_THAT(error, HasSubstr("gfx/palette.lmp"));
    EXPECT_THAT(error, HasSubstr("lies outside the file"));

    // starting past the end, even with nothing in it
    bytes = MakeThreeFiles();
    PutI32(bytes, EntryAt(1) + entry_offset_at, static_cast<std::int32_t>(bytes.size() + 1));
    PutI32(bytes, EntryAt(1) + entry_size_at, 0);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));

    // the largest numbers the file can say, whose sum does not fit in its four bytes
    bytes = MakeThreeFiles();
    PutI32(bytes, EntryAt(2) + entry_offset_at, 0x7fffffff);
    PutI32(bytes, EntryAt(2) + entry_size_at, 0x7fffffff);
    EXPECT_THAT(ErrorOf(bytes), HasSubstr("lies outside the file"));
  }

  TEST(PakFileTest, StaysAsItWasWhenBytesAreRefused)
  {
    const Bytes good = MakeThreeFiles();
    PakFile pak;
    std::string error;
    ASSERT_TRUE(pak.Read(good, error)) << error;

    Bytes bad = MakePak({{"other.txt", {1}}});
    bad[0] = 'p';
    EXPECT_FALSE(pak.Read(bad, error));
    EXPECT_THAT(error, HasSubstr("PACK"));

    EXPECT_EQ(pak.GetHeader().directory_offset, 21u);
    EXPECT_EQ(pak.GetEntries().size(), 3u);
    EXPECT_THAT(pak.GetBytes("maps/e1m1.bsp"), ElementsAre(4, 5));
    EXPECT_FALSE(pak.Has("other.txt"));
  }
}
