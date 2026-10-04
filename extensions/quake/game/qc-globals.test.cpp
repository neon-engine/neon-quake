#include "qc-globals.hpp"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/real-data.test.hpp"
#include "level-program.test.hpp"

namespace
{
  using quake::LevelProgram;
  using quake::Progs;
  using quake::ProgsBuilder;
  using quake::ProgsType;
  using quake::QcGlobal;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::RealData;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;
  using ::testing::Not;

  TEST(QcGlobalsTest, ReadsAndWritesAGlobalOfEachType)
  {
    LevelProgram program;
    const std::int32_t start_frame = program.builder.NextFunction();
    program.EngineFunction("StartFrame");
    QcMachine machine = program.Make();
    const QcGlobals globals(machine.GetProgs());

    EXPECT_TRUE(globals.time.Set(machine, 2.5f));
    EXPECT_EQ(globals.time.Get(machine), 2.5f);
    EXPECT_EQ(machine.GetFloat(globals.time.GetOffset()), 2.5f);

    EXPECT_TRUE(globals.v_forward.Set(machine, {1.0f, 2.0f, 3.0f}));
    EXPECT_THAT(globals.v_forward.Get(machine), ElementsAre(1.0f, 2.0f, 3.0f));

    EXPECT_TRUE(globals.self.Set(machine, 7));
    EXPECT_EQ(globals.self.Get(machine), 7);

    EXPECT_TRUE(globals.mapname.SetText(machine, "start"));
    EXPECT_EQ(globals.mapname.GetText(machine), "start");
    EXPECT_EQ(machine.GetString(globals.mapname.Get(machine)), "start");

    EXPECT_EQ(globals.StartFrame.Get(machine), start_frame);

    EXPECT_TRUE(globals.parms[0].Set(machine, 4.0f));
    EXPECT_EQ(globals.parms[0].Get(machine), 4.0f);
  }

  TEST(QcGlobalsTest, ReadsZeroAndRefusesAWriteWhereTheProgramLacksAGlobal)
  {
    LevelProgram program;
    QcMachine machine = program.Make();
    const QcGlobals globals(machine.GetProgs());

    EXPECT_TRUE(globals.HasEssentials());
    EXPECT_FALSE(globals.trace_fraction.IsFound());
    EXPECT_EQ(globals.trace_fraction.GetOffset(), -1);
    EXPECT_EQ(globals.trace_fraction.Get(machine), 0.0f);
    EXPECT_FALSE(globals.trace_fraction.Set(machine, 1.0f));
    EXPECT_THAT(globals.trace_endpos.Get(machine), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_FALSE(globals.trace_endpos.Set(machine, {1.0f, 2.0f, 3.0f}));
    EXPECT_EQ(globals.trace_ent.Get(machine), 0);
    EXPECT_FALSE(globals.trace_ent.Set(machine, 1));
    EXPECT_EQ(globals.StartFrame.Get(machine), 0);
    EXPECT_FALSE(globals.parms[15].Set(machine, 1.0f));

    EXPECT_THAT(globals.missing, Contains("trace_fraction"));
    EXPECT_THAT(globals.missing, Contains("parm16"));
    EXPECT_THAT(globals.missing, Not(Contains("self")));
    EXPECT_THAT(globals.missing, Not(Contains("parm1")));
  }

  TEST(QcGlobalsTest, TakesAGlobalOfAnotherTypeAsNotFound)
  {
    // a program whose `time` is a vector and whose `self` is a float
    ProgsBuilder builder;
    builder.Name(builder.Vector(), "time", ProgsType::Vector);
    builder.Name(builder.Float(3.0f), "self", ProgsType::Float);
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(builder.Build(), error)) << error;
    QcMachine machine(progs);
    const QcGlobals globals(machine.GetProgs());

    EXPECT_FALSE(globals.time.IsFound());
    EXPECT_FALSE(globals.self.IsFound());
    EXPECT_EQ(globals.self.Get(machine), 0);
    EXPECT_FALSE(globals.HasEssentials());
    EXPECT_THAT(globals.missing, Contains("time"));
  }

  TEST(QcGlobalsTest, FindsASavedGlobalByItsTypeWithoutTheBitOfTheSavedGame)
  {
    ProgsBuilder builder;
    builder.Name(builder.Float(9.0f), "serverflags", ProgsType::Float, true);
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(builder.Build(), error)) << error;
    const QcMachine machine(progs);

    const QcGlobal<ProgsType::Float> flags(machine.GetProgs(), "serverflags");
    EXPECT_TRUE(flags.IsFound());
    EXPECT_EQ(flags.Get(machine), 9.0f);
  }

  TEST(QcGlobalsTest, FindsEveryGlobalInTheGameCodeOfTheRealGame)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(data.GetBytes("progs.dat"), error)) << error;

    const QcGlobals globals(progs);
    EXPECT_TRUE(globals.HasEssentials());
    EXPECT_THAT(globals.missing, IsEmpty());
  }
}
