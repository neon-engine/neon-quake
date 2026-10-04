#include "wad.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "palette.hpp"

namespace
{
  using quake::Palette;
  using quake::Picture;
  using quake::Wad;
  using quake::WadLump;
  using quake::WadLumpType;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// A lump to make a wad of: what its entry says, and its bytes.
  struct TestLump
  {
    std::string name;
    char type = 'B';
    std::vector<std::uint8_t> bytes;
    std::uint8_t compression = 0;
  };

  /// Adds a number of four bytes, the lowest first.
  void add_i32(std::vector<std::uint8_t> &bytes, const std::int32_t value)
  {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8)
    {
      bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
  }

  /// Writes a number of four bytes over the ones at a place.
  void set_i32(std::vector<std::uint8_t> &bytes, const std::size_t at, const std::int32_t value)
  {
    std::vector<std::uint8_t> number;
    add_i32(number, value);
    std::copy(number.begin(), number.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
  }

  /// The bytes of a picture with its width and height in front.
  std::vector<std::uint8_t> make_picture(
    const std::int32_t width,
    const std::int32_t height,
    const std::vector<std::uint8_t> &pixels)
  {
    std::vector<std::uint8_t> bytes;
    add_i32(bytes, width);
    add_i32(bytes, height);
    bytes.insert(bytes.end(), pixels.begin(), pixels.end());
    return bytes;
  }

  /// The bytes of a wad: the start, the lumps one after the other, and the
  /// directory after them, as the tools of the game write it.
  std::vector<std::uint8_t> make_wad(const std::vector<TestLump> &lumps)
  {
    std::vector<std::uint8_t> bytes = {'W', 'A', 'D', '2'};
    add_i32(bytes, static_cast<std::int32_t>(lumps.size()));
    add_i32(bytes, 0);

    std::vector<std::int32_t> offsets;
    for (const TestLump &lump : lumps)
    {
      offsets.push_back(static_cast<std::int32_t>(bytes.size()));
      bytes.insert(bytes.end(), lump.bytes.begin(), lump.bytes.end());
    }

    set_i32(bytes, 8, static_cast<std::int32_t>(bytes.size()));
    for (std::size_t i = 0; i < lumps.size(); i++)
    {
      const TestLump &lump = lumps[i];
      add_i32(bytes, offsets[i]);
      add_i32(bytes, static_cast<std::int32_t>(lump.bytes.size()));
      add_i32(bytes, static_cast<std::int32_t>(lump.bytes.size()));
      bytes.push_back(static_cast<std::uint8_t>(lump.type));
      bytes.push_back(lump.compression);
      bytes.push_back(0);
      bytes.push_back(0);
      for (std::size_t letter = 0; letter < 16; letter++)
      {
        bytes.push_back(letter < lump.name.size() ? static_cast<std::uint8_t>(lump.name[letter]) : 0);
      }
    }
    return bytes;
  }

  /// Where the entry of a lump starts in the bytes make_wad() made.
  std::size_t entry_at(const std::vector<std::uint8_t> &bytes, const std::size_t lump)
  {
    const std::size_t count = bytes[4];
    return bytes.size() - (count - lump) * 32;
  }

  /// A wad of three lumps: a picture, a palette of three bytes, a texture.
  std::vector<std::uint8_t> make_usual_wad()
  {
    return make_wad({
      {.name = "SBAR", .type = 'B', .bytes = make_picture(2, 2, {1, 2, 3, 255})},
      {.name = "palette", .type = '@', .bytes = {7, 8, 9}},
      {.name = "wall", .type = 'D', .bytes = {4, 5, 6, 7, 8}},
    });
  }

  /// A file of the data to test with on this machine, or nothing when it is
  /// not there.
  std::vector<std::uint8_t> read_test_data(const std::string &name)
  {
    std::ifstream file(std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / name, std::ios::binary);
    if (!file) { return {}; }

    const std::vector<char> content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return {content.begin(), content.end()};
  }

  TEST(WadTest, ListsItsLumpsInTheOrderOfTheDirectory)
  {
    const auto bytes = make_usual_wad();

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;
    EXPECT_TRUE(error.empty());

    const auto &lumps = wad.GetLumps();
    ASSERT_EQ(lumps.size(), 3u);

    EXPECT_EQ(lumps[0].name, "SBAR");
    EXPECT_EQ(lumps[0].type, WadLumpType::StatusBarPicture);
    EXPECT_EQ(lumps[0].offset, 12u);
    EXPECT_EQ(lumps[0].size_on_disk, 12u);
    EXPECT_EQ(lumps[0].size, 12u);
    EXPECT_EQ(lumps[0].compression, 0);

    EXPECT_EQ(lumps[1].name, "palette");
    EXPECT_EQ(lumps[1].type, WadLumpType::Palette);
    EXPECT_EQ(lumps[1].offset, 24u);
    EXPECT_EQ(lumps[1].size_on_disk, 3u);

    EXPECT_EQ(lumps[2].name, "wall");
    EXPECT_EQ(lumps[2].type, WadLumpType::MipTexture);
  }

  TEST(WadTest, SaysWhatEachLumpHolds)
  {
    const auto bytes = make_wad({
      {.name = "a", .type = '@', .bytes = {}},
      {.name = "b", .type = 'B', .bytes = {}},
      {.name = "d", .type = 'D', .bytes = {}},
      {.name = "e", .type = 'E', .bytes = {}},
      {.name = "z", .type = 'Z', .bytes = {}},
    });

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    const auto &lumps = wad.GetLumps();
    ASSERT_EQ(lumps.size(), 5u);
    EXPECT_EQ(lumps[0].DescribeType(), "palette");
    EXPECT_EQ(lumps[1].DescribeType(), "status bar picture");
    EXPECT_EQ(lumps[2].DescribeType(), "mip texture");
    EXPECT_EQ(lumps[3].type, WadLumpType::ConsolePicture);
    EXPECT_EQ(lumps[3].DescribeType(), "console picture");

    // a letter the game does not know is kept as it is
    EXPECT_EQ(static_cast<char>(lumps[4].type), 'Z');
    EXPECT_EQ(lumps[4].DescribeType(), "unknown");
  }

  TEST(WadTest, FindsALumpByItsNameWhateverTheCase)
  {
    const auto bytes = make_usual_wad();

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    const WadLump *lump = wad.Find("sbar");
    ASSERT_NE(lump, nullptr);
    EXPECT_EQ(lump, &wad.GetLumps()[0]);
    EXPECT_EQ(wad.Find("SBAR"), lump);
    EXPECT_EQ(wad.Find("Palette"), &wad.GetLumps()[1]);

    EXPECT_EQ(wad.Find("sba"), nullptr);
    EXPECT_EQ(wad.Find("sbar2"), nullptr);
    EXPECT_EQ(wad.Find(""), nullptr);
  }

  TEST(WadTest, FindsTheFirstOfLumpsWithOneNameAndANameOfAllSixteenBytes)
  {
    const auto bytes = make_wad({
      {.name = "twice", .type = '@', .bytes = {1}},
      {.name = "TWICE", .type = '@', .bytes = {2}},
      {.name = "sixteen_letters_", .type = '@', .bytes = {3}},
    });

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    EXPECT_EQ(wad.Find("twice"), &wad.GetLumps()[0]);

    const WadLump *full = wad.Find("sixteen_letters_");
    ASSERT_NE(full, nullptr);
    EXPECT_THAT(wad.GetBytes(*full), ElementsAre(3));
  }

  TEST(WadTest, HandsOutTheBytesOfALumpWithoutCopyingThem)
  {
    const auto bytes = make_usual_wad();

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    const WadLump *texture = wad.Find("wall");
    ASSERT_NE(texture, nullptr);

    // a mip texture is not read here, and its bytes are there for who does
    const auto lump_bytes = wad.GetBytes(*texture);
    EXPECT_THAT(lump_bytes, ElementsAre(4, 5, 6, 7, 8));
    EXPECT_EQ(lump_bytes.data(), bytes.data() + texture->offset);
  }

  TEST(WadTest, HandsOutNothingForALumpThatIsNotOfThisWad)
  {
    const auto bytes = make_usual_wad();

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    WadLump elsewhere;
    elsewhere.offset = bytes.size() - 1;
    elsewhere.size_on_disk = 2;
    EXPECT_TRUE(wad.GetBytes(elsewhere).empty());

    elsewhere.offset = 4;
    elsewhere.size_on_disk = static_cast<std::size_t>(-1);
    EXPECT_TRUE(wad.GetBytes(elsewhere).empty());

    const Wad never_read;
    EXPECT_TRUE(never_read.GetLumps().empty());
    EXPECT_EQ(never_read.Find("sbar"), nullptr);
    EXPECT_TRUE(never_read.GetBytes(elsewhere).empty());
  }

  TEST(WadTest, ReadsAWadWithoutLumps)
  {
    const auto bytes = make_wad({});
    ASSERT_EQ(bytes.size(), 12u);

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;
    EXPECT_TRUE(wad.GetLumps().empty());
  }

  TEST(WadTest, ReadsAPictureOfTheStatusBarThroughThePictureReader)
  {
    const auto bytes = make_usual_wad();

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    Picture picture;
    ASSERT_TRUE(wad.ReadPicture("sbar", picture, error)) << error;
    EXPECT_EQ(picture.width, 2);
    EXPECT_EQ(picture.height, 2);
    EXPECT_THAT(picture.pixels, ElementsAre(1, 2, 3, 255));
  }

  TEST(WadTest, ReadsTheLettersOfTheConsoleByTheirNameAsPixelsAlone)
  {
    std::vector<std::uint8_t> letters(128 * 128, 0);
    letters[0] = 5;
    letters[128 * 128 - 1] = 6;

    // the wad of the game says that they are a mip texture
    const auto bytes = make_wad({{.name = "CONCHARS", .type = 'D', .bytes = letters}});

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    Picture picture;
    ASSERT_TRUE(wad.ReadPicture("conchars", picture, error)) << error;
    EXPECT_EQ(picture.width, 128);
    EXPECT_EQ(picture.height, 128);
    ASSERT_EQ(picture.pixels.size(), letters.size());
    EXPECT_EQ(picture.pixels.front(), 5);
    EXPECT_EQ(picture.pixels.back(), 6);
  }

  TEST(WadTest, RefusesLettersOfTheConsoleThatAreNotOfTheirSize)
  {
    const auto bytes = make_wad({{.name = "conchars", .type = 'D', .bytes = std::vector<std::uint8_t>(100)}});

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    Picture picture;
    EXPECT_FALSE(wad.ReadPicture("conchars", picture, error));
    EXPECT_THAT(error, HasSubstr("`conchars`"));
    EXPECT_THAT(error, HasSubstr("there are 100"));
  }

  TEST(WadTest, RefusesToReadAsAPictureWhatIsNotThereOrIsNotOne)
  {
    auto lumps = std::vector<TestLump>{
      {.name = "wall", .type = 'D', .bytes = make_picture(1, 1, {1})},
      {.name = "palette", .type = '@', .bytes = make_picture(1, 1, {1})},
      {.name = "console", .type = 'E', .bytes = make_picture(1, 1, {1})},
      {.name = "broken", .type = 'B', .bytes = make_picture(4, 4, {1, 2, 3})},
      {.name = "short", .type = 'B', .bytes = {1, 2, 3}},
      {.name = "packed", .type = 'B', .bytes = make_picture(1, 1, {1}), .compression = 1},
    };
    const auto bytes = make_wad(lumps);

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    Picture picture{.width = 7, .height = 9, .pixels = {42}};

    EXPECT_FALSE(wad.ReadPicture("missing", picture, error));
    EXPECT_THAT(error, HasSubstr("no lump `missing`"));

    EXPECT_FALSE(wad.ReadPicture("wall", picture, error));
    EXPECT_THAT(error, HasSubstr("mip texture"));

    EXPECT_FALSE(wad.ReadPicture("palette", picture, error));
    EXPECT_THAT(error, HasSubstr("palette"));

    EXPECT_FALSE(wad.ReadPicture("console", picture, error));
    EXPECT_THAT(error, HasSubstr("console picture"));

    EXPECT_FALSE(wad.ReadPicture("broken", picture, error));
    EXPECT_THAT(error, HasSubstr("`broken`"));
    EXPECT_THAT(error, HasSubstr("has 16 bytes of them, and there are 3"));

    EXPECT_FALSE(wad.ReadPicture("short", picture, error));
    EXPECT_THAT(error, HasSubstr("there are 3"));

    EXPECT_FALSE(wad.ReadPicture("packed", picture, error));
    EXPECT_THAT(error, HasSubstr("packed"));

    EXPECT_EQ(picture.width, 7);
    EXPECT_EQ(picture.height, 9);
    EXPECT_THAT(picture.pixels, ElementsAre(42));
  }

  TEST(WadTest, RefusesBytesTooFewForTheStart)
  {
    Wad wad;
    std::string error;

    EXPECT_FALSE(wad.Read({}, error));
    EXPECT_THAT(error, HasSubstr("there are 0"));

    const std::vector<std::uint8_t> bytes = {'W', 'A', 'D', '2', 0, 0, 0, 0, 12, 0, 0};
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("there are 11"));
  }

  TEST(WadTest, RefusesAFileThatDoesNotStartAsAWad)
  {
    auto bytes = make_usual_wad();
    bytes[3] = '3';

    Wad wad;
    std::string error;
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("WAD2"));

    // the archive of the game's other files starts with other letters
    bytes[0] = 'P';
    bytes[1] = 'A';
    bytes[2] = 'C';
    bytes[3] = 'K';
    EXPECT_FALSE(wad.Read(bytes, error));
  }

  TEST(WadTest, RefusesANumberOfLumpsLessThanNothing)
  {
    auto bytes = make_usual_wad();
    set_i32(bytes, 4, -1);

    Wad wad;
    std::string error;
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("-1 lumps"));
  }

  TEST(WadTest, RefusesADirectoryThatIsNotAllInsideTheFile)
  {
    const auto good = make_usual_wad();
    Wad wad;
    std::string error;

    // one lump more than there are entries for
    auto bytes = good;
    set_i32(bytes, 4, 4);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("directory"));

    // so many lumps that their entries, counted in 32 bits, would wrap around
    bytes = good;
    set_i32(bytes, 4, 0x7fffffff);
    EXPECT_FALSE(wad.Read(bytes, error));

    bytes = good;
    set_i32(bytes, 4, 0x08000000);
    EXPECT_FALSE(wad.Read(bytes, error));

    // a directory that starts past the end, or before the start
    bytes = good;
    set_i32(bytes, 8, static_cast<std::int32_t>(bytes.size()) + 1);
    EXPECT_FALSE(wad.Read(bytes, error));

    bytes = good;
    set_i32(bytes, 8, 0x7fffffff);
    EXPECT_FALSE(wad.Read(bytes, error));

    bytes = good;
    set_i32(bytes, 8, -32);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("at byte -32"));

    // a file cut short in its last entry
    bytes = good;
    bytes.pop_back();
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("directory"));
  }

  TEST(WadTest, RefusesALumpThatIsNotAllInsideTheFile)
  {
    const auto good = make_usual_wad();
    const std::size_t entry = entry_at(good, 1);
    Wad wad;
    std::string error;

    // one byte more than the file has after the place of the lump
    auto bytes = good;
    set_i32(bytes, entry + 4, static_cast<std::int32_t>(bytes.size()) - 24 + 1);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("`palette`"));
    EXPECT_THAT(error, HasSubstr("not inside"));

    // a lump that ends exactly where the file does is inside it
    bytes = good;
    set_i32(bytes, entry + 4, static_cast<std::int32_t>(bytes.size()) - 24);
    EXPECT_TRUE(wad.Read(bytes, error)) << error;

    bytes = good;
    set_i32(bytes, entry, static_cast<std::int32_t>(bytes.size()) + 1);
    EXPECT_FALSE(wad.Read(bytes, error));

    // a place and a size that, added in 32 bits, would wrap around
    bytes = good;
    set_i32(bytes, entry, 0x7fffffff);
    set_i32(bytes, entry + 4, 0x7fffffff);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("not inside"));
  }

  TEST(WadTest, RefusesALumpWithAPlaceOrASizeLessThanNothing)
  {
    const auto good = make_usual_wad();
    const std::size_t entry = entry_at(good, 2);
    Wad wad;
    std::string error;

    auto bytes = good;
    set_i32(bytes, entry, -1);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("`wall`"));
    EXPECT_THAT(error, HasSubstr("less than nothing"));

    bytes = good;
    set_i32(bytes, entry + 4, -5);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("-5 bytes"));

    bytes = good;
    set_i32(bytes, entry + 8, -5);
    EXPECT_FALSE(wad.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("-5 unpacked"));
  }

  TEST(WadTest, StaysAsItWasWhenARefusedReadFollowsAGoodOne)
  {
    const auto good = make_usual_wad();
    auto broken = good;
    broken[0] = 'X';

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(good, error)) << error;
    EXPECT_FALSE(wad.Read(broken, error));

    ASSERT_EQ(wad.GetLumps().size(), 3u);
    const WadLump *lump = wad.Find("palette");
    ASSERT_NE(lump, nullptr);
    EXPECT_EQ(wad.GetBytes(*lump).data(), good.data() + lump->offset);
  }

  TEST(WadTest, ReadsTheWadOfRealData)
  {
    const auto bytes = read_test_data("gfx.wad");
    if (bytes.empty()) { GTEST_SKIP() << "no gfx.wad in " << QUAKE_TEST_DATA_DIRECTORY; }

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;
    ASSERT_FALSE(wad.GetLumps().empty());

    Palette palette;
    const bool has_palette = palette.Read(read_test_data("gfx/palette.lmp"));

    // every lump is of a kind the game knows, and every picture of the
    // status bar is read as one
    int pictures = 0;
    for (const WadLump &lump : wad.GetLumps())
    {
      EXPECT_EQ(wad.GetBytes(lump).size(), lump.size_on_disk) << lump.name;
      EXPECT_EQ(lump.compression, 0) << lump.name;
      EXPECT_NE(lump.DescribeType(), "unknown") << lump.name;

      if (lump.type != WadLumpType::StatusBarPicture) { continue; }

      Picture picture;
      ASSERT_TRUE(wad.ReadPicture(lump.name, picture, error)) << lump.name << ": " << error;
      EXPECT_GT(picture.width, 0) << lump.name;
      EXPECT_GT(picture.height, 0) << lump.name;
      pictures++;

      if (has_palette)
      {
        EXPECT_EQ(palette.ToRgba(picture.pixels, true).size(), picture.pixels.size() * 4) << lump.name;
      }
    }
    EXPECT_GT(pictures, 0);
  }

  TEST(WadTest, ReadsTheStatusBarAndTheLettersOfTheConsoleOfRealData)
  {
    const auto bytes = read_test_data("gfx.wad");
    if (bytes.empty()) { GTEST_SKIP() << "no gfx.wad in " << QUAKE_TEST_DATA_DIRECTORY; }

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(bytes, error)) << error;

    Picture status_bar;
    ASSERT_TRUE(wad.ReadPicture("sbar", status_bar, error)) << error;
    EXPECT_EQ(status_bar.width, 320);
    EXPECT_EQ(status_bar.height, 24);

    const WadLump *letters_lump = wad.Find("conchars");
    ASSERT_NE(letters_lump, nullptr);
    EXPECT_EQ(letters_lump->type, WadLumpType::MipTexture);

    Picture letters;
    ASSERT_TRUE(wad.ReadPicture("conchars", letters, error)) << error;
    EXPECT_EQ(letters.width, 128);
    EXPECT_EQ(letters.height, 128);
    EXPECT_EQ(letters.pixels.size(), 16384u);
  }
}
