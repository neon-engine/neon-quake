#ifndef QUAKE_LEVEL_MOVING_TEST_HPP
#define QUAKE_LEVEL_MOVING_TEST_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "formats/bsp-hull.hpp"
#include "level-collision.hpp"
#include "level-physics.hpp"
#include "level-running.hpp"
#include "level-stepping.hpp"
#include "level-touching.hpp"
#include "level-world.test.hpp"
#include "qc-fields.hpp"
#include "qc-flag.hpp"
#include "qc-globals.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"

namespace quake
{
  /// The level of `LevelWorld` set up as a host sets a level up, for the
  /// tests of what moves in it: the collision, the level that runs, the
  /// touching, the stepping, and the physics as the mover.
  ///
  /// The program has two functions that note for whom they were called:
  /// `touch` and `blocked`.
  class LevelMovingTest : public ::testing::Test
  {
  protected:
    /// One call of `touch` or `blocked`, as the game code saw it.
    struct Call
    {
      std::int32_t self = 0;
      std::int32_t other = 0;
      float time = 0.0f;

      bool operator==(const Call &) const = default;
    };

    static constexpr float epsilon = BspHull::distance_epsilon;
    static constexpr float tolerance = 1.0e-3f;

    /// A box as wide as the player's that stands on its lowest side.
    static constexpr LevelVector crate_mins = {-16.0f, -16.0f, 0.0f};
    static constexpr LevelVector crate_maxs = {16.0f, 16.0f, 56.0f};

    LevelProgram _program;
    std::int32_t _touch = _program.Function("touch");
    std::int32_t _blocked = _program.Function("blocked");

    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<QcFields> _fields;
    std::unique_ptr<QcGlobals> _globals;
    std::unique_ptr<LevelCollision> _collision;
    std::unique_ptr<LevelRunning> _running;
    std::unique_ptr<LevelTouching> _touching;
    std::unique_ptr<LevelStepping> _stepping;
    std::unique_ptr<LevelPhysics> _physics;

    std::vector<Call> _touches;
    std::vector<Call> _blocks;

    /// What the monsters get for a number of chance. Close to 1 they never
    /// choose by chance.
    float _chance = 0.9f;

    void SetUp() override
    {
      LevelWorld::AddTo(_program);
      _machine = std::make_unique<QcMachine>(_program.Make());
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _globals = std::make_unique<QcGlobals>(_machine->GetProgs());

      const auto note = [this](std::vector<Call> &calls)
      {
        return [this, &calls](QcMachine &machine)
        {
          calls.push_back({_globals->self.Get(machine), _globals->other.Get(machine), _globals->time.Get(machine)});
        };
      };
      _machine->SetBuiltin(_touch, note(_touches));
      _machine->SetBuiltin(_blocked, note(_blocks));

      // the order a host makes them in
      _collision = std::make_unique<LevelCollision>(*_machine);
      std::string error;
      ASSERT_TRUE(_collision->Build(MakeLevel(), error)) << error;
      _running = std::make_unique<LevelRunning>(*_machine);
      _touching = std::make_unique<LevelTouching>(*_collision, *_running);
      _stepping = std::make_unique<LevelStepping>(*_collision, *_touching, [this] { return _chance; });
      _physics = std::make_unique<LevelPhysics>(*_collision, *_touching);
      _running->SetMover(_physics.get());
    }

    /// The level the tests run in, for a fixture that wants another.
    [[nodiscard]] virtual BspFile MakeLevel() const
    {
      return LevelWorld::MakeLevel();
    }

    [[nodiscard]] std::int32_t FunctionOf(const std::string_view name) const
    {
      return _machine->GetProgs().FindFunction(name).value_or(0);
    }

    std::int32_t Make(
      const LevelVector &origin, const LevelVector &mins = crate_mins, const LevelVector &maxs = crate_maxs,
      const QcSolid solid = QcSolid::BoundingBox)
    {
      return LevelWorld::MakeEntity(*_machine, origin, mins, maxs, solid);
    }

    /// Makes an entity that moves in a way and notes what it touches.
    std::int32_t MakeMover(
      const LevelVector &origin, const QcMoveType move_type, const LevelVector &mins = crate_mins,
      const LevelVector &maxs = crate_maxs)
    {
      const std::int32_t entity = Make(origin, mins, maxs);
      _fields->movetype.Set(*_machine, entity, static_cast<float>(move_type));
      _fields->touch.Set(*_machine, entity, FunctionOf("touch"));
      return entity;
    }

    /// Makes a monster with the box of a player that stands at a place on
    /// the ground, `height` being what the floor is at there.
    std::int32_t MakeMonster(const float x, const float y, const float height = 0.0f)
    {
      const std::int32_t monster =
        Make({x, y, height + 24.0f + epsilon}, LevelWorld::player_mins, LevelWorld::player_maxs, QcSolid::SlideBox);
      _fields->movetype.Set(*_machine, monster, static_cast<float>(QcMoveType::Step));
      _fields->flags.Set(
        *_machine, monster,
        static_cast<float>(static_cast<std::int32_t>(QcFlag::Monster) | static_cast<std::int32_t>(QcFlag::OnGround)));
      _fields->yaw_speed.Set(*_machine, monster, 360.0f);
      return monster;
    }

    /// Makes the slab of the level a lift with its top at a place, which
    /// moves with a velocity and notes what blocks it.
    std::int32_t MakeLift(const LevelVector &origin, const LevelVector &velocity)
    {
      const float half = LevelWorld::slab_half_width;
      const std::int32_t lift =
        Make(origin, {-half, -half, -LevelWorld::slab_thickness}, {half, half, 0.0f}, QcSolid::Bsp);
      _fields->model.SetText(*_machine, lift, "*1");
      _fields->movetype.Set(*_machine, lift, static_cast<float>(QcMoveType::Push));
      _fields->velocity.Set(*_machine, lift, velocity);
      _fields->blocked.Set(*_machine, lift, FunctionOf("blocked"));
      // it moves until it thinks, which is far off
      _fields->nextthink.Set(*_machine, lift, 1000.0f);
      return lift;
    }

    [[nodiscard]] LevelVector OriginOf(const std::int32_t entity) const
    {
      return _fields->origin.Get(*_machine, entity);
    }

    [[nodiscard]] bool Has(const std::int32_t entity, const QcFlag flag) const
    {
      return HasFlag(_fields->flags.Get(*_machine, entity), flag);
    }

    void Set(const std::int32_t entity, const QcFlag flag) const
    {
      _fields->flags.Set(*_machine, entity, WithFlag(_fields->flags.Get(*_machine, entity), flag));
    }

    /// Lets a number of frames of a length pass.
    void Advance(const int frames, const float dt = 0.05f) const
    {
      for (int frame = 0; frame < frames; frame++) { _running->Advance(dt); }
    }
  };
} // quake

#endif //QUAKE_LEVEL_MOVING_TEST_HPP
