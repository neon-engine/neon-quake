#include "byte-reader.hpp"

#include <array>
#include <vector>

#include <gtest/gtest.h>

namespace
{
  using quake::ByteReader;

  TEST(ByteReaderTest, ReadsNumbersWithTheLowestByteFirst)
  {
    const std::vector<std::uint8_t> bytes = {
      0x7f,
      0x34, 0x12,
      0xff, 0xff,
      0x78, 0x56, 0x34, 0x12,
      0xfe, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x80, 0x3f,
    };
    ByteReader reader(bytes);

    EXPECT_EQ(reader.ReadU8(), 0x7f);
    EXPECT_EQ(reader.ReadU16(), 0x1234);
    EXPECT_EQ(reader.ReadI16(), -1);
    EXPECT_EQ(reader.ReadU32(), 0x12345678u);
    EXPECT_EQ(reader.ReadI32(), -2);
    EXPECT_EQ(reader.ReadF32(), 1.0f);
    EXPECT_TRUE(reader.IsGood());
    EXPECT_EQ(reader.GetPosition(), bytes.size());
  }

  TEST(ByteReaderTest, GivesZeroAndRemembersAReadPastTheEnd)
  {
    const std::vector<std::uint8_t> bytes = {0x01, 0x02, 0x03};
    ByteReader reader(bytes);

    EXPECT_EQ(reader.ReadU16(), 0x0201);
    EXPECT_TRUE(reader.IsGood());

    EXPECT_EQ(reader.ReadU32(), 0u);
    EXPECT_FALSE(reader.IsGood());

    // what could still be read is read, and the reader stays not good
    EXPECT_EQ(reader.ReadU8(), 0x03);
    EXPECT_FALSE(reader.IsGood());
  }

  TEST(ByteReaderTest, ReadsANameUpToItsZeroAndSkipsTheRestOfItsBytes)
  {
    const std::vector<std::uint8_t> bytes = {'m', 'a', 'p', 0, 'x', 'x', 'x', 'x', 0x2a};
    ByteReader reader(bytes);

    EXPECT_EQ(reader.ReadFixedString(8), "map");
    EXPECT_EQ(reader.ReadU8(), 0x2a);
    EXPECT_TRUE(reader.IsGood());
  }

  TEST(ByteReaderTest, ReadsANameThatFillsItsBytesWithoutAZero)
  {
    const std::vector<std::uint8_t> bytes = {'a', 'b', 'c', 'd'};
    ByteReader reader(bytes);

    EXPECT_EQ(reader.ReadFixedString(4), "abcd");
    EXPECT_TRUE(reader.IsGood());
  }

  TEST(ByteReaderTest, SeeksSkipsAndHandsOutBytesWithoutCopying)
  {
    const std::vector<std::uint8_t> bytes = {0, 1, 2, 3, 4, 5, 6, 7};
    ByteReader reader(bytes);

    reader.Seek(4);
    reader.Skip(1);
    const auto two = reader.ReadBytes(2);
    ASSERT_EQ(two.size(), 2u);
    EXPECT_EQ(two.data(), bytes.data() + 5);
    EXPECT_EQ(reader.GetPosition(), 7u);

    const auto elsewhere = reader.BytesAt(1, 3);
    ASSERT_EQ(elsewhere.size(), 3u);
    EXPECT_EQ(elsewhere[0], 1);
    EXPECT_EQ(reader.GetPosition(), 7u);
    EXPECT_TRUE(reader.IsGood());
  }

  TEST(ByteReaderTest, RefusesAPlaceOrBytesPastTheEnd)
  {
    const std::vector<std::uint8_t> bytes = {0, 1, 2, 3};

    ByteReader seeking(bytes);
    seeking.Seek(4);
    EXPECT_TRUE(seeking.IsGood());
    seeking.Seek(5);
    EXPECT_FALSE(seeking.IsGood());

    ByteReader skipping(bytes);
    skipping.Skip(5);
    EXPECT_FALSE(skipping.IsGood());
    EXPECT_EQ(skipping.GetPosition(), 0u);

    ByteReader asking(bytes);
    EXPECT_TRUE(asking.BytesAt(2, 3).empty());
    EXPECT_FALSE(asking.IsGood());

    // a count so large that adding it to the place would wrap around
    ByteReader wrapping(bytes);
    EXPECT_TRUE(wrapping.BytesAt(2, static_cast<std::size_t>(-1)).empty());
    EXPECT_FALSE(wrapping.IsGood());
  }
}
