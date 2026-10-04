#include "qc-fields.hpp"

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
  using quake::QcFields;
  using quake::QcMachine;
  using quake::RealData;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;
  using ::testing::Not;

  TEST(QcFieldsTest, ReadsAndWritesAFieldOfEachTypeForAnEntity)
  {
    LevelProgram program;
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());
    ASSERT_EQ(machine.CreateEntity(), 1);
    ASSERT_EQ(machine.CreateEntity(), 2);

    EXPECT_TRUE(fields.health.Set(machine, 1, 75.0f));
    EXPECT_EQ(fields.health.Get(machine, 1), 75.0f);
    EXPECT_EQ(machine.GetEntity(1)[static_cast<std::size_t>(fields.health.GetOffset())].AsFloat(), 75.0f);

    EXPECT_TRUE(fields.origin.Set(machine, 1, {1.0f, 2.0f, 3.0f}));
    EXPECT_THAT(fields.origin.Get(machine, 1), ElementsAre(1.0f, 2.0f, 3.0f));

    EXPECT_TRUE(fields.classname.SetText(machine, 1, "func_door"));
    EXPECT_EQ(fields.classname.GetText(machine, 1), "func_door");

    EXPECT_TRUE(fields.owner.Set(machine, 1, 2));
    EXPECT_EQ(fields.owner.Get(machine, 1), 2);

    EXPECT_TRUE(fields.think.Set(machine, 1, 5));
    EXPECT_EQ(fields.think.Get(machine, 1), 5);

    // the other entity has none of it
    EXPECT_EQ(fields.health.Get(machine, 2), 0.0f);
    EXPECT_THAT(fields.origin.Get(machine, 2), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(fields.classname.GetText(machine, 2), "");
  }

  TEST(QcFieldsTest, ReadsZeroAndRefusesAWriteWhereTheProgramLacksAField)
  {
    LevelProgram program;
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());

    EXPECT_TRUE(fields.HasEssentials());
    EXPECT_FALSE(fields.view_ofs.IsFound());
    EXPECT_THAT(fields.view_ofs.Get(machine, 0), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_FALSE(fields.view_ofs.Set(machine, 0, {0.0f, 0.0f, 22.0f}));
    EXPECT_EQ(fields.items.Get(machine, 0), 0.0f);
    EXPECT_FALSE(fields.items.Set(machine, 0, 1.0f));
    EXPECT_EQ(fields.message.GetText(machine, 0), "");
    EXPECT_FALSE(fields.message.SetText(machine, 0, "hello"));

    EXPECT_THAT(fields.missing, Contains("view_ofs"));
    EXPECT_THAT(fields.missing, Not(Contains("origin")));
  }

  TEST(QcFieldsTest, ReadsZeroAndRefusesAWriteForAnEntityThatThereIsNot)
  {
    LevelProgram program;
    QcMachine machine = program.Make();
    const QcFields fields(machine.GetProgs());

    EXPECT_EQ(fields.health.Get(machine, 1), 0.0f);
    EXPECT_FALSE(fields.health.Set(machine, 1, 1.0f));
    EXPECT_FALSE(fields.origin.Set(machine, -1, {1.0f, 2.0f, 3.0f}));
    EXPECT_FALSE(fields.classname.SetText(machine, 9, "light"));
    EXPECT_EQ(fields.classname.GetText(machine, 9), "");
  }

  TEST(QcFieldsTest, TakesAFieldOfAnotherTypeAsNotFoundAndSaysTheEssentialsAreNotThere)
  {
    ProgsBuilder builder;
    builder.Field("origin", ProgsType::Float);
    builder.Field("classname", ProgsType::String);
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(builder.Build(), error)) << error;

    const QcFields fields(progs);
    EXPECT_FALSE(fields.origin.IsFound());
    EXPECT_TRUE(fields.classname.IsFound());
    EXPECT_FALSE(fields.HasEssentials());
  }

  TEST(QcFieldsTest, FindsEveryFieldInTheGameCodeOfTheRealGame)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(data.GetBytes("progs.dat"), error)) << error;

    const QcFields fields(progs);
    EXPECT_TRUE(fields.HasEssentials());
    EXPECT_THAT(fields.missing, IsEmpty());
  }
}
