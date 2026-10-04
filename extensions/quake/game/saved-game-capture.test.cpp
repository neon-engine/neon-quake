#include "saved-game-capture.hpp"

#include <cstdint>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/qc-limits.hpp"
#include "saved-game-program.test.hpp"
#include "saved-game-text.hpp"

namespace
{
  using quake::Progs;
  using quake::QcCell;
  using quake::QcLimits;
  using quake::QcMachine;
  using quake::SavedGame;
  using quake::SavedGameCapture;
  using quake::SavedGameProgram;
  using quake::SavedGameRestoreReport;
  using quake::SavedGameText;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;
  using ::testing::Pair;

  class SavedGameCaptureTest : public ::testing::Test
  {
  protected:
    SavedGameProgram _program;

    [[nodiscard]] std::int32_t Global(const QcMachine &machine, const std::string_view name) const
    {
      return machine.GetProgs().FindGlobal(name)->offset;
    }

    [[nodiscard]] std::size_t Field(const QcMachine &machine, const std::string_view name) const
    {
      return machine.GetProgs().FindField(name)->offset;
    }

    /// A game of four entities: the world, a thing, a free one, and a sign
    /// that owns the thing.
    void Play(QcMachine &machine) const
    {
      machine.SetInteger(Global(machine, "self"), 1);
      machine.SetFloat(Global(machine, "time"), 12.5f);
      machine.SetInteger(Global(machine, "mapname"), machine.AddString("e1m1"));
      machine.SetFloat(Global(machine, "killed_monsters"), 3.0f);
      machine.SetVector(Global(machine, "v_forward"), {1.0f, 0.0f, 0.0f});

      for (int i = 0; i < 3; i++) { ASSERT_TRUE(machine.CreateEntity()); }

      machine.GetEntity(0)[Field(machine, "classname")] = QcCell::OfInteger(machine.AddString("worldspawn"));

      const std::span<QcCell> thing = machine.GetEntity(1);
      thing[Field(machine, "classname")] = QcCell::OfInteger(machine.AddString("thing"));
      thing[Field(machine, "origin")] = QcCell::OfFloat(1.0f);
      thing[Field(machine, "origin") + 1] = QcCell::OfFloat(2.5f);
      thing[Field(machine, "origin") + 2] = QcCell::OfFloat(-3.0f);
      thing[Field(machine, "health")] = QcCell::OfFloat(100.0f);
      thing[Field(machine, "owner")] = QcCell::OfInteger(3);
      thing[Field(machine, "think")] = QcCell::OfInteger(_program.think);
      thing[Field(machine, "which")] = QcCell::OfInteger(static_cast<std::int32_t>(Field(machine, "health")));

      // freed with a field still set, which is not saved
      machine.GetEntity(2)[Field(machine, "health")] = QcCell::OfFloat(5.0f);
      machine.FreeEntity(2);

      machine.GetEntity(3)[Field(machine, "message")] = QcCell::OfInteger(machine.AddString("say \"two\nlines\""));
    }

    /// The head of a saved game as a host fills it in.
    [[nodiscard]] static SavedGame MakeHead()
    {
      SavedGame game;
      game.comment = SavedGameText::MakeComment("the Test Level", 3, 42);
      game.parms[0] = 100.0f;
      game.skill = 2;
      game.map_name = "e1m1";
      game.time = 12.5;
      return game;
    }
  };

  TEST_F(SavedGameCaptureTest, TakesTheSavedGlobalsAndTheFieldsThatAreNotZero)
  {
    QcMachine machine = _program.Make();
    Play(machine);

    const SavedGame game = SavedGameCapture::Capture(machine, MakeHead());
    // what the host filled in stays
    EXPECT_EQ(game.map_name, "e1m1");
    EXPECT_EQ(game.skill, 2);
    EXPECT_EQ(game.parms[0], 100.0f);

    EXPECT_THAT(game.globals, ElementsAre(
                  Pair("self", "1"), Pair("time", "12.500000"), Pair("mapname", "e1m1"),
                  Pair("killed_monsters", "3.000000")));

    ASSERT_EQ(game.entities.size(), 4u);
    EXPECT_THAT(game.entities[0].pairs, ElementsAre(Pair("classname", "worldspawn")));
    EXPECT_THAT(game.entities[1].pairs, ElementsAre(
                  Pair("classname", "thing"), Pair("origin", "1.000000 2.500000 -3.000000"),
                  Pair("health", "100.000000"), Pair("owner", "3"), Pair("think", "thing_think"),
                  Pair("which", "health")));
    EXPECT_TRUE(game.entities[2].free);
    EXPECT_THAT(game.entities[2].pairs, IsEmpty());
    EXPECT_FALSE(game.entities[3].free);
    EXPECT_THAT(game.entities[3].pairs, ElementsAre(Pair("message", "say \"two\nlines\"")));
  }

  TEST_F(SavedGameCaptureTest, GivesTheTextOfTheOriginalForASmallGame)
  {
    QcMachine machine = _program.Make();
    Play(machine);

    std::string expected = "5\nthe_Test_Level________kills:__3/_42____\n100.000000\n";
    for (int i = 1; i < 16; i++) { expected += "0.000000\n"; }
    expected += "2\ne1m1\n12.500000\n";
    for (int i = 0; i < 64; i++) { expected += "m\n"; }
    expected +=
      "{\n"
      "\"self\" \"1\"\n"
      "\"time\" \"12.500000\"\n"
      "\"mapname\" \"e1m1\"\n"
      "\"killed_monsters\" \"3.000000\"\n"
      "}\n"
      "{\n"
      "\"classname\" \"worldspawn\"\n"
      "}\n"
      "{\n"
      "\"classname\" \"thing\"\n"
      "\"origin\" \"1.000000 2.500000 -3.000000\"\n"
      "\"health\" \"100.000000\"\n"
      "\"owner\" \"3\"\n"
      "\"think\" \"thing_think\"\n"
      "\"which\" \"health\"\n"
      "}\n"
      "{\n"
      "}\n"
      "{\n"
      "\"message\" \"say 'two\nlines'\"\n"
      "}\n";
    EXPECT_EQ(SavedGameText::Write(SavedGameCapture::Capture(machine, MakeHead())), expected);
  }

  TEST_F(SavedGameCaptureTest, PutsAGameBackIntoAnotherMachineCellForCell)
  {
    QcMachine first = _program.Make();
    Play(first);
    // no quote, which the text cannot hold
    first.GetEntity(3)[Field(first, "message")] = QcCell::OfInteger(first.AddString("two\nlines"));

    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(
      SavedGameText::Write(SavedGameCapture::Capture(first, MakeHead())), read, error)) << error;

    // The second machine is in another state: more entities, one of them
    // free where the saved game has one that is not, and fields set that
    // the saved game has as zero.
    QcMachine second = _program.Make();
    for (int i = 0; i < 6; i++) { ASSERT_TRUE(second.CreateEntity()); }
    second.FreeEntity(1);
    second.GetEntity(3)[Field(second, "health")] = QcCell::OfFloat(77.0f);
    second.GetEntity(5)[Field(second, "health")] = QcCell::OfFloat(77.0f);
    second.SetFloat(Global(second, "killed_monsters"), 40.0f);

    SavedGameRestoreReport report;
    ASSERT_TRUE(SavedGameCapture::Restore(read, second, error, &report)) << error;
    EXPECT_EQ(report.unknown_globals, 0u);
    EXPECT_EQ(report.unknown_fields, 0u);

    for (const std::string_view name : {"self", "time", "killed_monsters"})
    {
      EXPECT_EQ(second.GetInteger(Global(second, name)), first.GetInteger(Global(first, name))) << name;
    }
    EXPECT_EQ(second.GetString(second.GetInteger(Global(second, "mapname"))), "e1m1");
    // marked, but of a kind that is not saved
    EXPECT_EQ(second.GetVector(Global(second, "v_forward"))[0], 0.0f);

    ASSERT_EQ(second.GetEntityCount(), 7);
    const std::size_t message = Field(first, "message");
    const std::size_t classname = Field(first, "classname");
    for (std::int32_t entity = 0; entity < 4; entity++)
    {
      EXPECT_EQ(second.IsEntityFree(entity), first.IsEntityFree(entity)) << entity;
      if (first.IsEntityFree(entity)) { continue; }

      const std::span<QcCell> a = first.GetEntity(entity);
      const std::span<QcCell> b = second.GetEntity(entity);
      for (std::size_t cell = 0; cell < a.size(); cell++)
      {
        // a string is the same text, at an offset of the machine's own
        if (cell == message || cell == classname)
        {
          EXPECT_EQ(second.GetString(b[cell].AsInteger()), first.GetString(a[cell].AsInteger())) << entity;
          continue;
        }
        EXPECT_EQ(b[cell].bits, a[cell].bits) << "entity " << entity << ", cell " << cell;
      }
    }

    // a free one is zero, and those beyond the saved game are free
    EXPECT_EQ(second.GetEntity(2)[Field(second, "health")].bits, 0u);
    for (std::int32_t entity = 4; entity < 7; entity++) { EXPECT_TRUE(second.IsEntityFree(entity)) << entity; }
    // so that the next entity gets the number it would have got
    EXPECT_EQ(second.CreateEntity(), first.CreateEntity());
  }

  TEST_F(SavedGameCaptureTest, MakesTheEntitiesAMachineLacks)
  {
    QcMachine first = _program.Make();
    Play(first);
    const SavedGame game = SavedGameCapture::Capture(first);

    QcMachine second = _program.Make();
    std::string error;
    ASSERT_TRUE(SavedGameCapture::Restore(game, second, error)) << error;
    EXPECT_EQ(second.GetEntityCount(), 4);
    EXPECT_FALSE(second.IsEntityFree(1));
    EXPECT_TRUE(second.IsEntityFree(2));
    EXPECT_FALSE(second.IsEntityFree(3));
    EXPECT_EQ(second.GetEntity(1)[Field(second, "owner")].AsInteger(), 3);
  }

  TEST_F(SavedGameCaptureTest, FreesAnEntityThatHasEveryFieldZeroAsTheOriginal)
  {
    QcMachine first = _program.Make();
    ASSERT_TRUE(first.CreateEntity());
    const SavedGame game = SavedGameCapture::Capture(first);
    EXPECT_FALSE(game.entities[1].free);

    QcMachine second = _program.Make();
    std::string error;
    ASSERT_TRUE(SavedGameCapture::Restore(game, second, error)) << error;
    EXPECT_TRUE(second.IsEntityFree(1));
    // the world never is
    EXPECT_FALSE(second.IsEntityFree(0));
  }

  TEST_F(SavedGameCaptureTest, LeavesOutAndCountsNamesTheProgramDoesNotHave)
  {
    SavedGame game;
    game.globals = {{"no_such_global", "1"}, {"killed_monsters", "9"}};
    game.entities.resize(2);
    game.entities[0].pairs = {{"classname", "worldspawn"}, {"no_such_field", "1"}};
    game.entities[1].pairs = {{"alpha", "0.5"}, {"health", "50"}};

    QcMachine machine = _program.Make();
    std::string error;
    SavedGameRestoreReport report;
    ASSERT_TRUE(SavedGameCapture::Restore(game, machine, error, &report)) << error;
    EXPECT_EQ(report.unknown_globals, 1u);
    EXPECT_EQ(report.unknown_fields, 2u);
    EXPECT_EQ(machine.GetFloat(Global(machine, "killed_monsters")), 9.0f);
    EXPECT_EQ(machine.GetEntity(1)[Field(machine, "health")].AsFloat(), 50.0f);
  }

  TEST_F(SavedGameCaptureTest, RefusesWhatCannotBePutBackAndLeavesTheMachine)
  {
    QcMachine machine = _program.Make();
    Play(machine);
    const auto expect_untouched = [&]
    {
      EXPECT_EQ(machine.GetFloat(Global(machine, "killed_monsters")), 3.0f);
      EXPECT_EQ(machine.GetEntity(1)[Field(machine, "health")].AsFloat(), 100.0f);
      EXPECT_TRUE(machine.IsEntityFree(2));
    };

    std::string error;
    SavedGame game;
    game.globals = {{"killed_monsters", "9"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    EXPECT_THAT(error, HasSubstr("no entity"));
    expect_untouched();

    game.entities.resize(2);
    game.entities[1].pairs = {{"think", "no_such_function"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    EXPECT_THAT(error, HasSubstr("`think` of entity 1"));
    EXPECT_THAT(error, HasSubstr("no_such_function"));
    expect_untouched();

    game.entities[1].pairs = {{"which", "no_such_field"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    expect_untouched();

    game.entities[1].pairs = {{"owner", "2"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    EXPECT_THAT(error, HasSubstr("entity 2"));
    game.entities[1].pairs = {{"owner", "-1"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    expect_untouched();

    game.entities[1].pairs = {{"health", "1"}};
    game.globals = {{"self", "7"}};
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    EXPECT_THAT(error, HasSubstr("The global `self`"));
    expect_untouched();
  }

  TEST_F(SavedGameCaptureTest, RefusesMoreEntitiesThanTheMachineHasRoomFor)
  {
    Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(_program.builder.Build(), error)) << error;
    QcMachine machine(std::move(progs), QcLimits{.entities = 3});

    SavedGame game;
    game.entities.resize(5);
    for (auto &entity : game.entities) { entity.pairs = {{"health", "1"}}; }
    EXPECT_FALSE(SavedGameCapture::Restore(game, machine, error));
    EXPECT_THAT(error, HasSubstr("room for 3"));
  }
} // namespace
