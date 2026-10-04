#include "qc-world-builtins.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "level-collision.hpp"
#include "level-physics.hpp"
#include "level-running.hpp"
#include "level-spawning.hpp"
#include "level-stepping.hpp"
#include "level-touching.hpp"
#include "level-world-host.test.hpp"
#include "qc-builtin-number.hpp"
#include "qc-core-builtins.hpp"
#include "qc-flag.hpp"
#include "qc-recording-host.test.hpp"

// The builtins that ask the level and the mover of a level, with the game
// code and the levels of a real game: levels are started with the real
// builtins and the collision of the level, a player is let in, and time
// passes while everything falls, flies, and walks. Only what is a host's own
// is stood in for, and the player stands where the level starts one.
namespace
{
  using quake::BspFile;
  using quake::ClientThink;
  using quake::EntityText;
  using quake::HasFlag;
  using quake::LevelCollision;
  using quake::LevelFailure;
  using quake::LevelPhysics;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelSpawningReport;
  using quake::LevelStepping;
  using quake::LevelTouching;
  using quake::LevelTraceKind;
  using quake::LevelTraceResult;
  using quake::LevelVector;
  using quake::LevelWorldHost;
  using quake::Progs;
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
    std::unique_ptr<LevelRunning> _running;
    std::unique_ptr<LevelTouching> _touching;
    std::unique_ptr<LevelStepping> _stepping;
    std::unique_ptr<LevelPhysics> _physics;
    std::unique_ptr<QcWorldBuiltins> _world;
    std::unique_ptr<LevelWorldHost> _world_host;
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
    /// for a level that is not there or is refused, which fails the test.
    bool Start(const std::string_view name)
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
      _world_host.reset();
      _world.reset();
      _physics.reset();
      _stepping.reset();
      _touching.reset();
      _running.reset();
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
      _running = std::make_unique<LevelRunning>(*_machine);
      _touching = std::make_unique<LevelTouching>(*_collision, *_running);
      _stepping = std::make_unique<LevelStepping>(*_collision, *_touching, [this] { return _core->NextRandom(); });
      _physics = std::make_unique<LevelPhysics>(*_collision, *_touching);
      _running->SetMover(_physics.get());
      _world = std::make_unique<QcWorldBuiltins>(*_collision, *_stepping);
      _world->Register(*_machine);
      _world_host = std::make_unique<LevelWorldHost>(_level, *_collision);
      _world_host->Register(*_machine);

      LevelSpawning spawning(*_machine);
      _report = spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = 1});

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

    /// Puts the player in front of a monster that walks, where it looks
    /// and there is room and a floor, and gives the monster. 0 when the
    /// level has no monster with such a place.
    std::int32_t PutPlayerBefore(const std::map<std::int32_t, LevelVector> &monsters) const
    {
      const LevelVector player_mins = _fields->mins.Get(*_machine, player);
      const LevelVector player_maxs = _fields->maxs.Get(*_machine, player);
      const LevelVector before = _fields->origin.Get(*_machine, player);
      for (const auto &[monster, origin] : monsters)
      {
        const float flags = _fields->flags.Get(*_machine, monster);
        if (HasFlag(flags, QcFlag::Fly) || HasFlag(flags, QcFlag::Swim) || !HasFlag(flags, QcFlag::OnGround))
        {
          continue;
        }

        // with the feet where the monster has its own, a unit higher
        const float yaw = _fields->angles.Get(*_machine, monster)[1] * std::numbers::pi_v<float> / 180.0f;
        const float feet = origin[2] + _fields->mins.Get(*_machine, monster)[2];
        const LevelVector place = {
          origin[0] + std::cos(yaw) * 150.0f, origin[1] + std::sin(yaw) * 150.0f, feet - player_mins[2] + 1.0f,
        };
        _fields->origin.Set(*_machine, player, place);
        if (_collision->TestPosition(player)) { continue; }

        const LevelTraceResult floor = _collision->Trace(
          place, player_mins, player_maxs, {place[0], place[1], place[2] - 16.0f}, LevelTraceKind::Normal, player);
        const LevelTraceResult sight = _collision->Trace(
          quake::Sum(origin, _fields->view_ofs.Get(*_machine, monster)), {}, {},
          quake::Sum(place, _fields->view_ofs.Get(*_machine, player)), LevelTraceKind::NoMonsters, monster);
        if (floor.fraction == 1.0f || sight.fraction != 1.0f || sight.in_water) { continue; }

        _collision->Link(player);
        return monster;
      }
      _fields->origin.Set(*_machine, player, before);
      return 0;
    }

    /// Runs some seconds of a level the player is in. Nothing the machine
    /// stopped is expected, and every item that is there lies on
    /// something. Gives how many items there are.
    std::size_t Play(const std::string_view name, const float seconds)
    {
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

    /// Where every monster of the level is now, by its entity.
    [[nodiscard]] std::map<std::int32_t, LevelVector> FindMonsters() const
    {
      std::map<std::int32_t, LevelVector> monsters;
      for (std::int32_t entity = 2; entity < _machine->GetEntityCount(); entity++)
      {
        if (_machine->IsEntityFree(entity)) { continue; }
        if (!HasFlag(_fields->flags.Get(*_machine, entity), QcFlag::Monster)) { continue; }

        monsters[entity] = _fields->origin.Get(*_machine, entity);
      }
      return monsters;
    }

    /// How many of the monsters have the player for an enemy and are
    /// somewhere else than they were.
    [[nodiscard]] std::size_t CountHunters(const std::map<std::int32_t, LevelVector> &monsters) const
    {
      std::size_t hunters = 0;
      for (const auto &[monster, start] : monsters)
      {
        if (_machine->IsEntityFree(monster) || _fields->enemy.Get(*_machine, monster) != player) { continue; }

        hunters += quake::Length(quake::Difference(_fields->origin.Get(*_machine, monster), start)) > 16.0f ? 1 : 0;
      }
      return hunters;
    }
  };

  TEST_F(QcWorldBuiltinsRealDataTest, RunsALevelWithMonstersThatSeeThePlayerAndWalk)
  {
    ASSERT_TRUE(Start("lq_e1m1"));
    const std::map<std::int32_t, LevelVector> monsters = FindMonsters();
    EXPECT_EQ(monsters.size(), 49u);
    EXPECT_THAT(OfTheMachine(_report.failures), IsEmpty());
    ASSERT_TRUE(_running->ConnectClient(player, "player"));

    // No monster of the level sees the player from where the level starts
    // one, so the player is put in front of one.
    const std::int32_t watcher = PutPlayerBefore(monsters);
    ASSERT_NE(watcher, 0);

    // those the level itself put into something solid, or into each other
    std::set<std::int32_t> stuck_at_start;
    for (const auto &[monster, start] : monsters)
    {
      if (_collision->TestPosition(monster)) { stuck_at_start.insert(monster); }
    }

    const std::size_t items = Play("lq_e1m1", 20.0f);
    EXPECT_GT(items, 10u);

    // The game code drops an item a moment after the level started, and
    // takes away one that finds no floor. One is: a box of shells that
    // stands in the corner of a block, flush with it on both of its far
    // sides. A place on a plane counts as in front of it, which is inside
    // the block here, as it is in the original.
    EXPECT_EQ(CountCalls("fell out of level"), 1u);

    // The monster the player stood in front of saw the player, by a line
    // through the level, took the player for its enemy, and came walking.
    // The player does not move, and is hurt.
    EXPECT_EQ(_fields->enemy.Get(*_machine, watcher), player);
    EXPECT_GT(quake::Length(quake::Difference(_fields->origin.Get(*_machine, watcher), monsters.at(watcher))), 16.0f);
    EXPECT_GE(CountHunters(monsters), 1u);
    EXPECT_LT(_fields->health.Get(*_machine, player), 100.0f);
    std::cout << CountHunters(monsters) << " of " << monsters.size() << " monsters came for the player, who has "
      << _fields->health.Get(*_machine, player) << " health left.\n";

    // no monster ended in what is solid by walking
    for (const auto &[monster, start] : monsters)
    {
      if (_machine->IsEntityFree(monster) || _fields->solid.Get(*_machine, monster) == 0.0f) { continue; }
      if (stuck_at_start.contains(monster)) { continue; }

      EXPECT_FALSE(_collision->TestPosition(monster))
        << _fields->classname.GetText(*_machine, monster) << " " << monster;
    }
  }

  TEST_F(QcWorldBuiltinsRealDataTest, RunsTwentySecondsOfEveryLevelOfTheRealGame)
  {
    std::size_t levels = 0;
    std::size_t items = 0;
    std::size_t fallen = 0;
    std::size_t monsters = 0;
    std::size_t placed = 0;
    std::size_t woke = 0;
    std::size_t killed = 0;
    for (const std::string &path : RealData::Get().ListNames("maps"))
    {
      // the models of the boxes of ammunition are in the same folder and
      // format, and are no levels
      const std::string_view file = std::string_view(path).substr(std::string_view("maps/").size());
      if (!file.ends_with(".bsp") || file.starts_with("b_")) { continue; }
      const std::string level(file.substr(0, file.size() - std::string_view(".bsp").size()));

      if (!Start(level)) { continue; }

      EXPECT_THAT(OfTheMachine(_report.failures), IsEmpty()) << level;
      EXPECT_TRUE(_running->ConnectClient(player, "player")) << level;
      const std::map<std::int32_t, LevelVector> starts = FindMonsters();
      const std::int32_t watcher = PutPlayerBefore(starts);
      levels++;
      items += Play(level, 20.0f);
      monsters += starts.size();

      // The monster the player was put in front of woke: it walked, or it
      // hurt the player from where it stood. Most have killed the player
      // by now, who never moves, and have no enemy any more.
      if (watcher != 0)
      {
        placed++;
        const float walked =
          quake::Length(quake::Difference(_fields->origin.Get(*_machine, watcher), starts.at(watcher)));
        const bool is_awake = walked > 16.0f || _fields->health.Get(*_machine, player) < 100.0f;
        EXPECT_TRUE(is_awake) << level << ": " << _fields->classname.GetText(*_machine, watcher) << " " << watcher;
        woke += is_awake ? 1 : 0;
        killed += _fields->health.Get(*_machine, player) <= 0.0f ? 1 : 0;
      }
      fallen += CountCalls("fell out of level");
    }
    EXPECT_GT(levels, 3u);
    EXPECT_GT(items, 100u);
    EXPECT_GT(placed, levels / 2);

    std::cout << levels << " levels, " << items << " items on a floor, " << fallen << " items that found none, "
      << monsters << " monsters. The player was put in front of a monster in " << placed << " levels: it woke in "
      << woke << " and the player was dead after twenty seconds in " << killed << ".\n";
  }
}
