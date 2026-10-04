#include "player-movement.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-contents.hpp"
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
#include "player-command.hpp"
#include "qc-core-builtins.hpp"
#include "qc-flag.hpp"
#include "qc-recording-host.test.hpp"
#include "qc-world-builtins.hpp"

// A player moved as the original moves one, in the levels of a real game and
// with its game code: the game code puts the player in, the player is
// steered and walked, and jumping and what water does are the game code's.
namespace
{
  using quake::BspContents;
  using quake::BspFile;
  using quake::EntityText;
  using quake::HasFlag;
  using quake::LevelCollision;
  using quake::LevelPhysics;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelStepping;
  using quake::LevelTouching;
  using quake::LevelTraceKind;
  using quake::LevelTraceResult;
  using quake::LevelVector;
  using quake::LevelWorldHost;
  using quake::PlayerCommand;
  using quake::PlayerMovement;
  using quake::PlayerMovementSettings;
  using quake::Progs;
  using quake::QcCoreBuiltins;
  using quake::QcFields;
  using quake::QcFlag;
  using quake::QcMachine;
  using quake::QcRecordingHost;
  using quake::QcWorldBuiltins;
  using quake::RealData;

  class PlayerMovementRealDataTest : public ::testing::Test
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
    std::unique_ptr<PlayerMovement> _movement;

    /// Where the player looks, which is a host's to keep.
    LevelVector _view{};

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      std::string error;
      ASSERT_TRUE(_progs.Read(data.GetBytes("progs.dat"), error)) << error;
    }

    /// Starts a level as a host does that has the player moved as the
    /// original moves one, and lets the player in.
    void Enter(const std::string_view name)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      ASSERT_TRUE(_level.Read(RealData::Get().GetBytes(path), error)) << path << ": " << error;

      EntityText text;
      ASSERT_TRUE(text.Read(_level.entities, error)) << path << ": " << error;

      _machine = std::make_unique<QcMachine>(_progs);
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _host = std::make_unique<QcRecordingHost>();

      _core = std::make_unique<QcCoreBuiltins>(*_host);
      _core->Register(*_machine);
      _core->SeedRandom(1);
      _core->GetVariables().Set("skill", "1");
      _core->GetModels().Add(path);

      _collision = std::make_unique<LevelCollision>(*_machine);
      ASSERT_TRUE(_collision->Build(_level, error)) << path << ": " << error;
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
      spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = 1});
      _running->Advance(0.1f);
      _running->Advance(0.1f);

      // what switches a host over: the mover walks the players, the level
      // gives the one player a whole frame, and the player is steered
      _physics->SetWalksClients(true);
      _physics->SetGravity(_core->GetVariables().GetFloat("sv_gravity"));
      _running->SetClientCount(1);
      _movement = std::make_unique<PlayerMovement>(*_collision);
      _movement->SetSettings(PlayerMovementSettings::From(_core->GetVariables()));

      ASSERT_TRUE(_running->ConnectClient(player, "player"));
    }

    /// A step as a host makes it. The game code may have turned the
    /// player, which the host then looks along.
    void Step(PlayerCommand command = {})
    {
      if (_fields->fixangle.Get(*_machine, player) != 0.0f)
      {
        _view = _fields->angles.Get(*_machine, player);
        _fields->fixangle.Set(*_machine, player, 0.0f);
      }
      command.view_angles = _view;
      _movement->Steer(player, command, static_cast<float>(_running->GetTime()), frame_time);
      _running->Advance(frame_time);
    }

    void Play(const float seconds, const PlayerCommand &command = {})
    {
      const int frames = static_cast<int>(seconds / frame_time);
      for (int frame = 0; frame < frames; frame++) { Step(command); }
    }

    [[nodiscard]] LevelVector Origin() const
    {
      return _fields->origin.Get(*_machine, player);
    }

    [[nodiscard]] bool IsOnGround() const
    {
      return HasFlag(_fields->flags.Get(*_machine, player), QcFlag::OnGround);
    }

    /// The direction along the ground the player gets furthest in from
    /// where the player stands, as a yaw, and how far that is: the box of
    /// the player moved a step's height over the floor.
    [[nodiscard]] std::pair<float, float> FindOpenWay() const
    {
      constexpr float reach = 400.0f;
      const LevelVector origin = quake::Sum(Origin(), {0.0f, 0.0f, LevelPhysics::step_height});
      float best_yaw = 0.0f;
      float best = 0.0f;
      for (int step = 0; step < 16; step++)
      {
        const float yaw = static_cast<float>(step) * 22.5f;
        const float turn = yaw * std::numbers::pi_v<float> / 180.0f;
        const LevelVector end = quake::Sum(origin, {std::cos(turn) * reach, std::sin(turn) * reach, 0.0f});
        const LevelTraceResult trace = _collision->Trace(
          origin, _fields->mins.Get(*_machine, player), _fields->maxs.Get(*_machine, player), end,
          LevelTraceKind::Normal, player);
        if (trace.fraction * reach > best)
        {
          best = trace.fraction * reach;
          best_yaw = yaw;
        }
      }
      return {best_yaw, best};
    }

    /// A place of the level where a player is under water with all of
    /// the box free, and has water under the feet to sink in.
    [[nodiscard]] std::optional<LevelVector> FindWater() const
    {
      const auto is_water = [this](const LevelVector &point)
      {
        return _collision->GetPointContents(point) == BspContents::Water;
      };
      const quake::BspModel &world = _level.models[0];
      const LevelVector before = Origin();
      std::optional<LevelVector> found;
      for (float z = world.maxs.z; z > world.mins.z && !found; z -= 32.0f)
      {
        for (float y = world.mins.y; y < world.maxs.y && !found; y += 32.0f)
        {
          for (float x = world.mins.x; x < world.maxs.x && !found; x += 32.0f)
          {
            const LevelVector place = {x, y, z};
            if (!is_water(place) || !is_water({x, y, z + 24.0f}) || !is_water({x, y, z - 70.0f})) { continue; }

            _fields->origin.Set(*_machine, player, place);
            if (!_collision->TestPosition(player)) { found = place; }
          }
        }
      }
      _fields->origin.Set(*_machine, player, before);
      return found;
    }

    /// Stands still, walks, and jumps in a level, from where the game
    /// code puts a player.
    void StandWalkAndJump(const std::string_view name)
    {
      Enter(name);
      if (HasFatalFailure()) { return; }

      // the game code puts a player a little over the floor
      Play(0.5f);
      ASSERT_TRUE(IsOnGround()) << name;
      EXPECT_EQ(_fields->waterlevel.Get(*_machine, player), 0.0f);

      // a second of standing: not a tenth of a unit of shaking
      const LevelVector start = Origin();
      for (int frame = 0; frame < 72; frame++)
      {
        Step();
        ASSERT_LT(quake::Length(quake::Difference(Origin(), start)), 0.1f) << name << ", frame " << frame;
        ASSERT_TRUE(IsOnGround()) << name << ", frame " << frame;
      }

      // a jump is the game code's: it sees the button and that the player
      // stands, and throws the player up at 270
      _fields->button2.Set(*_machine, player, 1.0f);
      Step();
      _fields->button2.Set(*_machine, player, 0.0f);
      EXPECT_FALSE(IsOnGround()) << name;
      float highest = Origin()[2];
      int frames_in_the_air = 1;
      for (; frames_in_the_air < 144 && !IsOnGround(); frames_in_the_air++)
      {
        Step();
        highest = std::max(highest, Origin()[2]);
        ASSERT_FALSE(_collision->TestPosition(player)) << name;
      }
      EXPECT_TRUE(IsOnGround()) << name;
      EXPECT_NEAR(highest - start[2], 45.0f, 2.0f) << name;
      EXPECT_NEAR(Origin()[2], start[2], 0.1f) << name;
      // up and down at 800 a second each second takes two thirds of a second
      EXPECT_NEAR(static_cast<float>(frames_in_the_air) * frame_time, 0.675f, 0.05f) << name;

      // Walking where there is the most room, and from where that ends
      // on where there is the most room then: three seconds of it, never
      // in what is solid.
      float walked = 0.0f;
      float fastest = 0.0f;
      float yaw = 0.0f;
      for (int frame = 0; frame < 216; frame++)
      {
        const LevelVector velocity = _fields->velocity.Get(*_machine, player);
        const float speed = std::hypot(velocity[0], velocity[1]);
        fastest = std::max(fastest, speed);
        if (frame == 0 || (speed < 100.0f && frame % 36 == 0))
        {
          const auto [open_yaw, room] = FindOpenWay();
          ASSERT_GT(room, 100.0f) << name << ", frame " << frame;
          yaw = open_yaw;
          _view = {0.0f, yaw, 0.0f};
        }

        const LevelVector from = Origin();
        Step({.forward_move = 400.0f});
        ASSERT_FALSE(_collision->TestPosition(player)) << name << ", frame " << frame;
        walked += quake::Length(quake::Difference(Origin(), from));
      }
      EXPECT_GT(walked, 300.0f) << name;
      EXPECT_NEAR(fastest, 320.0f, 0.5f) << name;
      EXPECT_GT(_fields->health.Get(*_machine, player), 0.0f) << name;

      std::cout << name << ": the player jumped " << highest - start[2] << " units high, and walked " << walked
        << " units in three seconds, at up to " << fastest << " a second.\n";
    }
  };

  TEST_F(PlayerMovementRealDataTest, StandsWalksAndJumpsWhereTheGameStarts)
  {
    StandWalkAndJump("start");
  }

  TEST_F(PlayerMovementRealDataTest, StandsWalksAndJumpsInTheFirstLevel)
  {
    StandWalkAndJump("lq_e1m1");
  }

  TEST_F(PlayerMovementRealDataTest, SwimsInTheWaterOfALevel)
  {
    // the first level has no water deep enough to swim in
    Enter("lq_e1m3");
    if (HasFatalFailure()) { return; }
    Play(0.25f);

    const std::optional<LevelVector> water = FindWater();
    ASSERT_TRUE(water.has_value());
    _fields->origin.Set(*_machine, player, *water);
    _fields->velocity.Set(*_machine, player, {});
    _collision->Link(player);

    // nothing asked: under water to the eyes, sinking slowly, standing
    // on nothing
    Play(0.5f);
    EXPECT_EQ(_fields->waterlevel.Get(*_machine, player), 3.0f);
    EXPECT_EQ(_fields->watertype.Get(*_machine, player), static_cast<float>(BspContents::Water));
    EXPECT_FALSE(IsOnGround());
    const float sunk = (*water)[2] - Origin()[2];
    EXPECT_GT(sunk, 5.0f);
    EXPECT_LT(sunk, 30.0f);
    EXPECT_LT(_fields->velocity.Get(*_machine, player)[2], 0.0f);
    EXPECT_GT(_fields->velocity.Get(*_machine, player)[2], -42.5f);

    // and up when asked
    const float lowest = Origin()[2];
    float fastest_rise = 0.0f;
    for (int frame = 0; frame < 18; frame++)
    {
      Step({.up_move = 320.0f});
      fastest_rise = std::max(fastest_rise, _fields->velocity.Get(*_machine, player)[2]);
    }
    EXPECT_GT(Origin()[2], lowest + 10.0f);
    EXPECT_GT(fastest_rise, 50.0f);
    EXPECT_FALSE(_collision->TestPosition(player));
    EXPECT_FALSE(IsOnGround());

    std::cout << "The player sank " << sunk << " units in half a second at " << (*water)[0] << " " << (*water)[1]
      << " " << (*water)[2] << ", and swam up " << Origin()[2] - lowest << " in a quarter.\n";
  }
}
