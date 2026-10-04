#include "level-spawning.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/entity-text.hpp"
#include "level-program.test.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"

namespace
{
  using quake::BspEntity;
  using quake::EntityText;
  using quake::LevelProgram;
  using quake::LevelSpawning;
  using quake::LevelSpawningReport;
  using quake::LevelSpawningSettings;
  using quake::QcFields;
  using quake::QcGlobals;
  using quake::QcMachine;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  /// The entities of a text as a level has it.
  std::vector<BspEntity> Read(const std::string_view text)
  {
    EntityText entity_text;
    std::string error;
    EXPECT_TRUE(entity_text.Read(text, error)) << error;
    return entity_text.entities;
  }

  /// A level that starts on a small program, whose functions `worldspawn`
  /// and `thing` note for which entity they were called.
  class LevelSpawningTest : public ::testing::Test
  {
  protected:
    LevelProgram _program;
    std::int32_t _worldspawn = _program.Function("worldspawn");
    std::int32_t _thing = _program.Function("thing");
    std::int32_t _thing_think = _program.Function("thing_think");
    std::int32_t _broken = _program.Function("broken");

    /// The entities `thing` was called for, as the global `self` named them.
    std::vector<std::int32_t> _things;
    int _worlds = 0;

    /// Starts a level of a text on a machine made now.
    LevelSpawningReport Spawn(QcMachine &machine, const std::string_view text, const LevelSpawningSettings &settings = {})
    {
      const QcGlobals globals(machine.GetProgs());
      machine.SetBuiltin(_worldspawn, [this](QcMachine &) { _worlds++; });
      machine.SetBuiltin(_thing, [this, globals](QcMachine &running)
      {
        _things.push_back(globals.self.Get(running));
      });
      machine.SetBuiltin(_thing_think, [](QcMachine &) {});
      machine.SetBuiltin(_broken, [](QcMachine &running) { running.Stop("it is broken"); });

      LevelSpawning spawning(machine);
      return spawning.Spawn(Read(text), settings);
    }
  };

  TEST_F(LevelSpawningTest, SetsUpWhatTheGameCodeExpectsBeforeItRuns)
  {
    QcMachine machine = _program.Make();
    const QcGlobals globals(machine.GetProgs());
    const QcFields fields(machine.GetProgs());

    Spawn(machine, R"({ "classname" "worldspawn" })", {
            .map_name = "start", .model_name = "maps/start.bsp", .deathmatch = 1.0f, .coop = 0.0f,
            .server_flags = 3.0f,
          });

    EXPECT_EQ(globals.mapname.GetText(machine), "start");
    EXPECT_EQ(globals.time.Get(machine), 1.0f);
    EXPECT_EQ(globals.serverflags.Get(machine), 3.0f);
    EXPECT_EQ(globals.deathmatch.Get(machine), 1.0f);

    EXPECT_EQ(fields.model.GetText(machine, 0), "maps/start.bsp");
    EXPECT_EQ(fields.modelindex.Get(machine, 0), 1.0f);
    EXPECT_EQ(fields.solid.Get(machine, 0), static_cast<float>(quake::QcSolid::Bsp));
    EXPECT_EQ(fields.movetype.Get(machine, 0), static_cast<float>(quake::QcMoveType::Push));
    EXPECT_EQ(_worlds, 1);
  }

  TEST_F(LevelSpawningTest, FillsTheWorldFromTheFirstEntityAndMakesTheOthersAfterThePlayers)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" "health" "5" }
      { "classname" "thing" }
      { "classname" "thing" }
    )", {.player_count = 2});

    EXPECT_EQ(report.spawned, 3u);
    EXPECT_EQ(fields.health.Get(machine, 0), 5.0f);
    EXPECT_EQ(fields.classname.GetText(machine, 0), "worldspawn");

    // entities 1 and 2 are the players', made and left empty
    EXPECT_EQ(machine.GetEntityCount(), 5);
    EXPECT_FALSE(machine.IsEntityFree(1));
    EXPECT_FALSE(machine.IsEntityFree(2));
    EXPECT_EQ(fields.classname.GetText(machine, 1), "");
    EXPECT_THAT(_things, ElementsAre(3, 4));
  }

  TEST_F(LevelSpawningTest, WritesEachKeyIntoItsFieldByTheTypeOfTheField)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" }
      {
        "classname" "thing"
        "target" "first line\nsecond"
        "health" "25.5"
        "origin" "1 -2.5 300"
        "owner" "1"
        "think" "thing_think"
      }
    )");

    EXPECT_EQ(report.spawned, 2u);
    EXPECT_EQ(report.unknown_keys, 0u);
    EXPECT_EQ(fields.classname.GetText(machine, 2), "thing");
    EXPECT_EQ(fields.target.GetText(machine, 2), "first line\nsecond");
    EXPECT_EQ(fields.health.Get(machine, 2), 25.5f);
    EXPECT_THAT(fields.origin.Get(machine, 2), ElementsAre(1.0f, -2.5f, 300.0f));
    EXPECT_EQ(fields.owner.Get(machine, 2), 1);
    EXPECT_EQ(fields.think.Get(machine, 2), machine.GetProgs().FindFunction("thing_think"));
  }

  TEST_F(LevelSpawningTest, TakesAVectorWithNumbersMissingAsZeroThere)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());

    Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "thing" "origin" "8 16" }
      { "classname" "thing" "origin" "none" }
    )");

    EXPECT_THAT(fields.origin.Get(machine, 2), ElementsAre(8.0f, 16.0f, 0.0f));
    EXPECT_THAT(fields.origin.Get(machine, 3), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(LevelSpawningTest, TakesAngleAsTheYawOfTheAnglesAndLightAsTheBrightness)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());
    const quake::QcField<quake::ProgsType::Float> light_lev(machine.GetProgs(), "light_lev");
    const quake::QcField<quake::ProgsType::Function> light(machine.GetProgs(), "light");

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "thing" "angle" "90" "light" "250" }
    )");

    EXPECT_EQ(report.unknown_keys, 0u);
    EXPECT_THAT(fields.angles.Get(machine, 2), ElementsAre(0.0f, 90.0f, 0.0f));
    EXPECT_EQ(light_lev.Get(machine, 2), 250.0f);
    // the field `light` is a function, and is not what the key means
    EXPECT_EQ(light.Get(machine, 2), 0);
  }

  TEST_F(LevelSpawningTest, SkipsKeysOfTheCompilerAndCountsThoseThatAreNoField)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" "_sunlight" "200" "wad" "gfx/base.wad" }
      { "classname" "thing" "_minlight" "10" "nonsense" "1" "think" "no_such_function" "health " "3" }
    )");

    // `wad`, `nonsense`, and the function that there is not
    EXPECT_EQ(report.unknown_keys, 3u);
    EXPECT_EQ(report.spawned, 2u);
    EXPECT_EQ(fields.think.Get(machine, 2), 0);
    // spaces after a key are not part of it
    EXPECT_EQ(fields.health.Get(machine, 2), 3.0f);
  }

  TEST_F(LevelSpawningTest, LeavesOutByTheSkillWhatTheFlagsSay)
  {
    const std::string_view text = R"(
      { "classname" "worldspawn" }
      { "classname" "thing" "health" "1" "spawnflags" "256" }
      { "classname" "thing" "health" "2" "spawnflags" "512" }
      { "classname" "thing" "health" "3" "spawnflags" "1024" }
      { "classname" "thing" "health" "4" "spawnflags" "2048" }
      { "classname" "thing" "health" "5" "spawnflags" "1793" }
      { "classname" "thing" "health" "6" }
    )";

    // which of the six are there, by the health each was given
    const auto spawned = [this, text](const LevelSpawningSettings &settings, std::size_t left_out)
    {
      QcMachine machine = _program.Make();
      const QcFields fields(machine.GetProgs());
      _things.clear();
      const LevelSpawningReport report = Spawn(machine, text, settings);
      EXPECT_EQ(report.left_out, left_out);
      EXPECT_EQ(report.spawned + report.left_out, 7u);

      std::vector<float> healths;
      for (const std::int32_t thing : _things)
      {
        EXPECT_FALSE(machine.IsEntityFree(thing));
        healths.push_back(fields.health.Get(machine, thing));
      }
      return healths;
    };

    EXPECT_THAT(spawned({.skill = 0}, 2), ElementsAre(2.0f, 3.0f, 4.0f, 6.0f));
    EXPECT_THAT(spawned({.skill = 1}, 2), ElementsAre(1.0f, 3.0f, 4.0f, 6.0f));
    EXPECT_THAT(spawned({.skill = 2}, 2), ElementsAre(1.0f, 2.0f, 4.0f, 6.0f));
    EXPECT_THAT(spawned({.skill = 3}, 2), ElementsAre(1.0f, 2.0f, 4.0f, 6.0f));
    // a skill outside counts as the nearest
    EXPECT_THAT(spawned({.skill = -1}, 2), ElementsAre(2.0f, 3.0f, 4.0f, 6.0f));
    EXPECT_THAT(spawned({.skill = 9}, 2), ElementsAre(1.0f, 2.0f, 4.0f, 6.0f));
    // a deathmatch has no skill
    EXPECT_THAT(spawned({.skill = 0, .deathmatch = 1.0f}, 1), ElementsAre(1.0f, 2.0f, 3.0f, 5.0f, 6.0f));
  }

  TEST_F(LevelSpawningTest, GivesTheNumberOfAnEntityLeftOutToTheNext)
  {
    QcMachine machine = _program.Make();
    const QcFields fields(machine.GetProgs());

    Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "thing" "health" "1" "spawnflags" "512" "target" "left" }
      { "classname" "thing" "health" "2" }
    )");

    // nothing of the one left out is on the one that took its place
    EXPECT_THAT(_things, ElementsAre(2));
    EXPECT_EQ(fields.health.Get(machine, 2), 2.0f);
    EXPECT_EQ(fields.spawnflags.Get(machine, 2), 0.0f);
    EXPECT_EQ(fields.target.GetText(machine, 2), "");
    EXPECT_EQ(machine.GetEntityCount(), 3);
  }

  TEST_F(LevelSpawningTest, FreesAnEntityWithoutAFunctionAndNotesItsClassname)
  {
    QcMachine machine = _program.Make();

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "thing" }
      { "classname" "monster_unknown" }
      { "health" "5" }
      { "classname" "monster_unknown" }
      { "classname" "thing" }
    )");

    EXPECT_EQ(report.spawned, 3u);
    EXPECT_EQ(report.without_function, 3u);
    EXPECT_THAT(report.classnames_without_function, ElementsAre("monster_unknown", ""));
    EXPECT_THAT(report.failures, IsEmpty());
    // each took the number the one before had given back
    EXPECT_THAT(_things, ElementsAre(2, 3));
    EXPECT_EQ(machine.GetEntityCount(), 4);
  }

  TEST_F(LevelSpawningTest, NotesAFunctionThatIsStoppedAndGoesOn)
  {
    QcMachine machine = _program.Make();

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "broken" }
      { "classname" "thing" }
    )");

    EXPECT_EQ(report.spawned, 2u);
    EXPECT_EQ(report.failed, 1u);
    ASSERT_EQ(report.failures.size(), 1u);
    EXPECT_EQ(report.failures[0].entity, 2);
    EXPECT_EQ(report.failures[0].classname, "broken");
    EXPECT_EQ(report.failures[0].function, "broken");
    EXPECT_THAT(report.failures[0].error.message, HasSubstr("it is broken"));

    // the entity stays, and the next one ran
    EXPECT_FALSE(machine.IsEntityFree(2));
    EXPECT_THAT(_things, ElementsAre(3));
  }

  TEST_F(LevelSpawningTest, StopsAtTheEntityTheMachineHasNoRoomFor)
  {
    quake::Progs progs;
    std::string error;
    ASSERT_TRUE(progs.Read(_program.builder.Build(), error)) << error;
    // the world, a player, and one more
    QcMachine machine(progs, {.entities = 3});

    const LevelSpawningReport report = Spawn(machine, R"(
      { "classname" "worldspawn" }
      { "classname" "thing" }
      { "classname" "thing" }
      { "classname" "thing" }
    )");

    EXPECT_EQ(report.spawned, 2u);
    EXPECT_EQ(report.without_room, 2u);
    EXPECT_THAT(_things, ElementsAre(2));
  }

  TEST_F(LevelSpawningTest, SpawnsNothingOfATextWithoutEntities)
  {
    QcMachine machine = _program.Make();

    const LevelSpawningReport report = Spawn(machine, "");

    EXPECT_EQ(report.spawned, 0u);
    EXPECT_EQ(_worlds, 0);
    // the player's entity is there all the same
    EXPECT_EQ(machine.GetEntityCount(), 2);
  }
}
