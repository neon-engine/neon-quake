#include "qc-builtin-number.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::QcBuiltinName;
  using quake::QcBuiltinNumber;

  TEST(QcBuiltinNumberTest, NamesABuiltinByItsNumberAsTheOriginalDoes)
  {
    EXPECT_EQ(QcBuiltinName(1), "makevectors");
    EXPECT_EQ(QcBuiltinName(QcBuiltinNumber::Spawn), "spawn");
    EXPECT_EQ(QcBuiltinName(QcBuiltinNumber::VecToAngles), "vectoangles");
    EXPECT_EQ(QcBuiltinName(QcBuiltinNumber::WriteEntity), "WriteEntity");
    EXPECT_EQ(QcBuiltinName(QcBuiltinNumber::CVarSet), "cvar_set");
    EXPECT_EQ(QcBuiltinName(QcBuiltinNumber::SetSpawnParms), "setspawnparms");
  }

  TEST(QcBuiltinNumberTest, HasNoNameForANumberTheOriginalHasNoBuiltinOf)
  {
    EXPECT_EQ(QcBuiltinName(0), "");
    EXPECT_EQ(QcBuiltinName(5), "");
    EXPECT_EQ(QcBuiltinName(71), "");
    EXPECT_EQ(QcBuiltinName(79), "");
    EXPECT_EQ(QcBuiltinName(-1), "");
  }
}
