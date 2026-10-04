#include "level-collision.hpp"

#include <cstdint>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-hull.hpp"
#include "level-world.test.hpp"
#include "qc-flag.hpp"
#include "qc-move-type.hpp"

namespace
{
  using quake::BspContents;
  using quake::BspFile;
  using quake::BspHull;
  using quake::LevelBox;
  using quake::LevelCollision;
  using quake::LevelProgram;
  using quake::LevelTraceKind;
  using quake::LevelTraceResult;
  using quake::LevelVector;
  using quake::LevelWorld;
  using quake::QcFields;
  using quake::QcFlag;
  using quake::QcMachine;
  using quake::QcMoveType;
  using quake::QcSolid;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;

  constexpr float epsilon = BspHull::distance_epsilon;
  constexpr float tolerance = 1.0e-3f;
  constexpr std::int32_t nobody = LevelCollision::no_entity;

  /// The level of `LevelWorld` with the entities a test puts in it.
  class LevelCollisionTest : public ::testing::Test
  {
  protected:
    /// A box that is a point's width under the player's: it moves through
    /// hull 1, and stands where a player would.
    static constexpr LevelVector crate_mins = {-16.0f, -16.0f, 0.0f};
    static constexpr LevelVector crate_maxs = {16.0f, 16.0f, 56.0f};

    LevelProgram _program;
    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<QcFields> _fields;
    std::unique_ptr<LevelCollision> _collision;

    void SetUp() override
    {
      LevelWorld::AddTo(_program);
      _machine = std::make_unique<QcMachine>(_program.Make());
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _collision = std::make_unique<LevelCollision>(*_machine);

      std::string error;
      ASSERT_TRUE(_collision->Build(LevelWorld::MakeLevel(), error)) << error;
      EXPECT_EQ(_collision->GetModelCount(), 2u);
    }

    std::int32_t Make(
      const LevelVector &origin, const LevelVector &mins, const LevelVector &maxs,
      const QcSolid solid = QcSolid::BoundingBox)
    {
      return LevelWorld::MakeEntity(*_machine, origin, mins, maxs, solid);
    }

    /// A crate that stands on the step, in the way of a line from the west
    /// to the side of the ledge.
    std::int32_t MakeCrate()
    {
      return Make({150.0f, 0.0f, 16.0f}, crate_mins, crate_maxs);
    }

    /// The slab of the level as a lift, with its top at a place.
    std::int32_t MakeLift(const LevelVector &origin)
    {
      const float half = LevelWorld::slab_half_width;
      const std::int32_t lift =
        Make(origin, {-half, -half, -LevelWorld::slab_thickness}, {half, half, 0.0f}, QcSolid::Bsp);
      _fields->model.SetText(*_machine, lift, "*1");
      _fields->movetype.Set(*_machine, lift, static_cast<float>(QcMoveType::Push));
      return lift;
    }

    /// Moves a point 40 units above the floor from the west to the east,
    /// into the side of the ledge.
    [[nodiscard]] LevelTraceResult TraceEast(
      const LevelTraceKind kind = LevelTraceKind::Normal, const std::int32_t pass = nobody) const
    {
      return _collision->Trace({-50.0f, 0.0f, 40.0f}, {}, {}, {250.0f, 0.0f, 40.0f}, kind, pass);
    }
  };

  TEST_F(LevelCollisionTest, IsEmptySpaceBeforeItIsGivenALevel)
  {
    const LevelCollision collision(*_machine);
    EXPECT_EQ(collision.GetModelCount(), 0u);
    EXPECT_EQ(collision.GetPointContents({0.0f, 0.0f, -50.0f}), BspContents::Empty);

    const LevelTraceResult result = collision.Trace({0.0f, 0.0f, 50.0f}, {}, {}, {0.0f, 0.0f, -50.0f},
                                                    LevelTraceKind::Normal, nobody);
    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.HasEntity());
    EXPECT_THAT(result.end_position, ElementsAre(0.0f, 0.0f, -50.0f));
  }

  TEST_F(LevelCollisionTest, RefusesALevelWithAModelThatIsWrongAndStaysAsItWas)
  {
    BspFile level = LevelWorld::MakeLevel();
    level.models[1].head_nodes[1] = 10000;

    std::string error;
    EXPECT_FALSE(_collision->Build(level, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(_collision->GetModelCount(), 2u);
    EXPECT_EQ(_collision->GetPointContents({0.0f, 0.0f, -50.0f}), BspContents::Solid);
  }

  TEST_F(LevelCollisionTest, StopsAMoveAtTheWorldAndSaysItWasTheWorld)
  {
    // a point that falls onto the floor
    LevelTraceResult result = _collision->Trace({0.0f, 0.0f, 50.0f}, {}, {}, {0.0f, 0.0f, -50.0f},
                                                LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[2], epsilon, tolerance);
    EXPECT_THAT(result.plane_normal, ElementsAre(0.0f, 0.0f, 1.0f));
    EXPECT_EQ(result.entity, 0);
    EXPECT_TRUE(result.in_open);
    EXPECT_FALSE(result.start_solid);

    // the box of a player comes to stand with its feet on the floor
    result = _collision->Trace({0.0f, 0.0f, 80.0f}, LevelWorld::player_mins, LevelWorld::player_maxs,
                               {0.0f, 0.0f, -50.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[2], 24.0f + epsilon, tolerance);
    EXPECT_EQ(result.entity, 0);

    // a move that meets nothing was stopped by nothing
    result = _collision->Trace({0.0f, 0.0f, 50.0f}, {}, {}, {50.0f, 0.0f, 50.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_EQ(result.fraction, 1.0f);
    EXPECT_FALSE(result.HasEntity());
    EXPECT_THAT(result.end_position, ElementsAre(50.0f, 0.0f, 50.0f));

    // one that starts in the wall never left it
    result = _collision->Trace({350.0f, 0.0f, 50.0f}, {}, {}, {360.0f, 0.0f, 50.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_TRUE(result.all_solid);
    EXPECT_TRUE(result.start_solid);
    EXPECT_EQ(result.entity, 0);
  }

  TEST_F(LevelCollisionTest, SaysWhatFillsAPlaceOfTheWorld)
  {
    EXPECT_EQ(_collision->GetPointContents({0.0f, 0.0f, 10.0f}), BspContents::Empty);
    EXPECT_EQ(_collision->GetPointContents({0.0f, 0.0f, -10.0f}), BspContents::Solid);
    EXPECT_EQ(_collision->GetPointContents({-150.0f, 0.0f, -10.0f}), BspContents::Empty);
    EXPECT_EQ(_collision->GetPointContents({350.0f, 0.0f, 500.0f}), BspContents::Solid);

    // an entity is not asked
    MakeCrate();
    EXPECT_EQ(_collision->GetPointContents({150.0f, 0.0f, 40.0f}), BspContents::Empty);
  }

  TEST_F(LevelCollisionTest, HitsTheBoxOfAnEntityBeforeTheWallBehindIt)
  {
    // without the crate the line reaches the side of the ledge
    LevelTraceResult result = TraceEast();
    EXPECT_NEAR(result.end_position[0], LevelWorld::ledge_edge - epsilon, tolerance);
    EXPECT_EQ(result.entity, 0);

    const std::int32_t crate = MakeCrate();
    result = TraceEast();
    EXPECT_NEAR(result.end_position[0], 150.0f - 16.0f - epsilon, tolerance);
    EXPECT_THAT(result.end_position, ElementsAre(::testing::_, 0.0f, 40.0f));
    EXPECT_THAT(result.plane_normal, ElementsAre(-1.0f, 0.0f, 0.0f));
    EXPECT_NEAR(result.plane_distance, -134.0f, tolerance);
    EXPECT_EQ(result.entity, crate);
    EXPECT_LT(result.fraction, 1.0f);

    // a box that moves is stopped where the two boxes meet
    result = _collision->Trace({-50.0f, 0.0f, 40.0f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f},
                               {250.0f, 0.0f, 40.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[0], 150.0f - 16.0f - 8.0f - epsilon, tolerance);
    EXPECT_EQ(result.entity, crate);

    // a line over the crate goes on to the wall
    result = _collision->Trace({-50.0f, 0.0f, 80.0f}, {}, {}, {350.0f, 0.0f, 80.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[0], LevelWorld::wall_edge - epsilon, tolerance);
    EXPECT_EQ(result.entity, 0);
  }

  TEST_F(LevelCollisionTest, LeavesOutWhatStopsNothingAndWhatIsFree)
  {
    const std::int32_t crate = MakeCrate();
    for (const QcSolid solid : {QcSolid::Not, QcSolid::Trigger})
    {
      _fields->solid.Set(*_machine, crate, static_cast<float>(solid));
      EXPECT_EQ(TraceEast().entity, 0);
    }

    _fields->solid.Set(*_machine, crate, static_cast<float>(QcSolid::SlideBox));
    EXPECT_EQ(TraceEast().entity, crate);
    _machine->FreeEntity(crate);
    EXPECT_EQ(TraceEast().entity, 0);
  }

  TEST_F(LevelCollisionTest, PassesAnEntityWhatItOwnsAndWhatOwnsIt)
  {
    const std::int32_t crate = MakeCrate();
    const std::int32_t shooter = Make({-80.0f, 0.0f, 0.0f}, crate_mins, crate_maxs);

    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, shooter).entity, crate);
    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, crate).entity, 0);

    // what the passed one owns
    _fields->owner.Set(*_machine, crate, shooter);
    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, shooter).entity, 0);
    EXPECT_EQ(TraceEast().entity, crate);

    // and what owns the passed one
    _fields->owner.Set(*_machine, crate, 0);
    _fields->owner.Set(*_machine, shooter, crate);
    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, shooter).entity, 0);
  }

  TEST_F(LevelCollisionTest, PassesWhatHasNoOwnerWhenTheWorldIsPassedAsTheOriginalDoes)
  {
    const std::int32_t crate = MakeCrate();
    const std::int32_t shooter = Make({-80.0f, 0.0f, 0.0f}, crate_mins, crate_maxs);

    // the world owns what names no owner
    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, 0).entity, 0);
    _fields->owner.Set(*_machine, crate, shooter);
    EXPECT_EQ(TraceEast(LevelTraceKind::Normal, 0).entity, crate);
  }

  TEST_F(LevelCollisionTest, LetsOnlyPartsOfTheLevelStopAMoveWithoutMonsters)
  {
    MakeCrate();
    EXPECT_EQ(TraceEast(LevelTraceKind::NoMonsters).entity, 0);

    const std::int32_t lift = MakeLift({50.0f, 0.0f, 44.0f});
    const LevelTraceResult result = TraceEast(LevelTraceKind::NoMonsters);
    EXPECT_EQ(result.entity, lift);
    EXPECT_NEAR(result.end_position[0], 50.0f - LevelWorld::slab_half_width - epsilon, tolerance);
  }

  TEST_F(LevelCollisionTest, LeavesAnEntityWithoutASizeOutForOneThatHasASize)
  {
    const std::int32_t spot = Make({150.0f, 0.0f, 40.0f}, {}, {});
    const std::int32_t mover = Make({-80.0f, 0.0f, 0.0f}, crate_mins, crate_maxs);
    const std::int32_t point = Make({-80.0f, 50.0f, 0.0f}, {}, {});

    const auto trace = [this](const std::int32_t pass)
    {
      return _collision->Trace({-50.0f, 0.0f, 40.0f}, {-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f},
                               {250.0f, 0.0f, 40.0f}, LevelTraceKind::Normal, pass);
    };
    EXPECT_EQ(trace(nobody).entity, spot);
    EXPECT_EQ(trace(point).entity, spot);
    EXPECT_EQ(trace(mover).entity, 0);
  }

  TEST_F(LevelCollisionTest, HitsAMonsterInALargerBoxWithAMissile)
  {
    // the line passes its box, 14 units to the side
    const std::int32_t monster = Make({150.0f, 30.0f, 16.0f}, crate_mins, crate_maxs, QcSolid::SlideBox);
    EXPECT_EQ(TraceEast(LevelTraceKind::Missile).entity, 0);

    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));
    EXPECT_EQ(TraceEast().entity, 0);
    const LevelTraceResult result = TraceEast(LevelTraceKind::Missile);
    EXPECT_EQ(result.entity, monster);
    EXPECT_NEAR(result.end_position[0], 150.0f - 16.0f - LevelCollision::missile_margin - epsilon, tolerance);
  }

  TEST_F(LevelCollisionTest, StopsAMoveWithTheShapeOfAPartOfTheLevelWhereItsEntityStands)
  {
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f});
    const auto drop_point = [this]
    {
      return _collision->Trace({0.0f, 0.0f, 100.0f}, {}, {}, {0.0f, 0.0f, 10.0f}, LevelTraceKind::Normal, nobody);
    };

    LevelTraceResult result = drop_point();
    EXPECT_NEAR(result.end_position[2], 40.0f + epsilon, tolerance);
    EXPECT_NEAR(result.plane_distance, 40.0f, tolerance);
    EXPECT_EQ(result.entity, lift);

    // from below it is its underside
    result = _collision->Trace({0.0f, 0.0f, 10.0f}, {}, {}, {0.0f, 0.0f, 100.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[2], 40.0f - LevelWorld::slab_thickness - epsilon, tolerance);

    // the box of a player stands on it with its feet
    result = _collision->Trace({0.0f, 0.0f, 120.0f}, LevelWorld::player_mins, LevelWorld::player_maxs,
                               {0.0f, 0.0f, 30.0f}, LevelTraceKind::Normal, nobody);
    EXPECT_NEAR(result.end_position[2], 64.0f + epsilon, tolerance);
    EXPECT_EQ(result.entity, lift);

    // it is where the entity is now
    _fields->origin.Set(*_machine, lift, {0.0f, 0.0f, 60.0f});
    EXPECT_NEAR(drop_point().end_position[2], 60.0f + epsilon, tolerance);
    _fields->origin.Set(*_machine, lift, {0.0f, 200.0f, 60.0f});
    EXPECT_FALSE(drop_point().HasEntity());
  }

  TEST_F(LevelCollisionTest, StopsWithTheBoxOfAnEntityWhoseModelIsNoPartOfTheLevel)
  {
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f});
    const auto drop_at = [this](const float x)
    {
      return _collision->Trace({x, 0.0f, 100.0f}, {}, {}, {x, 0.0f, 10.0f}, LevelTraceKind::Normal, nobody);
    };

    for (const std::string_view name : {"maps/b_explob.bsp", "*", "*2", "*0", "*1x", ""})
    {
      // a box of its own, narrower than the slab, to tell the two apart
      _fields->model.SetText(*_machine, lift, name);
      _fields->mins.Set(*_machine, lift, {-10.0f, -10.0f, -8.0f});
      _fields->maxs.Set(*_machine, lift, {10.0f, 10.0f, 0.0f});
      EXPECT_EQ(drop_at(0.0f).entity, lift) << name;
      EXPECT_FALSE(drop_at(20.0f).HasEntity()) << name;
    }
  }

  TEST_F(LevelCollisionTest, SaysWhetherAnEntityStandsInWhatIsSolid)
  {
    const std::int32_t walker =
      Make({0.0f, 0.0f, 24.0f + epsilon}, LevelWorld::player_mins, LevelWorld::player_maxs, QcSolid::SlideBox);
    EXPECT_FALSE(_collision->TestPosition(walker));

    // in the floor, in the wall
    _fields->origin.Set(*_machine, walker, {0.0f, 0.0f, 20.0f});
    EXPECT_TRUE(_collision->TestPosition(walker));
    _fields->origin.Set(*_machine, walker, {290.0f, 0.0f, 200.0f});
    EXPECT_TRUE(_collision->TestPosition(walker));

    // in the box of another, and in a lift
    _fields->origin.Set(*_machine, walker, {0.0f, 0.0f, 24.0f + epsilon});
    const std::int32_t crate = Make({20.0f, 0.0f, 0.0f}, crate_mins, crate_maxs);
    EXPECT_TRUE(_collision->TestPosition(walker));
    _fields->origin.Set(*_machine, crate, {60.0f, 0.0f, 0.0f});
    EXPECT_FALSE(_collision->TestPosition(walker));

    MakeLift({0.0f, 0.0f, 30.0f});
    EXPECT_TRUE(_collision->TestPosition(walker));
  }

  TEST_F(LevelCollisionTest, RemembersThatAMoveStartedInsideWhenSomethingNearerIsFound)
  {
    // the move starts inside one crate and is then stopped by another
    Make({-50.0f, 0.0f, 16.0f}, crate_mins, crate_maxs);
    const std::int32_t crate = MakeCrate();

    const LevelTraceResult result = TraceEast();
    EXPECT_TRUE(result.start_solid);
    EXPECT_FALSE(result.all_solid);
    EXPECT_EQ(result.entity, crate);
    EXPECT_NEAR(result.end_position[0], 134.0f - epsilon, tolerance);
  }

  TEST_F(LevelCollisionTest, WritesTheBoxAnEntityIsFoundBy)
  {
    const std::int32_t crate = MakeCrate();
    LevelBox box = _collision->GetBox(crate);
    EXPECT_THAT(box.mins, ElementsAre(133.0f, -17.0f, 15.0f));
    EXPECT_THAT(box.maxs, ElementsAre(167.0f, 17.0f, 73.0f));

    _collision->Link(crate);
    EXPECT_THAT(_fields->absmin.Get(*_machine, crate), ElementsAre(133.0f, -17.0f, 15.0f));
    EXPECT_THAT(_fields->absmax.Get(*_machine, crate), ElementsAre(167.0f, 17.0f, 73.0f));

    // an item is wider and no taller
    _fields->flags.Set(*_machine, crate, static_cast<float>(QcFlag::Item));
    box = _collision->GetBox(crate);
    EXPECT_THAT(box.mins, ElementsAre(119.0f, -31.0f, 16.0f));
    EXPECT_THAT(box.maxs, ElementsAre(181.0f, 31.0f, 72.0f));

    // nothing is written for the world and for a free entity
    _collision->Link(0);
    EXPECT_THAT(_fields->absmax.Get(*_machine, 0), ElementsAre(0.0f, 0.0f, 0.0f));
    _machine->FreeEntity(crate);
    _collision->Link(crate);
    EXPECT_THAT(_fields->absmin.Get(*_machine, crate), ElementsAre(133.0f, -17.0f, 15.0f));
  }
}
