#include "qc-world-builtins.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-contents.hpp"
#include "formats/bsp-hull.hpp"
#include "level-world.test.hpp"
#include "qc-flag.hpp"

namespace
{
  using quake::BspContents;
  using quake::BspHull;
  using quake::HasFlag;
  using quake::LevelCollision;
  using quake::LevelProgram;
  using quake::LevelVector;
  using quake::LevelWorld;
  using quake::QcFields;
  using quake::QcFlag;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcSolid;
  using quake::QcWorldBuiltins;
  using ::testing::ElementsAre;

  constexpr float epsilon = BspHull::distance_epsilon;
  constexpr float tolerance = 1.0e-3f;

  /// The builtins on the level of `LevelWorld`, called as game code calls
  /// them: the parameters put in place, then the function of the name.
  class QcWorldBuiltinsTest : public ::testing::Test
  {
  protected:
    static constexpr LevelVector crate_mins = {-16.0f, -16.0f, 0.0f};
    static constexpr LevelVector crate_maxs = {16.0f, 16.0f, 56.0f};

    LevelProgram _program;
    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<QcFields> _fields;
    std::unique_ptr<QcGlobals> _globals;
    std::unique_ptr<LevelCollision> _collision;
    std::unique_ptr<QcWorldBuiltins> _builtins;

    void SetUp() override
    {
      LevelWorld::AddTo(_program);
      _machine = std::make_unique<QcMachine>(_program.Make());
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _globals = std::make_unique<QcGlobals>(_machine->GetProgs());
      _collision = std::make_unique<LevelCollision>(*_machine);

      std::string error;
      ASSERT_TRUE(_collision->Build(LevelWorld::MakeLevel(), error)) << error;
      _builtins = std::make_unique<QcWorldBuiltins>(*_collision);
      _builtins->Register(*_machine);
    }

    std::int32_t Make(
      const LevelVector &origin, const LevelVector &mins = crate_mins, const LevelVector &maxs = crate_maxs,
      const QcSolid solid = QcSolid::BoundingBox)
    {
      return LevelWorld::MakeEntity(*_machine, origin, mins, maxs, solid);
    }

    void TraceLine(const LevelVector &start, const LevelVector &end, const float no_monsters, const std::int32_t pass)
    {
      _machine->SetParameterVector(0, start);
      _machine->SetParameterVector(1, end);
      _machine->SetParameterFloat(2, no_monsters);
      _machine->SetParameterInteger(3, pass);
      ASSERT_TRUE(_machine->Call("traceline")) << _machine->GetError().message;
    }

    /// Calls `droptofloor` for an entity and gives what it returned.
    float DropToFloor(const std::int32_t entity)
    {
      _globals->self.Set(*_machine, entity);
      EXPECT_TRUE(_machine->Call("droptofloor")) << _machine->GetError().message;
      return _machine->GetReturnFloat();
    }

    /// The entities `findradius` gives, in the order of its list.
    std::vector<std::int32_t> FindRadius(const LevelVector &place, const float radius)
    {
      _machine->SetParameterVector(0, place);
      _machine->SetParameterFloat(1, radius);
      EXPECT_TRUE(_machine->Call("findradius")) << _machine->GetError().message;

      std::vector<std::int32_t> found;
      for (std::int32_t entity = _machine->GetReturnInteger(); entity != 0 && found.size() < 100;
           entity = _fields->chain.Get(*_machine, entity))
      {
        found.push_back(entity);
      }
      return found;
    }

    std::int32_t CheckClient(const float time)
    {
      _globals->time.Set(*_machine, time);
      EXPECT_TRUE(_machine->Call("checkclient")) << _machine->GetError().message;
      return _machine->GetReturnInteger();
    }
  };

  TEST_F(QcWorldBuiltinsTest, LeavesWhatALineHitInTheGlobalsOfTheGameCode)
  {
    const std::int32_t shooter = Make({-80.0f, 0.0f, 0.0f});
    const std::int32_t crate = Make({150.0f, 0.0f, 16.0f});

    TraceLine({-50.0f, 0.0f, 40.0f}, {250.0f, 0.0f, 40.0f}, 0.0f, shooter);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), crate);
    EXPECT_NEAR(_globals->trace_fraction.Get(*_machine), (184.0f - epsilon) / 300.0f, tolerance);
    EXPECT_NEAR(_globals->trace_endpos.Get(*_machine)[0], 134.0f - epsilon, tolerance);
    EXPECT_THAT(_globals->trace_plane_normal.Get(*_machine), ElementsAre(-1.0f, 0.0f, 0.0f));
    EXPECT_NEAR(_globals->trace_plane_dist.Get(*_machine), -134.0f, tolerance);
    EXPECT_EQ(_globals->trace_allsolid.Get(*_machine), 0.0f);
    EXPECT_EQ(_globals->trace_startsolid.Get(*_machine), 0.0f);
    EXPECT_EQ(_globals->trace_inopen.Get(*_machine), 1.0f);
    EXPECT_EQ(_globals->trace_inwater.Get(*_machine), 0.0f);

    // without monsters it is the side of the ledge, a part of the world
    TraceLine({-50.0f, 0.0f, 40.0f}, {250.0f, 0.0f, 40.0f}, 1.0f, shooter);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), 0);
    EXPECT_NEAR(_globals->trace_endpos.Get(*_machine)[0], LevelWorld::ledge_edge - epsilon, tolerance);

    // the crate is passed
    TraceLine({-50.0f, 0.0f, 40.0f}, {250.0f, 0.0f, 40.0f}, 0.0f, crate);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), 0);

    // a line that hits nothing names the world too, and ends where it was
    // meant to
    TraceLine({-50.0f, 0.0f, 40.0f}, {50.0f, 0.0f, 40.0f}, 0.0f, shooter);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), 0);
    EXPECT_EQ(_globals->trace_fraction.Get(*_machine), 1.0f);
    EXPECT_THAT(_globals->trace_endpos.Get(*_machine), ElementsAre(50.0f, 0.0f, 40.0f));

    // one that starts in the wall
    TraceLine({350.0f, 0.0f, 40.0f}, {360.0f, 0.0f, 40.0f}, 0.0f, shooter);
    EXPECT_EQ(_globals->trace_allsolid.Get(*_machine), 1.0f);
    EXPECT_EQ(_globals->trace_startsolid.Get(*_machine), 1.0f);
    EXPECT_EQ(_globals->trace_inopen.Get(*_machine), 0.0f);
  }

  TEST_F(QcWorldBuiltinsTest, HitsAMonsterBesideALineWithTheLineOfAMissile)
  {
    const std::int32_t shooter = Make({-80.0f, 0.0f, 0.0f});
    const std::int32_t monster = Make({150.0f, 30.0f, 16.0f}, crate_mins, crate_maxs, QcSolid::SlideBox);
    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));

    TraceLine({-50.0f, 0.0f, 40.0f}, {250.0f, 0.0f, 40.0f}, 0.0f, shooter);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), 0);
    TraceLine({-50.0f, 0.0f, 40.0f}, {250.0f, 0.0f, 40.0f}, 2.0f, shooter);
    EXPECT_EQ(_globals->trace_ent.Get(*_machine), monster);
  }

  TEST_F(QcWorldBuiltinsTest, SaysWhatFillsAPlace)
  {
    _machine->SetParameterVector(0, {0.0f, 0.0f, 10.0f});
    ASSERT_TRUE(_machine->Call("pointcontents"));
    EXPECT_EQ(_machine->GetReturnFloat(), static_cast<float>(BspContents::Empty));

    _machine->SetParameterVector(0, {0.0f, 0.0f, -10.0f});
    ASSERT_TRUE(_machine->Call("pointcontents"));
    EXPECT_EQ(_machine->GetReturnFloat(), static_cast<float>(BspContents::Solid));
  }

  TEST_F(QcWorldBuiltinsTest, DropsAnEntityOntoTheFloor)
  {
    const std::int32_t item = Make({0.0f, 0.0f, 100.0f}, crate_mins, crate_maxs, QcSolid::Trigger);
    _fields->groundentity.Set(*_machine, item, 5);

    EXPECT_EQ(DropToFloor(item), 1.0f);
    const LevelVector origin = _fields->origin.Get(*_machine, item);
    EXPECT_THAT(origin, ElementsAre(0.0f, 0.0f, ::testing::FloatNear(epsilon, tolerance)));
    EXPECT_TRUE(HasFlag(_fields->flags.Get(*_machine, item), QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, item), 0);

    // the box it is found by is where it is now
    EXPECT_NEAR(_fields->absmin.Get(*_machine, item)[2], epsilon - 1.0f, tolerance);
    EXPECT_NEAR(_fields->absmax.Get(*_machine, item)[2], epsilon + 57.0f, tolerance);
  }

  TEST_F(QcWorldBuiltinsTest, DropsAnEntityOntoTheBoxOfAnother)
  {
    const std::int32_t crate = Make({0.0f, 0.0f, 0.0f});
    const std::int32_t item = Make({10.0f, 0.0f, 100.0f}, {-8.0f, -8.0f, 0.0f}, {8.0f, 8.0f, 16.0f});

    EXPECT_EQ(DropToFloor(item), 1.0f);
    EXPECT_NEAR(_fields->origin.Get(*_machine, item)[2], 56.0f + epsilon, tolerance);
    EXPECT_EQ(_fields->groundentity.Get(*_machine, item), crate);
    EXPECT_TRUE(HasFlag(_fields->flags.Get(*_machine, item), QcFlag::OnGround));

    // beside the crate it is the floor
    _fields->origin.Set(*_machine, item, {40.0f, 0.0f, 100.0f});
    EXPECT_EQ(DropToFloor(item), 1.0f);
    EXPECT_NEAR(_fields->origin.Get(*_machine, item)[2], epsilon, tolerance);
    EXPECT_EQ(_fields->groundentity.Get(*_machine, item), 0);
  }

  TEST_F(QcWorldBuiltinsTest, DropsNothingThatHasNoFloorNearOrIsInsideWhatIsSolid)
  {
    // the floor is further down than it looks
    const std::int32_t high = Make({0.0f, 0.0f, QcWorldBuiltins::drop_distance + 10.0f});
    EXPECT_EQ(DropToFloor(high), 0.0f);
    EXPECT_THAT(_fields->origin.Get(*_machine, high), ElementsAre(0.0f, 0.0f, QcWorldBuiltins::drop_distance + 10.0f));
    EXPECT_FALSE(HasFlag(_fields->flags.Get(*_machine, high), QcFlag::OnGround));

    const std::int32_t buried = Make({0.0f, 0.0f, -400.0f});
    EXPECT_EQ(DropToFloor(buried), 0.0f);
    EXPECT_THAT(_fields->origin.Get(*_machine, buried), ElementsAre(0.0f, 0.0f, -400.0f));
    EXPECT_FALSE(HasFlag(_fields->flags.Get(*_machine, buried), QcFlag::OnGround));
  }

  TEST_F(QcWorldBuiltinsTest, ListsTheEntitiesNearAPlaceThroughTheirChain)
  {
    const std::int32_t near = Make({10.0f, 0.0f, 0.0f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f});
    const std::int32_t far = Make({90.0f, 0.0f, 0.0f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f});
    const std::int32_t ghost = Make({0.0f, 5.0f, 0.0f}, {}, {}, QcSolid::Not);
    const std::int32_t trigger = Make({0.0f, -5.0f, 0.0f}, {}, {}, QcSolid::Trigger);
    // its origin is far and the middle of its box is near
    const std::int32_t lopsided = Make({0.0f, 100.0f, 0.0f}, {-10.0f, -120.0f, 0.0f}, {10.0f, -80.0f, 0.0f});
    const std::int32_t gone = Make({1.0f, 1.0f, 1.0f});
    _machine->FreeEntity(gone);

    // the last one found is the first of the list
    EXPECT_THAT(FindRadius({0.0f, 0.0f, 0.0f}, 20.0f), ElementsAre(lopsided, trigger, near));
    EXPECT_THAT(FindRadius({0.0f, 0.0f, 0.0f}, 100.0f), ElementsAre(lopsided, trigger, far, near));
    EXPECT_THAT(FindRadius({0.0f, 0.0f, 500.0f}, 20.0f), ElementsAre());
    EXPECT_NE(ghost, 0);
  }

  TEST_F(QcWorldBuiltinsTest, GivesAMonsterAPlayerWhoIsAliveAndMayBeLookedFor)
  {
    const std::int32_t player = Make({0.0f, 0.0f, 24.0f});
    ASSERT_EQ(player, 1);

    // not alive yet
    EXPECT_EQ(CheckClient(1.0f), 0);
    _fields->health.Set(*_machine, player, 100.0f);
    EXPECT_EQ(CheckClient(1.0f), player);

    _fields->flags.Set(*_machine, player, static_cast<float>(QcFlag::NoTarget));
    EXPECT_EQ(CheckClient(1.0f), 0);
    _fields->flags.Set(*_machine, player, static_cast<float>(QcFlag::Client));
    EXPECT_EQ(CheckClient(1.0f), player);

    _builtins->SetClientCount(0);
    EXPECT_EQ(CheckClient(1.0f), 0);
  }

  TEST_F(QcWorldBuiltinsTest, GivesEachOfSeveralPlayersATurn)
  {
    const std::int32_t first = Make({0.0f, 0.0f, 24.0f});
    const std::int32_t second = Make({50.0f, 0.0f, 24.0f});
    const std::int32_t monster = Make({-50.0f, 0.0f, 24.0f});
    for (const std::int32_t entity : {first, second, monster}) { _fields->health.Set(*_machine, entity, 100.0f); }

    _builtins->SetClientCount(2);
    EXPECT_EQ(_builtins->GetClientCount(), 2);
    EXPECT_EQ(CheckClient(1.05f), first);
    EXPECT_EQ(CheckClient(1.15f), second);
    EXPECT_EQ(CheckClient(1.25f), first);

    // the turn of one who is dead goes to the other
    _fields->health.Set(*_machine, second, 0.0f);
    EXPECT_EQ(CheckClient(1.15f), first);
    _machine->FreeEntity(first);
    EXPECT_EQ(CheckClient(1.15f), 0);
  }

  TEST_F(QcWorldBuiltinsTest, AimsWhereThePlayerLooks)
  {
    _globals->v_forward.Set(*_machine, {0.6f, 0.0f, 0.8f});
    _machine->SetParameterInteger(0, 1);
    _machine->SetParameterFloat(1, 1000.0f);
    ASSERT_TRUE(_machine->Call("aim"));
    EXPECT_THAT(_machine->GetReturnVector(), ElementsAre(0.6f, 0.0f, 0.8f));
  }
}
