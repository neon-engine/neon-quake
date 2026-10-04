#include "qc-cell.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::QcCell;

  TEST(QcCellTest, HoldsAFloatOrAnIntegerInTheSameBits)
  {
    EXPECT_EQ(QcCell{}.AsFloat(), 0.0f);
    EXPECT_EQ(QcCell{}.AsInteger(), 0);

    EXPECT_EQ(QcCell::OfFloat(1.0f).bits, 0x3f800000u);
    EXPECT_EQ(QcCell::OfFloat(-2.5f).AsFloat(), -2.5f);
    EXPECT_EQ(QcCell::OfFloat(1.0f).AsInteger(), 0x3f800000);

    EXPECT_EQ(QcCell::OfInteger(-1).bits, 0xffffffffu);
    EXPECT_EQ(QcCell::OfInteger(-7).AsInteger(), -7);
  }
}
