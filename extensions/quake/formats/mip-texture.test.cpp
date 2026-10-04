#include "mip-texture.hpp"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::MipTexture;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;

  void PutU32(std::vector<std::uint8_t> &bytes, const std::uint32_t value)
  {
    for (int shift = 0; shift < 32; shift += 8) { bytes.push_back(static_cast<std::uint8_t>(value >> shift)); }
  }

  void SetU32(std::vector<std::uint8_t> &bytes, const std::size_t at, const std::uint32_t value)
  {
    for (int i = 0; i < 4; i++) { bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i)); }
  }

  /// The bytes of a texture whose four sizes follow its header, one after
  /// the other. The pixels of size `level` all have the value `level + 1`,
  /// but for the first pixel of the full size, which is 9.
  std::vector<std::uint8_t> MakeBytes(const std::string &name, const std::uint32_t width, const std::uint32_t height)
  {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < 16; i++) { bytes.push_back(i < name.size() ? static_cast<std::uint8_t>(name[i]) : 0); }
    PutU32(bytes, width);
    PutU32(bytes, height);

    std::uint32_t offset = 40;
    for (int level = 0; level < 4; level++)
    {
      PutU32(bytes, offset);
      offset += (width >> level) * (height >> level);
    }
    for (int level = 0; level < 4; level++)
    {
      bytes.insert(bytes.end(), (width >> level) * (height >> level), static_cast<std::uint8_t>(level + 1));
    }
    bytes[40] = 9;
    return bytes;
  }

  MipTexture Named(const std::string &name)
  {
    MipTexture texture;
    texture.name = name;
    return texture;
  }

  TEST(MipTextureTest, ReadsItsNameItsSizeAndItsFourPictures)
  {
    MipTexture texture;
    std::string error;
    ASSERT_TRUE(texture.Read(MakeBytes("wall1", 16, 8), error)) << error;

    EXPECT_EQ(texture.name, "wall1");
    EXPECT_EQ(texture.width, 16u);
    EXPECT_EQ(texture.height, 8u);
    ASSERT_EQ(texture.pixels[0].size(), 128u);
    EXPECT_EQ(texture.pixels[0][0], 9);
    EXPECT_EQ(texture.pixels[0][127], 1);
    EXPECT_EQ(texture.pixels[1].size(), 32u);
    EXPECT_EQ(texture.pixels[1][0], 2);
    EXPECT_EQ(texture.pixels[2].size(), 8u);
    EXPECT_THAT(texture.pixels[3], ElementsAre(4, 4));
  }

  TEST(MipTextureTest, ReadsFromBytesThatGoOnPastItsEnd)
  {
    std::vector<std::uint8_t> bytes = MakeBytes("wall1", 16, 16);
    bytes.insert(bytes.end(), 100, 0xee);

    MipTexture texture;
    std::string error;
    ASSERT_TRUE(texture.Read(bytes, error)) << error;
    EXPECT_EQ(texture.pixels[3].size(), 4u);
    EXPECT_EQ(texture.pixels[3][3], 4);
  }

  TEST(MipTextureTest, LeavesASmallerSizeEmptyWhenTheFileDoesNotHaveIt)
  {
    std::vector<std::uint8_t> bytes = MakeBytes("wall1", 16, 16);
    SetU32(bytes, 28, 0);
    SetU32(bytes, 32, 0);
    SetU32(bytes, 36, 0);
    bytes.resize(40 + 256);

    MipTexture texture;
    std::string error;
    ASSERT_TRUE(texture.Read(bytes, error)) << error;
    EXPECT_EQ(texture.pixels[0].size(), 256u);
    EXPECT_TRUE(texture.pixels[1].empty());
    EXPECT_TRUE(texture.pixels[2].empty());
    EXPECT_TRUE(texture.pixels[3].empty());
  }

  TEST(MipTextureTest, RefusesBytesShorterThanItsHeaderAndStaysAsItWas)
  {
    MipTexture texture;
    std::string error;
    ASSERT_TRUE(texture.Read(MakeBytes("wall1", 16, 16), error)) << error;

    std::vector<std::uint8_t> bytes = MakeBytes("other", 32, 32);
    bytes.resize(39);
    EXPECT_FALSE(texture.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("39 bytes"));

    EXPECT_EQ(texture.name, "wall1");
    EXPECT_EQ(texture.pixels[0].size(), 256u);
  }

  TEST(MipTextureTest, RefusesATextureWithoutAWidthOrAHeight)
  {
    MipTexture texture;
    std::string error;

    EXPECT_FALSE(texture.Read(MakeBytes("flat", 0, 16), error));
    EXPECT_THAT(error, HasSubstr("\"flat\" is 0 by 16"));

    EXPECT_FALSE(texture.Read(MakeBytes("flat", 16, 0), error));
    EXPECT_THAT(error, HasSubstr("\"flat\" is 16 by 0"));
  }

  TEST(MipTextureTest, RefusesAPictureThatLiesOutsideTheBytes)
  {
    MipTexture texture;
    std::string error;

    // the smallest size is cut short by one byte
    std::vector<std::uint8_t> cut = MakeBytes("wall1", 16, 16);
    cut.pop_back();
    EXPECT_FALSE(texture.Read(cut, error));
    EXPECT_THAT(error, HasSubstr("size 3 of \"wall1\""));

    // the full size starts where nothing is
    std::vector<std::uint8_t> far = MakeBytes("wall1", 16, 16);
    SetU32(far, 24, 0xfffffff0u);
    EXPECT_FALSE(texture.Read(far, error));
    EXPECT_THAT(error, HasSubstr("size 0 of \"wall1\""));
  }

  TEST(MipTextureTest, RefusesASizeWhoseCountOfPixelsNoFileCouldHold)
  {
    std::vector<std::uint8_t> bytes = MakeBytes("huge", 16, 16);
    SetU32(bytes, 16, 0xffffffffu);
    SetU32(bytes, 20, 0xffffffffu);

    MipTexture texture;
    std::string error;
    EXPECT_FALSE(texture.Read(bytes, error));
    EXPECT_THAT(error, HasSubstr("\"huge\""));
  }

  TEST(MipTextureTest, TellsALiquidTheSkyAndHolesByTheName)
  {
    EXPECT_TRUE(Named("*water1").IsLiquid());
    EXPECT_TRUE(Named("*lava1").IsLiquid());
    EXPECT_FALSE(Named("water1").IsLiquid());

    EXPECT_TRUE(Named("sky4").IsSky());
    EXPECT_TRUE(Named("SKY1").IsSky());
    EXPECT_FALSE(Named("sk").IsSky());
    EXPECT_FALSE(Named("*sky").IsSky());

    EXPECT_TRUE(Named("{fence").HasHoles());
    EXPECT_FALSE(Named("fence").HasHoles());

    EXPECT_FALSE(Named("").IsLiquid());
    EXPECT_FALSE(Named("").IsSky());
    EXPECT_FALSE(Named("").HasHoles());
  }

  TEST(MipTextureTest, TellsTheFrameOfAnAnimationByTheName)
  {
    EXPECT_EQ(Named("+0button").GetAnimationFrame(), 0);
    EXPECT_EQ(Named("+9button").GetAnimationFrame(), 9);
    EXPECT_FALSE(Named("+3button").IsAlternateAnimation());

    EXPECT_EQ(Named("+abutton").GetAnimationFrame(), 0);
    EXPECT_EQ(Named("+Jbutton").GetAnimationFrame(), 9);
    EXPECT_TRUE(Named("+abutton").IsAlternateAnimation());

    EXPECT_EQ(Named("button").GetAnimationFrame(), -1);
    EXPECT_EQ(Named("+").GetAnimationFrame(), -1);
    EXPECT_EQ(Named("+kbutton").GetAnimationFrame(), -1);
    EXPECT_FALSE(Named("button").IsAlternateAnimation());
    EXPECT_FALSE(Named("+kbutton").IsAlternateAnimation());
  }
}
