#include "qc-console-variables.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::QcConsoleVariables;

  TEST(QcConsoleVariablesTest, StartsWithWhatTheGameCodeReadsAsTheOriginalHasIt)
  {
    const QcConsoleVariables variables;

    EXPECT_EQ(variables.GetFloat("skill"), 1.0f);
    EXPECT_EQ(variables.GetFloat("deathmatch"), 0.0f);
    EXPECT_EQ(variables.GetFloat("coop"), 0.0f);
    EXPECT_EQ(variables.GetFloat("teamplay"), 0.0f);
    EXPECT_EQ(variables.GetFloat("registered"), 0.0f);
    EXPECT_EQ(variables.GetFloat("temp1"), 0.0f);
    EXPECT_EQ(variables.GetFloat("sv_gravity"), 800.0f);
    EXPECT_EQ(variables.GetFloat("sv_maxspeed"), 320.0f);
    EXPECT_FLOAT_EQ(variables.GetFloat("sv_aim"), 0.93f);
    EXPECT_TRUE(variables.Has("registered"));
    EXPECT_EQ(variables.GetText("sv_gravity"), "800");
  }

  TEST(QcConsoleVariablesTest, ReadsAVariableNobodySetAsEmptyAndZero)
  {
    const QcConsoleVariables variables;

    EXPECT_FALSE(variables.Has("crosshair"));
    EXPECT_EQ(variables.GetText("crosshair"), "");
    EXPECT_EQ(variables.GetFloat("crosshair"), 0.0f);
  }

  TEST(QcConsoleVariablesTest, SetsAVariableThatIsThereAndMakesOneThatIsNot)
  {
    QcConsoleVariables variables;
    const std::size_t count = variables.GetCount();

    variables.Set("skill", "3");
    EXPECT_EQ(variables.GetFloat("skill"), 3.0f);
    EXPECT_EQ(variables.GetCount(), count);

    variables.Set("nextmap", "e1m2");
    EXPECT_TRUE(variables.Has("nextmap"));
    EXPECT_EQ(variables.GetText("nextmap"), "e1m2");
    EXPECT_EQ(variables.GetCount(), count + 1);
  }

  TEST(QcConsoleVariablesTest, ReadsATextAsANumberAsFarAsItIsOne)
  {
    QcConsoleVariables variables;

    variables.Set("a", "2.5 or so");
    variables.Set("b", "-7");
    variables.Set("c", "fast");
    variables.Set("d", "");
    EXPECT_EQ(variables.GetFloat("a"), 2.5f);
    EXPECT_EQ(variables.GetFloat("b"), -7.0f);
    EXPECT_EQ(variables.GetFloat("c"), 0.0f);
    EXPECT_EQ(variables.GetFloat("d"), 0.0f);
  }

  TEST(QcConsoleVariablesTest, WritesANumberAsShortAsReadsBackTheSame)
  {
    QcConsoleVariables variables;

    variables.SetFloat("sv_gravity", 100.0f);
    EXPECT_EQ(variables.GetText("sv_gravity"), "100");
    variables.SetFloat("sv_aim", 0.93f);
    EXPECT_EQ(variables.GetText("sv_aim"), "0.93");
    EXPECT_EQ(variables.GetFloat("sv_aim"), 0.93f);
  }
}
