#include "level-stepping.hpp"

#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "level-moving.test.hpp"

namespace
{
  using quake::LevelVector;
  using quake::LevelWorld;
  using quake::QcFlag;
  using quake::QcSolid;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;

  class LevelSteppingTest : public quake::LevelMovingTest
  {
  protected:
    /// The height a monster stands at on a floor of a height.
    static constexpr float StandingOn(const float height)
    {
      return height + 24.0f + epsilon;
    }
  };

  TEST_F(LevelSteppingTest, WalksAlongTheFloorAndNamesWhatItStandsOn)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    _fields->groundentity.Set(*_machine, monster, 7);

    EXPECT_TRUE(_stepping->WalkMove(monster, 0.0f, 30.0f));
    EXPECT_THAT(OriginOf(monster), ElementsAre(30.0f, 0.0f, FloatNear(StandingOn(0.0f), tolerance)));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, monster), 0);

    // a quarter turn to the left is along Y
    EXPECT_TRUE(_stepping->WalkMove(monster, 90.0f, 20.0f));
    EXPECT_THAT(OriginOf(monster), ElementsAre(FloatNear(30.0f, tolerance), 20.0f, ::testing::_));

    // the box it is found by went with it
    EXPECT_NEAR(_fields->absmin.Get(*_machine, monster)[1], 20.0f - 17.0f, tolerance);
  }

  TEST_F(LevelSteppingTest, WalksUpAStepAndDownIt)
  {
    const std::int32_t monster = MakeMonster(80.0f, 0.0f);

    EXPECT_TRUE(_stepping->WalkMove(monster, 0.0f, 30.0f));
    EXPECT_THAT(OriginOf(monster),
                ElementsAre(110.0f, 0.0f, FloatNear(StandingOn(LevelWorld::step_height), tolerance)));
    EXPECT_TRUE(Has(monster, QcFlag::OnGround));

    EXPECT_TRUE(_stepping->WalkMove(monster, 180.0f, 40.0f));
    EXPECT_THAT(OriginOf(monster), ElementsAre(FloatNear(70.0f, tolerance), ::testing::_,
                                               FloatNear(StandingOn(0.0f), tolerance)));
  }

  TEST_F(LevelSteppingTest, RefusesWhatIsTooHighToStepOn)
  {
    const std::int32_t monster = MakeMonster(170.0f, 0.0f, LevelWorld::step_height);
    const LevelVector before = OriginOf(monster);

    EXPECT_FALSE(_stepping->WalkMove(monster, 0.0f, 30.0f));
    EXPECT_EQ(OriginOf(monster), before);
  }

  TEST_F(LevelSteppingTest, RefusesAStepOffAnEdge)
  {
    const std::int32_t monster = MakeMonster(-80.0f, 0.0f);
    const LevelVector before = OriginOf(monster);

    // all of it over the pit
    EXPECT_FALSE(_stepping->WalkMove(monster, 180.0f, 40.0f));
    EXPECT_EQ(OriginOf(monster), before);

    // and with only two corners over it: the box still has floor under
    // it, and its corners do not
    EXPECT_FALSE(_stepping->WalkMove(monster, 180.0f, 10.0f));
    EXPECT_EQ(OriginOf(monster), before);
    EXPECT_TRUE(Has(monster, QcFlag::OnGround));

    // up to the edge it goes
    EXPECT_TRUE(_stepping->WalkMove(monster, 180.0f, 3.0f));
  }

  TEST_F(LevelSteppingTest, SaysWhetherAnEntityHasAFloorUnderAllOfIt)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    EXPECT_TRUE(_stepping->CheckBottom(monster));

    // two corners over the pit
    _fields->origin.Set(*_machine, monster, {-90.0f, 0.0f, StandingOn(0.0f)});
    EXPECT_FALSE(_stepping->CheckBottom(monster));

    // two corners a step lower than the others is a floor still
    _fields->origin.Set(*_machine, monster, {105.0f, 0.0f, StandingOn(LevelWorld::step_height)});
    EXPECT_TRUE(_stepping->CheckBottom(monster));

    // in the air there is none
    _fields->origin.Set(*_machine, monster, {0.0f, 0.0f, 200.0f});
    EXPECT_FALSE(_stepping->CheckBottom(monster));
  }

  TEST_F(LevelSteppingTest, DoesNotLetWhatIsInTheAirWalk)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));

    EXPECT_FALSE(_stepping->WalkMove(monster, 0.0f, 30.0f));
    EXPECT_EQ(OriginOf(monster)[0], 0.0f);
  }

  TEST_F(LevelSteppingTest, IsStoppedByTheBoxOfAnother)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    Make({50.0f, 0.0f, 0.0f});

    // the two boxes would meet at 18
    EXPECT_FALSE(_stepping->WalkMove(monster, 0.0f, 30.0f));
    EXPECT_EQ(OriginOf(monster)[0], 0.0f);
    EXPECT_TRUE(_stepping->WalkMove(monster, 0.0f, 10.0f));
  }

  TEST_F(LevelSteppingTest, LetsAMonsterWithoutAWholeFloorWalkOffWhatIsLeft)
  {
    const std::int32_t monster = MakeMonster(-80.0f, 0.0f);
    Set(monster, QcFlag::PartialGround);

    // corners over the edge: the step is taken all the same
    EXPECT_TRUE(_stepping->WalkMove(monster, 180.0f, 10.0f));
    EXPECT_NEAR(OriginOf(monster)[0], -90.0f, tolerance);
    EXPECT_TRUE(Has(monster, QcFlag::OnGround));

    // all of it over the pit: it goes and no longer stands on anything
    EXPECT_TRUE(_stepping->WalkMove(monster, 180.0f, 40.0f));
    EXPECT_NEAR(OriginOf(monster)[0], -130.0f, tolerance);
    EXPECT_FALSE(Has(monster, QcFlag::OnGround));

    // back on a whole floor it is as any monster again
    const std::int32_t other = MakeMonster(0.0f, 100.0f);
    Set(other, QcFlag::PartialGround);
    EXPECT_TRUE(_stepping->WalkMove(other, 0.0f, 10.0f));
    EXPECT_FALSE(Has(other, QcFlag::PartialGround));
  }

  TEST_F(LevelSteppingTest, MovesWhatFliesThroughTheAirAndToTheHeightOfItsEnemy)
  {
    const std::int32_t flyer = MakeMonster(-80.0f, 0.0f);
    _fields->flags.Set(*_machine, flyer, static_cast<float>(QcFlag::Fly));
    _fields->origin.Set(*_machine, flyer, {-80.0f, 0.0f, 100.0f});

    // out over the pit
    EXPECT_TRUE(_stepping->WalkMove(flyer, 180.0f, 40.0f));
    EXPECT_THAT(OriginOf(flyer), ElementsAre(FloatNear(-120.0f, tolerance), ::testing::_, 100.0f));

    // an enemy below draws it down, 8 units a step
    const std::int32_t enemy = MakeMonster(0.0f, 0.0f);
    _fields->enemy.Set(*_machine, flyer, enemy);
    EXPECT_TRUE(_stepping->WalkMove(flyer, 0.0f, 10.0f));
    EXPECT_THAT(OriginOf(flyer), ElementsAre(FloatNear(-110.0f, tolerance), ::testing::_, 92.0f));

    // one above draws it up
    _fields->origin.Set(*_machine, enemy, {0.0f, 0.0f, 300.0f});
    EXPECT_TRUE(_stepping->WalkMove(flyer, 0.0f, 10.0f));
    EXPECT_EQ(OriginOf(flyer)[2], 100.0f);

    // into the wall it does not go
    _fields->origin.Set(*_machine, flyer, {280.0f, 0.0f, 200.0f});
    EXPECT_FALSE(_stepping->WalkMove(flyer, 0.0f, 30.0f));
  }

  TEST_F(LevelSteppingTest, KeepsWhatSwimsOutOfTheAir)
  {
    // the level has no water, so every step would leave it
    const std::int32_t fish = MakeMonster(0.0f, 0.0f);
    _fields->flags.Set(*_machine, fish, static_cast<float>(QcFlag::Swim));
    _fields->origin.Set(*_machine, fish, {0.0f, 0.0f, 100.0f});

    EXPECT_FALSE(_stepping->WalkMove(fish, 0.0f, 10.0f));
    EXPECT_EQ(OriginOf(fish)[0], 0.0f);
  }

  TEST_F(LevelSteppingTest, TouchesTheTriggersAStepComesInto)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    const std::int32_t trigger = Make({60.0f, 0.0f, 0.0f}, crate_mins, crate_maxs, QcSolid::Trigger);
    _fields->touch.Set(*_machine, trigger, FunctionOf("touch"));
    _globals->self.Set(*_machine, monster);
    _globals->other.Set(*_machine, 9);

    EXPECT_TRUE(_stepping->WalkMove(monster, 0.0f, 10.0f));
    EXPECT_THAT(_touches, ElementsAre());

    // it walks into it and is not stopped by it
    EXPECT_TRUE(_stepping->WalkMove(monster, 0.0f, 20.0f));
    EXPECT_THAT(_touches, ElementsAre(Call{trigger, monster, 1.0f}));
    EXPECT_NEAR(OriginOf(monster)[0], 30.0f, tolerance);

    // the game code that asked for the step goes on as who it was
    EXPECT_EQ(_globals->self.Get(*_machine), monster);
    EXPECT_EQ(_globals->other.Get(*_machine), 9);
  }

  TEST_F(LevelSteppingTest, TurnsTowardsWhereItWantsToFaceTheShorterWayRound)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    const auto turn = [&](const float from, const float ideal, const float speed)
    {
      _fields->angles.Set(*_machine, monster, {0.0f, from, 0.0f});
      _fields->ideal_yaw.Set(*_machine, monster, ideal);
      _fields->yaw_speed.Set(*_machine, monster, speed);
      _stepping->ChangeYaw(monster);
      return _fields->angles.Get(*_machine, monster)[1];
    };

    EXPECT_NEAR(turn(0.0f, 90.0f, 20.0f), 20.0f, 0.01f);
    EXPECT_NEAR(turn(0.0f, 10.0f, 20.0f), 10.0f, 0.01f);
    EXPECT_NEAR(turn(0.0f, 300.0f, 20.0f), 340.0f, 0.01f);
    EXPECT_NEAR(turn(0.0f, 350.0f, 20.0f), 350.0f, 0.01f);
    EXPECT_NEAR(turn(350.0f, 10.0f, 5.0f), 355.0f, 0.01f);
    EXPECT_NEAR(turn(350.0f, 10.0f, 30.0f), 10.0f, 0.01f);
    EXPECT_NEAR(turn(90.0f, 90.0f, 20.0f), 90.0f, 0.01f);
    // an angle outside a turn is brought into it
    EXPECT_NEAR(turn(-90.0f, 180.0f, 20.0f), 250.0f, 0.01f);
  }

  TEST_F(LevelSteppingTest, TurnsBeforeItWalks)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    _fields->yaw_speed.Set(*_machine, monster, 20.0f);

    // to its right: the step is good, and not taken while its yaw is more
    // than 45 above where it wants to face
    EXPECT_TRUE(_stepping->StepDirection(monster, 270.0f, 10.0f));
    EXPECT_EQ(OriginOf(monster)[1], 0.0f);
    EXPECT_NEAR(_fields->angles.Get(*_machine, monster)[1], 340.0f, 0.01f);
    EXPECT_EQ(_fields->ideal_yaw.Get(*_machine, monster), 270.0f);

    EXPECT_TRUE(_stepping->StepDirection(monster, 270.0f, 10.0f));
    EXPECT_EQ(OriginOf(monster)[1], 0.0f);
    EXPECT_TRUE(_stepping->StepDirection(monster, 270.0f, 10.0f));
    EXPECT_NEAR(OriginOf(monster)[1], -10.0f, tolerance);

    // To its left the original takes the step at once: it asks for the
    // yaw less the direction, which is below zero then.
    _fields->angles.Set(*_machine, monster, {0.0f, 0.0f, 0.0f});
    EXPECT_TRUE(_stepping->StepDirection(monster, 90.0f, 10.0f));
    EXPECT_NEAR(OriginOf(monster)[1], 0.0f, tolerance);
    EXPECT_NEAR(_fields->angles.Get(*_machine, monster)[1], 20.0f, 0.01f);
  }

  TEST_F(LevelSteppingTest, WalksTowardsItsGoalTheWayItFaces)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    const std::int32_t goal = Make({0.0f, 200.0f, 0.0f}, {}, {}, QcSolid::Not);
    _fields->goalentity.Set(*_machine, monster, goal);
    _fields->ideal_yaw.Set(*_machine, monster, 90.0f);

    _stepping->MoveToGoal(monster, 10.0f);
    EXPECT_NEAR(OriginOf(monster)[1], 10.0f, tolerance);
    EXPECT_NEAR(_fields->angles.Get(*_machine, monster)[1], 90.0f, 0.01f);

    // in the air it does not
    _fields->flags.Set(*_machine, monster, static_cast<float>(QcFlag::Monster));
    _stepping->MoveToGoal(monster, 10.0f);
    EXPECT_NEAR(OriginOf(monster)[1], 10.0f, tolerance);
  }

  TEST_F(LevelSteppingTest, FindsAnotherWayWhenTheWayItFacesIsBlocked)
  {
    // it faces the side of the ledge, and its goal is to the north
    const std::int32_t monster = MakeMonster(180.0f, 0.0f, LevelWorld::step_height);
    const std::int32_t goal = Make({180.0f, 200.0f, 16.0f}, {}, {}, QcSolid::Not);
    _fields->goalentity.Set(*_machine, monster, goal);
    _fields->ideal_yaw.Set(*_machine, monster, 0.0f);

    _stepping->MoveToGoal(monster, 10.0f);
    EXPECT_THAT(OriginOf(monster), ElementsAre(FloatNear(180.0f, tolerance), FloatNear(10.0f, tolerance), ::testing::_));
    EXPECT_EQ(_fields->ideal_yaw.Get(*_machine, monster), 90.0f);

    // a goal to the south-west is walked at straight, between the axes
    _fields->origin.Set(*_machine, goal, {0.0f, -200.0f, 0.0f});
    _fields->origin.Set(*_machine, monster, {150.0f, 0.0f, OriginOf(monster)[2]});
    _stepping->ChooseDirection(monster, goal, 10.0f);
    EXPECT_EQ(_fields->ideal_yaw.Get(*_machine, monster), 215.0f);
    EXPECT_LT(OriginOf(monster)[0], 150.0f);
    EXPECT_LT(OriginOf(monster)[1], 0.0f);
  }

  TEST_F(LevelSteppingTest, LooksForAnotherWayNowAndThenByChance)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    const std::int32_t goal = Make({0.0f, 200.0f, 0.0f}, {}, {}, QcSolid::Not);
    _fields->goalentity.Set(*_machine, monster, goal);
    _fields->ideal_yaw.Set(*_machine, monster, 0.0f);

    // by chance it does not go on east, and turns to its goal
    _chance = 0.1f;
    _stepping->MoveToGoal(monster, 10.0f);
    EXPECT_EQ(_fields->ideal_yaw.Get(*_machine, monster), 90.0f);
    EXPECT_NEAR(OriginOf(monster)[1], 10.0f, tolerance);
    EXPECT_NEAR(OriginOf(monster)[0], 0.0f, tolerance);
  }

  TEST_F(LevelSteppingTest, StaysWhenItHasAnEnemyAndIsCloseEnoughToItsGoal)
  {
    const std::int32_t monster = MakeMonster(0.0f, 0.0f);
    const std::int32_t goal = MakeMonster(60.0f, 0.0f);
    _fields->goalentity.Set(*_machine, monster, goal);
    _fields->enemy.Set(*_machine, monster, goal);

    // the boxes are 26 apart
    EXPECT_FALSE(_stepping->IsCloseEnough(monster, goal, 20.0f));
    EXPECT_TRUE(_stepping->IsCloseEnough(monster, goal, 30.0f));

    _stepping->MoveToGoal(monster, 30.0f);
    EXPECT_EQ(OriginOf(monster)[0], 0.0f);

    // without an enemy it walks on, to something it only follows
    _fields->enemy.Set(*_machine, monster, 0);
    _stepping->MoveToGoal(monster, 5.0f);
    EXPECT_NEAR(OriginOf(monster)[0], 5.0f, tolerance);
  }

  TEST_F(LevelSteppingTest, LetsAMonsterThatCannotMoveAndHasNoWholeFloorWalkOffIt)
  {
    // boxed in on a place with two corners over the pit
    const std::int32_t monster = MakeMonster(-90.0f, 0.0f);
    const std::int32_t goal = Make({200.0f, 0.0f, 0.0f}, {}, {}, QcSolid::Not);
    for (const LevelVector &place : {
           LevelVector{-58.0f, 0.0f, 0.0f}, LevelVector{-90.0f, 32.0f, 0.0f}, LevelVector{-90.0f, -32.0f, 0.0f},
           LevelVector{-58.0f, 32.0f, 0.0f}, LevelVector{-58.0f, -32.0f, 0.0f},
         })
    {
      Make(place);
    }
    _fields->ideal_yaw.Set(*_machine, monster, 50.0f);

    _stepping->ChooseDirection(monster, goal, 10.0f);
    EXPECT_NEAR(OriginOf(monster)[0], -90.0f, tolerance);
    EXPECT_TRUE(Has(monster, QcFlag::PartialGround));
    // the way it went before, to the eighth below it
    EXPECT_EQ(_fields->ideal_yaw.Get(*_machine, monster), 45.0f);
  }
}
