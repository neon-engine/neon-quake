#include "qc-world-builtins.hpp"

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "level-collision.hpp"
#include "level-running.hpp"
#include "level-spawning.hpp"
#include "level-world-host.test.hpp"
#include "qc-builtin-number.hpp"
#include "qc-core-builtins.hpp"
#include "qc-flag.hpp"
#include "qc-recording-host.test.hpp"

// The builtins that ask the level, with the game code and the levels of a
// real game: levels are started with the real builtins and the collision of
// the level, a player is let in, and time passes. Only what is a host's own
// is stood in for.
namespace
{
  using quake::BspFile;
  using quake::ClientThink;
  using quake::EntityText;
  using quake::HasFlag;
  using quake::LevelCollision;
  using quake::LevelFailure;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelSpawningReport;
  using quake::LevelTraceKind;
  using quake::LevelTraceResult;
  using quake::LevelVector;
  using quake::LevelWorldHost;
  using quake::Progs;
  using quake::QcBuiltinNumber;
  using quake::QcCoreBuiltins;
  using quake::QcFields;
  using quake::QcFlag;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcRecordingHost;
  using quake::QcWorldBuiltins;
  using quake::RealData;
  using ::testing::IsEmpty;

  class QcWorldBuiltinsRealDataTest : public ::testing::Test
  {
  protected:
    static constexpr std::int32_t player = 1;

    /// The length of a frame of the original at its usual rate.
    static constexpr float frame_time = 1.0f / 72.0f;

    Progs _progs;
    BspFile _level;
    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<QcFields> _fields;
    std::unique_ptr<QcRecordingHost> _host;
    std::unique_ptr<QcCoreBuiltins> _core;
    std::unique_ptr<LevelCollision> _collision;
    std::unique_ptr<QcWorldBuiltins> _world;
    std::unique_ptr<LevelWorldHost> _world_host;
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
    /// the builtins registered, the entities handed to the game code, then
    /// two frames of a tenth of a second for everything to settle. False
    /// for a level that is not there or is of a format that is not read.
    bool Start(const std::string_view name)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      _level = {};
      if (!_level.Read(RealData::Get().GetBytes(path), error)) { return false; }

      EntityText text;
      EXPECT_TRUE(text.Read(_level.entities, error)) << path << ": " << error;

      // the order matters: what was made for the machine before goes first
      _running.reset();
      _world_host.reset();
      _world.reset();
      _collision.reset();
      _core.reset();
      _machine = std::make_unique<QcMachine>(_progs);
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _host = std::make_unique<QcRecordingHost>();

      _core = std::make_unique<QcCoreBuiltins>(*_host);
      _core->Register(*_machine);
      _core->SeedRandom(1);
      _core->GetVariables().Set("skill", "1");
      _core->GetModels().Add(path);

      _collision = std::make_unique<LevelCollision>(*_machine);
      EXPECT_TRUE(_collision->Build(_level, error)) << path << ": " << error;
      _world = std::make_unique<QcWorldBuiltins>(*_collision);
      _world->Register(*_machine);
      _world_host = std::make_unique<LevelWorldHost>(_level, *_collision);
      _world_host->Register(*_machine);

      // Walking is not here yet: a monster that tries stays where it is.
      for (const QcBuiltinNumber number : {
             QcBuiltinNumber::WalkMove, QcBuiltinNumber::MoveToGoal, QcBuiltinNumber::ChangeYaw,
           })
      {
        _machine->SetBuiltin(static_cast<std::int32_t>(number), [](QcMachine &m) { m.SetReturnFloat(0.0f); });
      }
      _machine->SetBuiltin(
        static_cast<std::int32_t>(QcBuiltinNumber::CheckBottom), [](QcMachine &m) { m.SetReturnFloat(1.0f); });

      LevelSpawning spawning(*_machine);
      _report = spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = 1});

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
        if (failure.error.message.starts_with(QcCoreBuiltins::error_start)) { continue; }

        messages.push_back(std::format(
          "entity {} ({}), {}: stopped in {}: {}",
          failure.entity, failure.classname, failure.function, failure.error.function_name, failure.error.message));
      }
      return messages;
    }

    /// Whether an entity lies on something: it was put on the ground, or a
    /// unit under its box there is what stops it.
    [[nodiscard]] bool LiesOnSomething(const std::int32_t entity) const
    {
      if (HasFlag(_fields->flags.Get(*_machine, entity), QcFlag::OnGround)) { return true; }

      const LevelVector origin = _fields->origin.Get(*_machine, entity);
      const LevelTraceResult trace = _collision->Trace(
        origin, _fields->mins.Get(*_machine, entity), _fields->maxs.Get(*_machine, entity),
        {origin[0], origin[1], origin[2] - 1.0f}, LevelTraceKind::Normal, entity);
      return trace.fraction < 1.0f || trace.start_solid;
    }

    /// Starts a level, lets a player in, and runs some seconds of it.
    /// Nothing the machine stopped is expected, and every item that is
    /// there lies on something. Gives how many items there are.
    std::size_t Play(const std::string_view name, const float seconds)
    {
      EXPECT_THAT(OfTheMachine(_report.failures), IsEmpty()) << name;
      EXPECT_TRUE(_running->ConnectClient(player, "player")) << name;
      Run(seconds);
      EXPECT_THAT(OfTheMachine(_running->GetFailures()), IsEmpty()) << name;

      std::size_t items = 0;
      for (std::int32_t entity = 1; entity < _machine->GetEntityCount(); entity++)
      {
        if (_machine->IsEntityFree(entity)) { continue; }

        const std::string_view classname = _fields->classname.GetText(*_machine, entity);
        if (!classname.starts_with("item_") && !classname.starts_with("weapon_")) { continue; }

        items++;
        const LevelVector origin = _fields->origin.Get(*_machine, entity);
        EXPECT_TRUE(LiesOnSomething(entity)) << name << ": " << classname << " " << entity << " at " << origin[0]
          << " " << origin[1] << " " << origin[2];
      }
      return items;
    }

    /// How often the host was handed a line that has a text in it.
    [[nodiscard]] std::size_t CountCalls(const std::string_view text) const
    {
      std::size_t count = 0;
      for (const std::string &call : _host->calls) { count += call.find(text) != std::string::npos ? 1 : 0; }
      return count;
    }
  };

  TEST_F(QcWorldBuiltinsRealDataTest, PutsTheItemsOfALevelOnItsFloors)
  {
    ASSERT_TRUE(Start("lq_e1m1"));
    const std::size_t items = Play("lq_e1m1", 2.0f);
    EXPECT_GT(items, 10u);

    // The game code drops an item a moment after the level started, and
    // takes away one that finds no floor. One is: a box of shells that
    // stands in the corner of a block, flush with it on both of its far
    // sides. A place on a plane counts as in front of it, which is inside
    // the block here, as it is in the original.
    EXPECT_EQ(CountCalls("fell out of level"), 1u);

    // the player stands where the level starts one, in the open
    const QcGlobals globals(_machine->GetProgs());
    EXPECT_EQ(_fields->health.Get(*_machine, player), 100.0f);
    EXPECT_FALSE(_collision->TestPosition(player));
    EXPECT_EQ(globals.total_monsters.Get(*_machine), 49.0f);
  }

  TEST_F(QcWorldBuiltinsRealDataTest, StartsEveryLevelOfTheRealGameWithItsCollision)
  {
    std::size_t levels = 0;
    std::size_t items = 0;
    std::size_t fallen = 0;
    std::size_t with_enemy = 0;
    for (const std::string &path : RealData::Get().ListNames("maps"))
    {
      // the models of the boxes of ammunition are in the same folder and
      // format, and are no levels
      const std::string_view file = std::string_view(path).substr(std::string_view("maps/").size());
      if (!file.ends_with(".bsp") || file.starts_with("b_")) { continue; }
      const std::string level(file.substr(0, file.size() - std::string_view(".bsp").size()));

      // a level of a format that is not read is not this test's to mind
      if (!Start(level)) { continue; }

      levels++;
      items += Play(level, 2.0f);
      fallen += CountCalls("fell out of level");
      for (std::int32_t entity = 2; entity < _machine->GetEntityCount(); entity++)
      {
        with_enemy += !_machine->IsEntityFree(entity) && _fields->enemy.Get(*_machine, entity) != 0 ? 1 : 0;
      }
    }
    EXPECT_GT(levels, 3u);
    EXPECT_GT(items, 100u);

    std::cout << levels << " levels, " << items << " items on a floor, " << fallen
      << " items that found none, " << with_enemy << " entities with an enemy.\n";
  }
}
