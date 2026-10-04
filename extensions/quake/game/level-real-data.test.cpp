#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "level-running.hpp"
#include "level-spawning.hpp"
#include "level-stand-ins.test.hpp"
#include "qc-move-type.hpp"

// The tests of LevelSpawning and LevelRunning with the game code and the
// levels of a real game, and builtins that stand in for an engine: levels
// are started, a player is let in, and time passes. Nothing moves but doors,
// since there is no mover.
namespace
{
  using quake::BspFile;
  using quake::ClientThink;
  using quake::EntityText;
  using quake::LevelFailure;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelSpawningReport;
  using quake::LevelStandIns;
  using quake::Progs;
  using quake::QcFields;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcMoveType;
  using quake::RealData;
  using ::testing::IsEmpty;

  class LevelRealDataTest : public ::testing::Test
  {
  protected:
    static constexpr std::int32_t player = LevelStandIns::player;

    /// The length of a frame of the original at its usual rate.
    static constexpr float frame_time = 1.0f / 72.0f;

    Progs _progs;
    BspFile _level;
    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<LevelStandIns> _stand_ins;
    std::unique_ptr<LevelRunning> _running;
    LevelSpawningReport _report;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      std::string error;
      ASSERT_TRUE(_progs.Read(data.GetBytes("progs.dat"), error)) << error;
    }

    /// Starts a level of the paks on a machine made now, as a host does:
    /// the entities handed to the game code, then two frames of a tenth of
    /// a second for everything to settle. False for a level that is not
    /// there or is refused, which fails the test.
    bool Start(const std::string_view name, const int skill = 1)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      _level = {};
      if (!_level.Read(RealData::Get().GetBytes(path), error))
      {
        ADD_FAILURE() << path << ": " << error;
        return false;
      }

      EntityText text;
      EXPECT_TRUE(text.Read(_level.entities, error)) << path << ": " << error;

      // the order matters: what was made for the machine before goes first
      _running.reset();
      _stand_ins.reset();
      _machine = std::make_unique<QcMachine>(_progs);
      _stand_ins = std::make_unique<LevelStandIns>(*_machine, _level);
      _stand_ins->skill = static_cast<float>(skill);

      LevelSpawning spawning(*_machine);
      _report = spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = skill});

      _running = std::make_unique<LevelRunning>(*_machine);
      _running->Advance(0.1f);
      _running->Advance(0.1f);
      return true;
    }

    /// Lets time pass with the player in, a frame at a time.
    void Run(const float seconds)
    {
      const int frames = static_cast<int>(seconds / frame_time);
      for (int frame = 0; frame < frames; frame++)
      {
        _running->RunClientThink(player, ClientThink::Before);
        _running->Advance(frame_time);
        _running->RunClientThink(player, ClientThink::After);
      }
    }

    /// The failures that are the machine's: a run it stopped itself, not
    /// one the game code ended with `error`.
    [[nodiscard]] static std::vector<std::string> OfTheMachine(const std::vector<LevelFailure> &failures)
    {
      std::vector<std::string> messages;
      for (const LevelFailure &failure : failures)
      {
        if (failure.error.message.starts_with(LevelStandIns::game_error)) { continue; }

        messages.push_back(std::format(
          "entity {} ({}), {}: stopped in {}: {}",
          failure.entity, failure.classname, failure.function, failure.error.function_name, failure.error.message));
      }
      return messages;
    }

    /// The first entity of a classname that is not free, or 0.
    [[nodiscard]] std::int32_t Find(const std::string_view classname) const
    {
      const QcFields fields(_machine->GetProgs());
      for (std::int32_t entity = 1; entity < _machine->GetEntityCount(); entity++)
      {
        if (!_machine->IsEntityFree(entity) && fields.classname.GetText(*_machine, entity) == classname)
        {
          return entity;
        }
      }
      return 0;
    }

    /// Starts a level, lets a player in, and runs ten seconds of it. Nothing
    /// the machine stopped is expected.
    void Play(const std::string_view name)
    {
      ASSERT_TRUE(Start(name)) << name;
      EXPECT_THAT(OfTheMachine(_report.failures), IsEmpty()) << name;
      EXPECT_THAT(_report.classnames_without_function, IsEmpty()) << name;
      EXPECT_EQ(_report.without_room, 0u) << name;
      EXPECT_GT(_report.spawned, 5u) << name;

      EXPECT_TRUE(_running->ConnectClient(player, "player")) << name;
      Run(10.0f);
      EXPECT_THAT(OfTheMachine(_running->GetFailures()), IsEmpty()) << name;
      EXPECT_LE(_running->GetFailureCount(), LevelRunning::max_failures_kept) << name;
    }
  };

  TEST_F(LevelRealDataTest, StartsTheLevelTheGameStartsWithAndRunsTenSecondsOfIt)
  {
    Play("start");
    const QcGlobals globals(_machine->GetProgs());
    const QcFields fields(_machine->GetProgs());

    // what the engine sets, as the game code sees it
    EXPECT_EQ(globals.mapname.GetText(*_machine), "start");
    EXPECT_EQ(fields.model.GetText(*_machine, 0), "maps/start.bsp");
    EXPECT_EQ(fields.classname.GetText(*_machine, 0), "worldspawn");

    // two frames to settle and 720 of the game
    EXPECT_NEAR(_running->GetTime(), 11.2, 0.001);
    EXPECT_NEAR(globals.time.Get(*_machine), 11.2f, 0.001f);
    const quake::QcGlobal<quake::ProgsType::Float> frame_count(_machine->GetProgs(), "framecount");
    EXPECT_EQ(frame_count.Get(*_machine), 722.0f);

    // the player is in, standing where the level starts one, and has gone
    // on thinking: the game code shows a new frame of standing every tenth
    // of a second
    EXPECT_EQ(fields.classname.GetText(*_machine, player), "player");
    EXPECT_EQ(fields.netname.GetText(*_machine, player), "player");
    EXPECT_EQ(fields.health.Get(*_machine, player), 100.0f);
    EXPECT_GT(fields.nextthink.Get(*_machine, player), 11.0f);
    EXPECT_THAT(_stand_ins->printed, ::testing::HasSubstr("player joined the server"));

    // A door is what pushes, with a model that is a part of the level. Its
    // function, `func_door`, names it anew.
    const std::int32_t door = Find("door");
    ASSERT_NE(door, 0);
    EXPECT_EQ(fields.movetype.Get(*_machine, door), static_cast<float>(QcMoveType::Push));
    EXPECT_TRUE(fields.model.GetText(*_machine, door).starts_with('*'));
    EXPECT_NE(fields.blocked.Get(*_machine, door), 0);
    EXPECT_NE(fields.use.Get(*_machine, door), 0);
  }

  TEST_F(LevelRealDataTest, GivesADoorItsThinkWhenTheLevelStartsAndHasItThinkByItsOwnClock)
  {
    const RealData &data = RealData::Get();
    std::string error;
    ASSERT_TRUE(_level.Read(data.GetBytes("maps/start.bsp"), error)) << error;
    EntityText text;
    ASSERT_TRUE(text.Read(_level.entities, error)) << error;

    _machine = std::make_unique<QcMachine>(_progs);
    _stand_ins = std::make_unique<LevelStandIns>(*_machine, _level);
    const QcFields fields(_machine->GetProgs());
    LevelSpawning spawning(*_machine);
    _report = spawning.Spawn(text.entities, {.map_name = "start", .model_name = "maps/start.bsp"});
    EXPECT_THAT(_report.failures, IsEmpty());

    // The function of a door leaves a think for a tenth of a second on its
    // own clock, in which it finds the doors that touch it.
    const std::int32_t door = Find("door");
    ASSERT_NE(door, 0);
    EXPECT_EQ(fields.movetype.Get(*_machine, door), static_cast<float>(QcMoveType::Push));
    EXPECT_NE(fields.think.Get(*_machine, door), 0);
    EXPECT_FLOAT_EQ(fields.nextthink.Get(*_machine, door), 0.1f);
    EXPECT_EQ(fields.ltime.Get(*_machine, door), 0.0f);
    EXPECT_EQ(fields.owner.Get(*_machine, door), 0);

    // a frame that does not reach it, then one that does
    LevelRunning running(*_machine);
    running.Advance(0.05f);
    EXPECT_FLOAT_EQ(fields.nextthink.Get(*_machine, door), 0.1f);
    running.Advance(0.1f);
    EXPECT_EQ(fields.nextthink.Get(*_machine, door), 0.0f);
    EXPECT_FLOAT_EQ(fields.ltime.Get(*_machine, door), 0.1f);
    // every door knows the first of the doors it is one with
    EXPECT_NE(fields.owner.Get(*_machine, door), 0);
    EXPECT_THAT(running.GetFailures(), IsEmpty());
  }

  TEST_F(LevelRealDataTest, StartsALevelWithMonstersAndRunsTenSecondsOfIt)
  {
    Play("lq_e1m1");
    const QcGlobals globals(_machine->GetProgs());
    const QcFields fields(_machine->GetProgs());

    // the monsters of a game of medium skill counted themselves
    EXPECT_EQ(globals.total_monsters.Get(*_machine), 49.0f);
    EXPECT_EQ(globals.killed_monsters.Get(*_machine), 0.0f);
    EXPECT_EQ(fields.message.GetText(*_machine, 0), "Rats Behind Bars");

    // the level has `"origin" "968 1456 88"` and `"angle" "80"` for where a
    // player starts, and the game code puts the player one unit above
    EXPECT_THAT(fields.origin.Get(*_machine, player), ::testing::ElementsAre(968.0f, 1456.0f, 89.0f));
    EXPECT_THAT(fields.angles.Get(*_machine, player), ::testing::ElementsAre(0.0f, 80.0f, 0.0f));
  }

  TEST_F(LevelRealDataTest, LeavesMonstersOutByTheSkill)
  {
    ASSERT_TRUE(Start("lq_e1m1", 0));
    const QcGlobals globals(_machine->GetProgs());
    const std::size_t left_out_on_easy = _report.left_out;
    const float monsters_on_easy = globals.total_monsters.Get(*_machine);

    ASSERT_TRUE(Start("lq_e1m1", 3));
    const float monsters_on_nightmare = QcGlobals(_machine->GetProgs()).total_monsters.Get(*_machine);

    // an easy game has fewer monsters, and so more entities left out
    EXPECT_GT(left_out_on_easy, _report.left_out);
    EXPECT_GT(monsters_on_easy, 0.0f);
    EXPECT_LT(monsters_on_easy, monsters_on_nightmare);
    // the level has 57 soldiers and 15 dogs, one of which is only there in
    // the easier games
    EXPECT_EQ(monsters_on_nightmare, 71.0f);
  }

  TEST_F(LevelRealDataTest, StartsEveryLevelOfTheRealGameAndRunsTenSecondsOfEach)
  {
    std::size_t levels = 0;
    std::int64_t statements = 0;
    std::vector<std::string> object_errors;
    for (const std::string &path : RealData::Get().ListNames("maps"))
    {
      // the models of the boxes of ammunition are in the same folder and
      // format, and are no levels
      const std::string_view file = std::string_view(path).substr(std::string_view("maps/").size());
      if (!file.ends_with(".bsp") || file.starts_with("b_")) { continue; }
      const std::string level(file.substr(0, file.size() - std::string_view(".bsp").size()));

      Play(level);
      if (HasFatalFailure()) { continue; }
      levels++;
      statements += _machine->GetStatementsRun();
      for (const std::string &message : _stand_ins->object_errors) { object_errors.push_back(level + ": " + message); }
    }
    EXPECT_GT(levels, 3u);

    std::cout << levels << " levels, " << statements << " statements.\nEntities the game code gave up on:\n";
    for (const std::string &message : object_errors) { std::cout << "  " << message << "\n"; }
  }
}
