#include "picture-reader.hpp"

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
  using quake::read_picture;
  using quake::read_picture_file;
  using quake::read_raw_picture;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  /// Adds a number of four bytes, the lowest first.
  void add_i32(std::vector<std::uint8_t> &bytes, const std::int32_t value)
  {
    const auto bits = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8)
    {
      bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
  }

  /// The bytes of a picture file: the width, the height, and the pixels as
  /// they are given, however many there are.
  std::vector<std::uint8_t> make_file(
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

  /// A palette whose colour `i` is red i, green 255 - i, blue i / 2.
  Palette make_palette()
  {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < 256; i++)
    {
      bytes.push_back(static_cast<std::uint8_t>(i));
      bytes.push_back(static_cast<std::uint8_t>(255 - i));
      bytes.push_back(static_cast<std::uint8_t>(i / 2));
    }

    Palette palette;
    EXPECT_TRUE(palette.Read(bytes));
    return palette;
  }

  /// A picture that tells whether a refused read left it as it was.
  Picture make_untouched()
  {
    return Picture{.width = 7, .height = 9, .pixels = {42}};
  }

  void expect_untouched(const Picture &picture)
  {
    EXPECT_EQ(picture.width, 7);
    EXPECT_EQ(picture.height, 9);
    EXPECT_THAT(picture.pixels, ElementsAre(42));
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

  TEST(PictureReaderTest, ReadsTheSizeAndThePixelsOfAPicture)
  {
    const auto bytes = make_file(3, 2, {1, 2, 3, 4, 5, 6});

    Picture picture;
    std::string error;
    ASSERT_TRUE(read_picture(bytes, picture, error)) << error;

    EXPECT_EQ(picture.width, 3);
    EXPECT_EQ(picture.height, 2);
    EXPECT_THAT(picture.pixels, ElementsAre(1, 2, 3, 4, 5, 6));
    EXPECT_TRUE(error.empty());
  }

  TEST(PictureReaderTest, GivesPixelsThePaletteTurnsIntoColoursWithHoles)
  {
    const auto bytes = make_file(2, 1, {10, Palette::see_through});

    Picture picture;
    std::string error;
    ASSERT_TRUE(read_picture(bytes, picture, error)) << error;

    EXPECT_THAT(make_palette().ToRgba(picture.pixels, true), ElementsAre(10, 245, 5, 255, 0, 0, 0, 0));
  }

  TEST(PictureReaderTest, RefusesBytesTooFewForTheWidthAndHeight)
  {
    const std::vector<std::uint8_t> bytes = {3, 0, 0, 0, 2, 0, 0};

    Picture picture = make_untouched();
    std::string error;
    EXPECT_FALSE(read_picture(bytes, picture, error));
    EXPECT_THAT(error, HasSubstr("there are 7"));
    expect_untouched(picture);

    EXPECT_FALSE(read_picture({}, picture, error));
    EXPECT_THAT(error, HasSubstr("there are 0"));
  }

  TEST(PictureReaderTest, RefusesAPictureWithFewerOrMoreBytesThanItsSize)
  {
    Picture picture = make_untouched();
    std::string error;

    EXPECT_FALSE(read_picture(make_file(3, 2, {1, 2, 3, 4, 5}), picture, error));
    EXPECT_THAT(error, HasSubstr("has 6 bytes of them, and there are 5"));

    EXPECT_FALSE(read_picture(make_file(3, 2, {1, 2, 3, 4, 5, 6, 7}), picture, error));
    EXPECT_THAT(error, HasSubstr("has 6 bytes of them, and there are 7"));

    expect_untouched(picture);
  }

  TEST(PictureReaderTest, RefusesASideThatIsNothingOrLessThanNothing)
  {
    Picture picture = make_untouched();
    std::string error;

    EXPECT_FALSE(read_picture(make_file(0, 0, {}), picture, error));
    EXPECT_THAT(error, HasSubstr("0 by 0"));

    EXPECT_FALSE(read_picture(make_file(0, 4, {}), picture, error));
    EXPECT_FALSE(read_picture(make_file(4, 0, {}), picture, error));

    EXPECT_FALSE(read_picture(make_file(-1, 4, {1, 2, 3, 4}), picture, error));
    EXPECT_THAT(error, HasSubstr("-1 by 4"));

    // two sides less than nothing, whose product is the number of bytes
    EXPECT_FALSE(read_picture(make_file(-2, -2, {1, 2, 3, 4}), picture, error));
    EXPECT_THAT(error, HasSubstr("-2 by -2"));

    expect_untouched(picture);
  }

  TEST(PictureReaderTest, RefusesSidesSoLargeThatTheirProductWouldWrapAround)
  {
    Picture picture = make_untouched();
    std::string error;

    // 65536 times 65536 is nothing in a number of 32 bits
    EXPECT_FALSE(read_picture(make_file(65536, 65536, {}), picture, error));
    EXPECT_THAT(error, HasSubstr("65536 by 65536"));

    // 65537 times 65537 wraps around to 131073 in a number of 32 bits
    EXPECT_FALSE(read_picture(make_file(65537, 65537, std::vector<std::uint8_t>(131073)), picture, error));

    EXPECT_FALSE(read_picture(make_file(0x7fffffff, 0x7fffffff, {1}), picture, error));
    EXPECT_FALSE(read_picture(make_file(Picture::max_side + 1, 1, {}), picture, error));

    // the largest side there may be is believed, and then lacks its bytes
    EXPECT_FALSE(read_picture(make_file(Picture::max_side, Picture::max_side, {1}), picture, error));
    EXPECT_THAT(error, HasSubstr("there are 1"));

    expect_untouched(picture);
  }

  TEST(PictureReaderTest, ReadsAPictureWithoutWidthAndHeightFromAKnownSize)
  {
    const std::vector<std::uint8_t> bytes = {1, 2, 3, 4, 5, 6};

    Picture picture;
    std::string error;
    ASSERT_TRUE(read_raw_picture(bytes, 2, 3, picture, error)) << error;

    EXPECT_EQ(picture.width, 2);
    EXPECT_EQ(picture.height, 3);
    EXPECT_THAT(picture.pixels, ElementsAre(1, 2, 3, 4, 5, 6));
  }

  TEST(PictureReaderTest, RefusesAPictureWithoutWidthAndHeightThatIsNotOfTheKnownSize)
  {
    const std::vector<std::uint8_t> bytes = {1, 2, 3, 4, 5, 6};

    Picture picture = make_untouched();
    std::string error;

    EXPECT_FALSE(read_raw_picture(bytes, 2, 2, picture, error));
    EXPECT_THAT(error, HasSubstr("has 4 bytes of them, and there are 6"));

    EXPECT_FALSE(read_raw_picture(bytes, 4, 2, picture, error));
    EXPECT_FALSE(read_raw_picture(bytes, 0, 6, picture, error));
    EXPECT_FALSE(read_raw_picture(bytes, -2, -3, picture, error));
    EXPECT_FALSE(read_raw_picture(bytes, 65536, 65536, picture, error));
    EXPECT_FALSE(read_raw_picture({}, 0, 0, picture, error));

    expect_untouched(picture);
  }

  TEST(PictureReaderTest, ReadsAFileOfThePicturesByItsPath)
  {
    const auto bytes = make_file(1, 2, {8, 9});

    Picture picture;
    std::string error;
    ASSERT_TRUE(read_picture_file("gfx/qplaque.lmp", bytes, picture, error)) << error;
    EXPECT_THAT(picture.pixels, ElementsAre(8, 9));

    EXPECT_FALSE(read_picture_file("gfx/qplaque.lmp", make_file(1, 2, {8}), picture, error));
  }

  TEST(PictureReaderTest, SaysThatThePaletteAndTheOtherTablesAreNotPictures)
  {
    // even bytes that could be read as a picture are not one under these names
    const auto bytes = make_file(1, 2, {8, 9});

    Picture picture = make_untouched();
    std::string error;

    EXPECT_FALSE(read_picture_file("gfx/palette.lmp", bytes, picture, error));
    EXPECT_THAT(error, HasSubstr("gfx/palette.lmp is not a picture"));
    EXPECT_THAT(error, HasSubstr("palette"));

    EXPECT_FALSE(read_picture_file("gfx/colormap.lmp", bytes, picture, error));
    EXPECT_THAT(error, HasSubstr("gfx/colormap.lmp is not a picture"));

    EXPECT_FALSE(read_picture_file("gfx/pop.lmp", bytes, picture, error));
    EXPECT_THAT(error, HasSubstr("gfx/pop.lmp is not a picture"));

    EXPECT_FALSE(read_picture_file("GFX/Palette.LMP", bytes, picture, error));
    EXPECT_THAT(error, HasSubstr("is not a picture"));

    expect_untouched(picture);
  }

  TEST(PictureReaderTest, ReadsEveryPictureOfRealData)
  {
    const std::filesystem::path folder = std::filesystem::path(QUAKE_TEST_DATA_DIRECTORY) / "gfx";
    std::error_code not_there;
    if (!std::filesystem::is_directory(folder, not_there)) { GTEST_SKIP() << "no data at " << folder; }

    Palette palette;
    const bool has_palette = palette.Read(read_test_data("gfx/palette.lmp"));

    int pictures = 0;
    for (const auto &entry : std::filesystem::directory_iterator(folder))
    {
      if (entry.path().extension() != ".lmp") { continue; }

      const std::string path = "gfx/" + entry.path().filename().string();
      const auto bytes = read_test_data(path);

      Picture picture;
      std::string error;
      if (path == "gfx/palette.lmp" || path == "gfx/colormap.lmp" || path == "gfx/pop.lmp")
      {
        EXPECT_FALSE(read_picture_file(path, bytes, picture, error)) << path;
        EXPECT_THAT(error, HasSubstr("is not a picture"));

        // and what they hold does not pass for a picture without the name
        EXPECT_FALSE(read_picture(bytes, picture, error)) << path;
        continue;
      }

      ASSERT_TRUE(read_picture_file(path, bytes, picture, error)) << path << ": " << error;
      const auto count = static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height);
      EXPECT_EQ(picture.pixels.size(), count) << path;
      pictures++;

      if (!has_palette) { continue; }

      // a hole wherever the picture has the last colour, and nowhere else
      const auto rgba = palette.ToRgba(picture.pixels, true);
      ASSERT_EQ(rgba.size(), count * 4) << path;
      for (std::size_t i = 0; i < count; i++)
      {
        const bool hole = picture.pixels[i] == Palette::see_through;
        ASSERT_EQ(rgba[i * 4 + 3], hole ? 0 : 255) << path << " at pixel " << i;
      }
    }
    EXPECT_GT(pictures, 0);
  }

  TEST(PictureReaderTest, ReadsTheBackOfTheConsoleOfRealData)
  {
    const auto bytes = read_test_data("gfx/conback.lmp");
    if (bytes.empty()) { GTEST_SKIP() << "no gfx/conback.lmp in " << QUAKE_TEST_DATA_DIRECTORY; }

    Picture picture;
    std::string error;
    ASSERT_TRUE(read_picture_file("gfx/conback.lmp", bytes, picture, error)) << error;
    EXPECT_EQ(picture.width, 320);
    EXPECT_EQ(picture.height, 200);
    EXPECT_EQ(picture.pixels.size(), 64000u);
  }
}
