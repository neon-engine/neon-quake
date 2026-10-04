#include "level-physics.hpp"

#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-contents.hpp"
#include "level-moving.test.hpp"

namespace
{
  using quake::BspContents;
  using quake::LevelVector;
  using quake::LevelWorld;
  using quake::QcFlag;
  using quake::QcMachine;
  using quake::QcMoveType;
  using quake::QcSolid;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;
  using ::testing::IsEmpty;

  class LevelPhysicsTest : public quake::LevelMovingTest
  {
  protected:
    /// A step that floats hold exactly.
    static constexpr float step = 0.25f;

    /// A crate that rides: it moves by steps, and stands on something.
    std::int32_t MakeRider(const LevelVector &origin, const std::int32_t ground)
    {
      const std::int32_t rider = MakeMover(origin, QcMoveType::Step);
      Set(rider, QcFlag::OnGround);
      _fields->groundentity.Set(*_machine, rider, ground);
      return rider;
    }
  };

  TEST_F(LevelPhysicsTest, DropsWhatIsTossedOntoTheFloorWhereItStays)
  {
    const std::int32_t item = MakeMover({0.0f, 0.0f, 100.0f}, QcMoveType::Toss);
    _fields->avelocity.Set(*_machine, item, {0.0f, 90.0f, 0.0f});

    // a frame of falling: faster by the gravity, lower by the new speed
    _running->Advance(0.05f);
    EXPECT_THAT(_fields->velocity.Get(*_machine, item), ElementsAre(0.0f, 0.0f, -40.0f));
    EXPECT_THAT(OriginOf(item), ElementsAre(0.0f, 0.0f, 98.0f));
    EXPECT_NEAR(_fields->angles.Get(*_machine, item)[1], 4.5f, tolerance);
    EXPECT_FALSE(Has(item, QcFlag::OnGround));
    EXPECT_THAT(_touches, IsEmpty());

    Advance(20);
    EXPECT_THAT(OriginOf(item), ElementsAre(0.0f, 0.0f, FloatNear(epsilon, tolerance)));
    EXPECT_TRUE(Has(item, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, item), 0);
    EXPECT_THAT(_fields->velocity.Get(*_machine, item), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_THAT(_fields->avelocity.Get(*_machine, item), ElementsAre(0.0f, 0.0f, 0.0f));

    // it touched the world once, when it landed, at the time of that frame
    ASSERT_EQ(_touches.size(), 1u);
    EXPECT_EQ(_touches[0].self, item);
    EXPECT_EQ(_touches[0].other, 0);
    EXPECT_GT(_touches[0].time, 1.0f);

    // and it was noted to be in the open
    EXPECT_EQ(_fields->watertype.Get(*_machine, item), static_cast<float>(BspContents::Empty));
  }

  TEST_F(LevelPhysicsTest, DropsWhatIsTossedOntoTheBoxOfAnotherAndTellsBoth)
  {
    const std::int32_t crate = Make({0.0f, 0.0f, 0.0f});
    _fields->touch.Set(*_machine, crate, FunctionOf("touch"));
    const std::int32_t item = MakeMover({10.0f, 0.0f, 100.0f}, QcMoveType::Toss, {-8.0f, -8.0f, 0.0f},
                                        {8.0f, 8.0f, 16.0f});

    Advance(20);
    EXPECT_NEAR(OriginOf(item)[2], 56.0f + epsilon, tolerance);
    EXPECT_TRUE(Has(item, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, item), crate);
    ASSERT_EQ(_touches.size(), 2u);
    EXPECT_EQ(_touches[0].self, item);
    EXPECT_EQ(_touches[0].other, crate);
    EXPECT_EQ(_touches[1].self, crate);
    EXPECT_EQ(_touches[1].other, item);
  }

  TEST_F(LevelPhysicsTest, ThrowsBackWhatBouncesUntilItIsSlow)
  {
    const std::int32_t ball = MakeMover({0.0f, 0.0f, 100.0f}, QcMoveType::Bounce);

    // down to the floor, and up again with half the speed it came with
    for (int frame = 0; frame < 40 && _touches.empty(); frame++) { _running->Advance(0.05f); }
    ASSERT_EQ(_touches.size(), 1u);
    EXPECT_GT(_fields->velocity.Get(*_machine, ball)[2], 100.0f);
    EXPECT_FALSE(Has(ball, QcFlag::OnGround));

    Advance(200);
    EXPECT_TRUE(Has(ball, QcFlag::OnGround));
    EXPECT_NEAR(OriginOf(ball)[2], epsilon, tolerance);
    EXPECT_GE(_touches.size(), 2u);
  }

  TEST_F(LevelPhysicsTest, LetsWhatFliesGoStraightUntilItHits)
  {
    const std::int32_t missile = MakeMover({0.0f, 0.0f, 40.0f}, QcMoveType::Fly, {}, {});
    _fields->velocity.Set(*_machine, missile, {400.0f, 0.0f, 0.0f});

    _running->Advance(step);
    EXPECT_THAT(OriginOf(missile), ElementsAre(100.0f, 0.0f, 40.0f));
    EXPECT_THAT(_fields->velocity.Get(*_machine, missile), ElementsAre(400.0f, 0.0f, 0.0f));

    // into the side of the ledge
    _running->Advance(step);
    _running->Advance(step);
    EXPECT_NEAR(OriginOf(missile)[0], LevelWorld::ledge_edge - epsilon, tolerance);
    EXPECT_THAT(_touches, ElementsAre(Call{missile, 0, 1.25f}));
    EXPECT_FALSE(Has(missile, QcFlag::OnGround));
    EXPECT_EQ(_fields->velocity.Get(*_machine, missile)[0], 0.0f);
  }

  TEST_F(LevelPhysicsTest, HitsAMonsterBesideItsWayWithAMissile)
  {
    // the box of the monster is 14 units off the line
    const std::int32_t monster = MakeMonster(150.0f, 30.0f, LevelWorld::step_height);
    const std::int32_t shooter = MakeMonster(-50.0f, 0.0f);
    const auto shoot = [&](const QcMoveType move_type)
    {
      const std::int32_t missile = MakeMover({0.0f, 0.0f, 50.0f}, move_type, {}, {});
      _fields->velocity.Set(*_machine, missile, {400.0f, 0.0f, 0.0f});
      _fields->owner.Set(*_machine, missile, shooter);
      Advance(3, step);
      return missile;
    };

    // what only flies goes past, to the ledge
    const std::int32_t arrow = shoot(QcMoveType::Fly);
    EXPECT_NEAR(OriginOf(arrow)[0], LevelWorld::ledge_edge - epsilon, tolerance);

    const std::int32_t missile = shoot(QcMoveType::FlyMissile);
    EXPECT_NEAR(OriginOf(missile)[0], 150.0f - 16.0f - 15.0f - epsilon, tolerance);
    EXPECT_THAT(_touches, Contains(Call{missile, monster, 2.0f}));
  }

  TEST_F(LevelPhysicsTest, MovesWhatClipsNothingThroughTheWall)
  {
    const std::int32_t ghost = MakeMover({250.0f, 0.0f, 100.0f}, QcMoveType::NoClip);
    _fields->velocity.Set(*_machine, ghost, {400.0f, 0.0f, -40.0f});
    _fields->avelocity.Set(*_machine, ghost, {0.0f, 40.0f, 0.0f});

    _running->Advance(step);
    EXPECT_THAT(OriginOf(ghost), ElementsAre(350.0f, 0.0f, 90.0f));
    EXPECT_THAT(_fields->angles.Get(*_machine, ghost), ElementsAre(0.0f, 10.0f, 0.0f));
    EXPECT_THAT(_fields->absmin.Get(*_machine, ghost), ElementsAre(333.0f, -17.0f, 89.0f));
    EXPECT_THAT(_touches, IsEmpty());
  }

  TEST_F(LevelPhysicsTest, LeavesAloneWhatDoesNotMoveAndThePlayer)
  {
    const std::int32_t still = MakeMover({0.0f, 0.0f, 100.0f}, QcMoveType::None);
    const std::int32_t player = MakeMover({50.0f, 0.0f, 100.0f}, QcMoveType::Walk);
    for (const std::int32_t entity : {still, player}) { _fields->velocity.Set(*_machine, entity, {10.0f, 0.0f, 0.0f}); }

    Advance(4);
    EXPECT_THAT(OriginOf(still), ElementsAre(0.0f, 0.0f, 100.0f));
    EXPECT_THAT(OriginOf(player), ElementsAre(50.0f, 0.0f, 100.0f));
    EXPECT_THAT(_fields->velocity.Get(*_machine, player), ElementsAre(10.0f, 0.0f, 0.0f));
  }

  TEST_F(LevelPhysicsTest, DropsAMonsterThatStandsOnNothing)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    _fields->origin.Set(*_machine, monster, {0.0f, 0.0f, 100.0f});

    // it stands, by its flag, and so does not fall
    Advance(4);
    EXPECT_EQ(OriginOf(monster)[2], 100.0f);

    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));
    Advance(20);
    EXPECT_NEAR(OriginOf(monster)[2], 24.0f + epsilon, tolerance);
    EXPECT_TRUE(Has(monster, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, monster), 0);

    // one that flies stays up
    const std::int32_t flyer = MakeMonster(100.0f, 100.0f);
    _fields->origin.Set(*_machine, flyer, {100.0f, 100.0f, 150.0f});
    _fields->flags.Set(*_machine, flyer, static_cast<float>(QcFlag::Fly));
    Advance(4);
    EXPECT_EQ(OriginOf(flyer)[2], 150.0f);
  }

  TEST_F(LevelPhysicsTest, SlidesAFallingMonsterAlongAWall)
  {
    // it falls and drifts east into the side of the ledge
    const std::int32_t monster = MakeMonster(170.0f, 0.0f);
    _fields->origin.Set(*_machine, monster, {170.0f, 0.0f, 62.0f});
    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));
    _fields->velocity.Set(*_machine, monster, {200.0f, 0.0f, 0.0f});

    Advance(10);
    EXPECT_NEAR(OriginOf(monster)[0], 184.0f - epsilon, tolerance);
    EXPECT_NEAR(OriginOf(monster)[2], LevelWorld::step_height + 24.0f + epsilon, tolerance);
    EXPECT_TRUE(Has(monster, QcFlag::OnGround));
  }

  TEST_F(LevelPhysicsTest, FallsByTheGravityOfTheLevelAndOfTheEntity)
  {
    EXPECT_EQ(_physics->GetGravity(), 800.0f);
    const std::int32_t item = MakeMover({0.0f, 0.0f, 200.0f}, QcMoveType::Toss);
    const quake::QcField<quake::ProgsType::Float> gravity(_machine->GetProgs(), "gravity");

    _physics->SetGravity(400.0f);
    _running->Advance(step);
    EXPECT_EQ(_fields->velocity.Get(*_machine, item)[2], -100.0f);

    // half of it for this one
    gravity.Set(*_machine, item, 0.5f);
    _running->Advance(step);
    EXPECT_EQ(_fields->velocity.Get(*_machine, item)[2], -150.0f);

    // and nothing moves faster than the fastest
    _physics->SetMaxVelocity(120.0f);
    EXPECT_EQ(_physics->GetMaxVelocity(), 120.0f);
    _running->Advance(step);
    EXPECT_EQ(_fields->velocity.Get(*_machine, item)[2], -170.0f);
    _running->Advance(step);
    EXPECT_EQ(_fields->velocity.Get(*_machine, item)[2], -170.0f);
  }

  TEST_F(LevelPhysicsTest, TouchesTheTriggersWhatFallsComesInto)
  {
    const std::int32_t trigger = Make({0.0f, 0.0f, 20.0f}, crate_mins, {16.0f, 16.0f, 20.0f}, QcSolid::Trigger);
    _fields->touch.Set(*_machine, trigger, FunctionOf("touch"));
    const std::int32_t item = MakeMover({0.0f, 0.0f, 100.0f}, QcMoveType::Toss);
    _fields->touch.Set(*_machine, item, 0);

    _running->Advance(0.05f);
    EXPECT_THAT(_touches, IsEmpty());

    // it falls through the trigger and is touched by it in every frame it
    // is in it, and then lies in it on the floor
    Advance(20);
    ASSERT_FALSE(_touches.empty());
    for (const Call &call : _touches)
    {
      EXPECT_EQ(call.self, trigger);
      EXPECT_EQ(call.other, item);
    }
    EXPECT_NEAR(OriginOf(item)[2], epsilon, tolerance);
  }

  TEST_F(LevelPhysicsTest, StopsMovingWhatATouchRemoved)
  {
    const std::int32_t item = MakeMover({0.0f, 0.0f, 10.0f}, QcMoveType::Toss);
    _machine->SetBuiltin(_touch, [this](QcMachine &machine) { machine.FreeEntity(_globals->self.Get(machine)); });

    Advance(10);
    EXPECT_TRUE(_machine->IsEntityFree(item));
    EXPECT_FALSE(Has(item, QcFlag::OnGround));
    EXPECT_THAT(_running->GetFailures(), IsEmpty());
  }

  TEST_F(LevelPhysicsTest, HasALiftCarryWhatStandsOnIt)
  {
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 40.0f});
    const std::int32_t rider = MakeRider({0.0f, 0.0f, 40.0f + epsilon}, lift);
    const std::int32_t bystander = MakeRider({0.0f, 100.0f, epsilon}, 0);

    _running->Advance(step);
    EXPECT_THAT(OriginOf(lift), ElementsAre(0.0f, 0.0f, 50.0f));
    EXPECT_EQ(_fields->ltime.Get(*_machine, lift), step);
    EXPECT_NEAR(OriginOf(rider)[2], 50.0f + epsilon, tolerance);
    EXPECT_NEAR(OriginOf(bystander)[2], epsilon, tolerance);
    EXPECT_THAT(_blocks, IsEmpty());

    // The rider finds its ground anew, and has it again in the frame the
    // lift has moved from under it and it has come down on it.
    EXPECT_TRUE(Has(bystander, QcFlag::OnGround));
    Advance(8, step);
    EXPECT_NEAR(OriginOf(lift)[2], 130.0f, tolerance);
    EXPECT_NEAR(OriginOf(rider)[2], 130.0f, 0.5f);
    EXPECT_EQ(_fields->groundentity.Get(*_machine, rider), lift);
    EXPECT_THAT(_running->GetFailures(), IsEmpty());
  }

  TEST_F(LevelPhysicsTest, HasWhatPushesMoveWhatIsInItsWay)
  {
    // the slab moves east along the step, into a crate that has room
    const std::int32_t slab = MakeLift({100.0f, 0.0f, 50.0f}, {80.0f, 0.0f, 0.0f});
    const std::int32_t crate = MakeRider({150.0f, 0.0f, LevelWorld::step_height + epsilon}, 0);

    _running->Advance(step);
    EXPECT_THAT(OriginOf(slab), ElementsAre(120.0f, 0.0f, 50.0f));
    EXPECT_NEAR(OriginOf(crate)[0], 170.0f, tolerance);
    EXPECT_THAT(_blocks, IsEmpty());
    EXPECT_FALSE(_collision->TestPosition(crate));
  }

  TEST_F(LevelPhysicsTest, StopsWhatPushesForWhatHasNowhereToGoAndPutsEverythingBack)
  {
    // a rider on top, and a crate between the slab and the ledge
    const std::int32_t slab = MakeLift({100.0f, 0.0f, 50.0f}, {160.0f, 0.0f, 0.0f});
    const std::int32_t rider = MakeRider({100.0f, 0.0f, 50.0f + epsilon}, slab);
    const std::int32_t crate = MakeRider({175.0f, 0.0f, LevelWorld::step_height + epsilon}, 0);
    _globals->self.Set(*_machine, 5);
    _globals->other.Set(*_machine, 6);

    _running->Advance(step);
    EXPECT_THAT(OriginOf(slab), ElementsAre(100.0f, 0.0f, 50.0f));
    EXPECT_EQ(_fields->ltime.Get(*_machine, slab), 0.0f);
    EXPECT_NEAR(OriginOf(crate)[0], 175.0f, tolerance);
    EXPECT_NEAR(OriginOf(rider)[0], 100.0f, tolerance);

    // the slab was told who is in its way, at the time of the level
    EXPECT_THAT(_blocks, ElementsAre(Call{slab, crate, 1.0f}));

    // with the crate gone it moves
    _machine->FreeEntity(crate);
    _running->Advance(step);
    EXPECT_THAT(OriginOf(slab), ElementsAre(140.0f, 0.0f, 50.0f));
    EXPECT_NEAR(OriginOf(rider)[0], 140.0f, tolerance);
    EXPECT_EQ(_fields->ltime.Get(*_machine, slab), step);
    EXPECT_EQ(_blocks.size(), 1u);
  }

  TEST_F(LevelPhysicsTest, SquashesWhatStopsNothingInsteadOfStoppingForIt)
  {
    const std::int32_t slab = MakeLift({100.0f, 0.0f, 50.0f}, {160.0f, 0.0f, 0.0f});
    const std::int32_t corpse = MakeRider({175.0f, 0.0f, LevelWorld::step_height + epsilon}, 0);
    _fields->solid.Set(*_machine, corpse, static_cast<float>(QcSolid::Not));

    _running->Advance(step);
    EXPECT_THAT(OriginOf(slab), ElementsAre(140.0f, 0.0f, 50.0f));
    EXPECT_THAT(_blocks, IsEmpty());
    EXPECT_THAT(_fields->mins.Get(*_machine, corpse), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_THAT(_fields->maxs.Get(*_machine, corpse), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(LevelPhysicsTest, LeavesThePlayersToTheHostUnlessToldOtherwise)
  {
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 40.0f});
    const std::int32_t player = MakeRider({0.0f, 0.0f, 40.0f + epsilon}, lift);
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::Walk));
    Set(player, QcFlag::Client);
    EXPECT_FALSE(_physics->GetMovesClients());

    _running->Advance(step);
    EXPECT_EQ(OriginOf(lift)[2], 50.0f);
    EXPECT_NEAR(OriginOf(player)[2], 40.0f + epsilon, tolerance);
    EXPECT_THAT(_blocks, IsEmpty());

    // moved as anything else, and still standing: a player keeps the flag
    _fields->origin.Set(*_machine, player, {0.0f, 0.0f, 50.0f + epsilon});
    _physics->SetMovesClients(true);
    _running->Advance(step);
    EXPECT_EQ(OriginOf(lift)[2], 60.0f);
    EXPECT_NEAR(OriginOf(player)[2], 60.0f + epsilon, tolerance);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
  }

  TEST_F(LevelPhysicsTest, LetsWhatPushesOnlyTurnWithoutAsking)
  {
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f}, {});
    _fields->avelocity.Set(*_machine, lift, {0.0f, 40.0f, 0.0f});
    MakeRider({0.0f, 0.0f, 20.0f}, 0);

    _running->Advance(step);
    EXPECT_THAT(_fields->angles.Get(*_machine, lift), ElementsAre(0.0f, 10.0f, 0.0f));
    EXPECT_THAT(_blocks, IsEmpty());
  }
}
