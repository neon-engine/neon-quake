#include "palette.hpp"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::Palette;
  using ::testing::ElementsAre;

  /// A palette whose colour `i` is red i, green 255 - i, blue i / 2.
  std::vector<std::uint8_t> MakeBytes()
  {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < 256; i++)
    {
      bytes.push_back(static_cast<std::uint8_t>(i));
      bytes.push_back(static_cast<std::uint8_t>(255 - i));
      bytes.push_back(static_cast<std::uint8_t>(i / 2));
    }
    return bytes;
  }

  TEST(PaletteTest, TakesItsColoursFromTheBytesOfTheFile)
  {
    Palette palette;
    ASSERT_TRUE(palette.Read(MakeBytes()));

    EXPECT_THAT(palette.GetColour(0), ElementsAre(0, 255, 0));
    EXPECT_THAT(palette.GetColour(10), ElementsAre(10, 245, 5));
    EXPECT_THAT(palette.GetColour(255), ElementsAre(255, 0, 127));
  }

  TEST(PaletteTest, RefusesBytesThatAreNotAPaletteAndStaysAsItWas)
  {
    Palette palette;
    ASSERT_TRUE(palette.Read(MakeBytes()));

    std::vector<std::uint8_t> short_by_one = MakeBytes();
    short_by_one.pop_back();
    EXPECT_FALSE(palette.Read(short_by_one));
    EXPECT_FALSE(palette.Read({}));

    EXPECT_THAT(palette.GetColour(10), ElementsAre(10, 245, 5));
  }

  TEST(PaletteTest, MakesFourBytesOfEveryPixelAllOfThemSolid)
  {
    Palette palette;
    ASSERT_TRUE(palette.Read(MakeBytes()));

    const std::vector<std::uint8_t> pixels = {0, 10, 255};
    EXPECT_THAT(palette.ToRgba(pixels), ElementsAre(0, 255, 0, 255, 10, 245, 5, 255, 255, 0, 127, 255));
  }

  TEST(PaletteTest, LeavesAHoleWhereAPictureWithHolesHasTheLastColour)
  {
    Palette palette;
    ASSERT_TRUE(palette.Read(MakeBytes()));

    const std::vector<std::uint8_t> pixels = {10, Palette::see_through};
    EXPECT_THAT(palette.ToRgba(pixels, true), ElementsAre(10, 245, 5, 255, 0, 0, 0, 0));
  }

  TEST(PaletteTest, MakesNothingOfNoPixels)
  {
    const Palette palette;
    EXPECT_TRUE(palette.ToRgba({}).empty());
  }
}
