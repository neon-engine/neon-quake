#include "player-movement.hpp"

#include <cmath>
#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "player-moving.test.hpp"
#include "qc-console-variables.hpp"

namespace
{
  using quake::Length;
  using quake::LevelVector;
  using quake::PlayerCommand;
  using quake::PlayerMovementSettings;
  using quake::PlayerWorld;
  using quake::QcConsoleVariables;
  using quake::QcFlag;
  using quake::QcMoveType;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;

  /// A player who is steered and not moved: each test looks at what one
  /// call does to the velocity.
  class PlayerMovementTest : public quake::PlayerMovingTest
  {
  protected:
    /// A player in the middle of the floor who stands on it.
    std::int32_t MakeStander(const LevelVector &velocity = {})
    {
      const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});
      Set(player, QcFlag::OnGround);
      _fields->velocity.Set(*_machine, player, velocity);
      return player;
    }

    /// A player in the air over the floor.
    std::int32_t MakeJumper(const LevelVector &velocity = {})
    {
      const std::int32_t player = MakePlayer({0.0f, 0.0f, 60.0f});
      _fields->velocity.Set(*_machine, player, velocity);
      return player;
    }

    /// A player deep in the pool, who the mover noted to be under water.
    std::int32_t MakeSwimmer()
    {
      const std::int32_t player = MakePlayer({-200.0f, 0.0f, -100.0f});
      _fields->waterlevel.Set(*_machine, player, 3.0f);
      return player;
    }

    [[nodiscard]] float SpeedOf(const std::int32_t entity) const
    {
      return Length(VelocityOf(entity));
    }
  };

  TEST_F(PlayerMovementTest, SlowsAPlayerOnTheGroundDownToAStopInTheTimeTheFrictionGives)
  {
    const std::int32_t player = MakeStander({0.0f, 320.0f, 0.0f});

    // a frame: slower by four times the speed a second
    Steer(player, {});
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, FloatNear(320.0f * (1.0f - 4.0f * frame_time), tolerance), 0.0f));

    // From 320 down to 100 the speed falls off as a curve, which takes a
    // quarter of the logarithm of 3.2 seconds, and from 100 by 400 a
    // second, a quarter of a second: 0.54 seconds, 39 frames.
    int frames = 1;
    while (SpeedOf(player) > 0.0f && frames < 100)
    {
      Steer(player, {});
      frames++;
    }
    EXPECT_GE(frames, 37);
    EXPECT_LE(frames, 41);
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(PlayerMovementTest, SlowsAPlayerDownTwiceAsMuchWhoseWayLeadsOverAnEdge)
  {
    // ten units from the pool and going towards it
    const std::int32_t player = MakePlayer({PlayerWorld::pool_edge + 10.0f, 0.0f, standing});
    Set(player, QcFlag::OnGround);
    _fields->velocity.Set(*_machine, player, {-320.0f, 0.0f, 0.0f});

    Steer(player, {});
    EXPECT_NEAR(VelocityOf(player)[0], -320.0f * (1.0f - 8.0f * frame_time), tolerance);
  }

  TEST_F(PlayerMovementTest, MakesAPlayerOnTheGroundFasterUpToTheFastestAndNoMore)
  {
    const std::int32_t player = MakeStander();

    // forward is along Y for a player who looks north; more is asked for
    // than a player goes
    const PlayerCommand command{.forward_move = 400.0f, .view_angles = {0.0f, 90.0f, 0.0f}};

    // a frame from standing: ten times the speed asked for a second
    Steer(player, command);
    EXPECT_NEAR(VelocityOf(player)[1], 10.0f * 320.0f * frame_time, tolerance);
    EXPECT_NEAR(VelocityOf(player)[0], 0.0f, tolerance);

    float fastest = 0.0f;
    for (int frame = 0; frame < 144; frame++)
    {
      Steer(player, command);
      fastest = std::max(fastest, SpeedOf(player));
    }
    EXPECT_NEAR(SpeedOf(player), 320.0f, tolerance);
    EXPECT_LE(fastest, 320.0f + tolerance);
  }

  TEST_F(PlayerMovementTest, WalksAtTheSpeedAskedForWhenThatIsLess)
  {
    const std::int32_t player = MakeStander();
    const PlayerCommand command{.forward_move = 200.0f, .side_move = 0.0f};

    for (int frame = 0; frame < 144; frame++) { Steer(player, command); }
    EXPECT_THAT(VelocityOf(player), ElementsAre(FloatNear(200.0f, tolerance), FloatNear(0.0f, tolerance), 0.0f));
  }

  TEST_F(PlayerMovementTest, GivesAPlayerInTheAirNoMoreThanThirtyAlongTheDirectionAskedFor)
  {
    const std::int32_t player = MakeJumper();

    // to the right of a player who looks east is south
    const PlayerCommand command{.side_move = 350.0f};
    Steer(player, command);
    EXPECT_THAT(VelocityOf(player), ElementsAre(FloatNear(0.0f, tolerance), FloatNear(-30.0f, tolerance), 0.0f));

    // and no more however long it is asked for
    for (int frame = 0; frame < 72; frame++) { Steer(player, command); }
    EXPECT_NEAR(VelocityOf(player)[1], -30.0f, tolerance);
  }

  TEST_F(PlayerMovementTest, GainsInTheAirByTheWholeSpeedAskedForInAShortFrame)
  {
    const std::int32_t player = MakeJumper();

    // ten times 320 a second is less than 30 in a frame this short
    constexpr float short_frame = 0.005f;
    Steer(player, {.side_move = 350.0f}, short_frame);
    EXPECT_NEAR(VelocityOf(player)[1], -10.0f * 320.0f * short_frame, tolerance);
  }

  TEST_F(PlayerMovementTest, DoesNotSlowAPlayerInTheAirDown)
  {
    const std::int32_t player = MakeJumper({0.0f, 320.0f, 100.0f});

    for (int frame = 0; frame < 72; frame++) { Steer(player, {}); }
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 320.0f, 100.0f));

    // nor does going forward add to what is faster than 30 already
    Steer(player, {.forward_move = 400.0f, .view_angles = {0.0f, 90.0f, 0.0f}});
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 320.0f, 100.0f));
  }

  TEST_F(PlayerMovementTest, MakesAPlayerFasterWhoTurnsInTheAir)
  {
    // going east as fast as a player runs, and asking to go left
    const std::int32_t player = MakeJumper({320.0f, 0.0f, 0.0f});
    PlayerCommand command{.side_move = -350.0f};

    // looking straight on, the 30 to the side are all there is to gain
    for (int frame = 0; frame < 36; frame++) { Steer(player, command); }
    EXPECT_NEAR(SpeedOf(player), std::sqrt(320.0f * 320.0f + 30.0f * 30.0f), tolerance);

    // Turning left with it, the side asked for keeps ahead of where the
    // player goes, and every frame adds to the speed.
    _fields->velocity.Set(*_machine, player, {320.0f, 0.0f, 0.0f});
    float before = SpeedOf(player);
    for (int frame = 0; frame < 36; frame++)
    {
      command.view_angles[1] += 2.0f;
      Steer(player, command);
      EXPECT_GT(SpeedOf(player), before);
      before = SpeedOf(player);
    }
    EXPECT_GT(SpeedOf(player), 345.0f);

    // and it goes where the player turned to
    EXPECT_GT(VelocityOf(player)[1], 100.0f);
  }

  TEST_F(PlayerMovementTest, SinksAPlayerInWaterSlowlyWhoAsksForNothing)
  {
    const std::int32_t player = MakeSwimmer();

    // a frame: towards 60 down, at seven tenths of it
    Steer(player, {});
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, FloatNear(-10.0f * 42.0f * frame_time, tolerance)));

    for (int frame = 0; frame < 144; frame++) { Steer(player, {}); }
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, FloatNear(-42.0f, tolerance)));
  }

  TEST_F(PlayerMovementTest, HasAPlayerInWaterSwimWhereThePlayerLooksAndUpWhenAsked)
  {
    const std::int32_t player = MakeSwimmer();

    // up alone: seven tenths of what is asked
    for (int frame = 0; frame < 144; frame++) { Steer(player, {.up_move = 200.0f}); }
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, FloatNear(140.0f, tolerance)));

    // from rest, forward while looking 30 degrees up: along that line, at
    // seven tenths of the fastest
    _fields->velocity.Set(*_machine, player, {});
    const PlayerCommand forward{.forward_move = 400.0f, .view_angles = {-30.0f, 0.0f, 0.0f}};
    for (int frame = 0; frame < 144; frame++) { Steer(player, forward); }
    EXPECT_NEAR(SpeedOf(player), 224.0f, tolerance);
    EXPECT_NEAR(VelocityOf(player)[0], 224.0f * std::cos(30.0f * 3.14159265f / 180.0f), 0.01f);
    EXPECT_NEAR(VelocityOf(player)[2], 112.0f, 0.01f);
  }

  TEST_F(PlayerMovementTest, SlowsAPlayerInWaterDownAlongEveryAxis)
  {
    const std::int32_t player = MakeSwimmer();
    _fields->velocity.Set(*_machine, player, {300.0f, 0.0f, 300.0f});

    // the water takes four times the speed a second; what is asked, to
    // sink, is slower than the player is and adds nothing
    Steer(player, {});
    const float left = 300.0f * (1.0f - 4.0f * frame_time);
    EXPECT_THAT(VelocityOf(player), ElementsAre(FloatNear(left, tolerance), 0.0f, FloatNear(left, tolerance)));
  }

  TEST_F(PlayerMovementTest, WalksAPlayerWhoStandsInWaterUpToTheKnees)
  {
    const std::int32_t player = MakeStander();
    _fields->waterlevel.Set(*_machine, player, 1.0f);

    // as on land: nothing goes up
    Steer(player, {.forward_move = 400.0f, .up_move = 200.0f});
    EXPECT_THAT(VelocityOf(player), ElementsAre(FloatNear(10.0f * 320.0f * frame_time, tolerance), 0.0f, 0.0f));
  }

  TEST_F(PlayerMovementTest, WritesWhereThePlayerLooksAndTheAnglesOfTheModel)
  {
    // going south at half the speed a player leans all the way at
    const std::int32_t player = MakeStander({0.0f, -100.0f, 0.0f});

    Steer(player, {.view_angles = {30.0f, 0.0f, 0.0f}});
    EXPECT_THAT(_fields->v_angle.Get(*_machine, player), ElementsAre(30.0f, 0.0f, 0.0f));

    // a third of the pitch the other way round, and half of two degrees
    // to the right, shown four times as large
    EXPECT_THAT(
      _fields->angles.Get(*_machine, player),
      ElementsAre(FloatNear(-10.0f, tolerance), FloatNear(0.0f, tolerance), FloatNear(4.0f, tolerance)));
  }

  TEST_F(PlayerMovementTest, LeavesTheAnglesTheGameCodeFixed)
  {
    const std::int32_t player = MakeStander();
    _fields->angles.Set(*_machine, player, {0.0f, 180.0f, 0.0f});
    _fields->fixangle.Set(*_machine, player, 1.0f);

    Steer(player, {.view_angles = {30.0f, 45.0f, 0.0f}});
    EXPECT_THAT(_fields->angles.Get(*_machine, player), ElementsAre(0.0f, 180.0f, FloatNear(0.0f, tolerance)));
    EXPECT_EQ(_fields->fixangle.Get(*_machine, player), 1.0f);
  }

  TEST_F(PlayerMovementTest, BringsAViewThatWasKickedBackByTenDegreesASecond)
  {
    const std::int32_t player = MakeStander();
    _fields->punchangle.Set(*_machine, player, {-4.0f, 0.0f, 0.0f});

    Steer(player, {}, 0.1f);
    EXPECT_THAT(_fields->punchangle.Get(*_machine, player), ElementsAre(FloatNear(-3.0f, tolerance), 0.0f, 0.0f));

    // the kick counts as looking: the model nods by a third of it
    EXPECT_NEAR(_fields->angles.Get(*_machine, player)[0], 1.0f, tolerance);

    for (int frame = 0; frame < 5; frame++) { Steer(player, {}, 0.1f); }
    EXPECT_THAT(_fields->punchangle.Get(*_machine, player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(PlayerMovementTest, DoesNotSteerThePlayerWhoIsDead)
  {
    const std::int32_t player = MakeStander({100.0f, 0.0f, 0.0f});
    _fields->health.Set(*_machine, player, 0.0f);
    _fields->punchangle.Set(*_machine, player, {-4.0f, 0.0f, 0.0f});

    Steer(player, {.forward_move = 400.0f, .view_angles = {0.0f, 90.0f, 0.0f}}, 0.1f);
    EXPECT_THAT(VelocityOf(player), ElementsAre(100.0f, 0.0f, 0.0f));
    EXPECT_THAT(_fields->angles.Get(*_machine, player), ElementsAre(0.0f, 0.0f, 0.0f));

    // the view still comes back
    EXPECT_NEAR(_fields->punchangle.Get(*_machine, player)[0], -3.0f, tolerance);
  }

  TEST_F(PlayerMovementTest, LeavesAPlayerAloneWhoNeverMoves)
  {
    const std::int32_t player = MakeStander({100.0f, 0.0f, 0.0f});
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::None));
    _fields->punchangle.Set(*_machine, player, {-4.0f, 0.0f, 0.0f});

    Steer(player, {.forward_move = 400.0f}, 0.1f);
    EXPECT_THAT(VelocityOf(player), ElementsAre(100.0f, 0.0f, 0.0f));
    EXPECT_THAT(_fields->punchangle.Get(*_machine, player), ElementsAre(-4.0f, 0.0f, 0.0f));

    // nor one that is free, or the world
    _machine->FreeEntity(player);
    Steer(player, {.forward_move = 400.0f});
    Steer(0, {.forward_move = 400.0f});
  }

  TEST_F(PlayerMovementTest, HasAPlayerWhoGoesThroughWallsGoExactlyAsAsked)
  {
    const std::int32_t player = MakeJumper({0.0f, 0.0f, -500.0f});
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::NoClip));

    // under water as well: such a player does not swim
    _fields->waterlevel.Set(*_machine, player, 3.0f);

    Steer(player, {.forward_move = 200.0f, .up_move = 100.0f});
    EXPECT_THAT(
      VelocityOf(player), ElementsAre(FloatNear(200.0f, tolerance), FloatNear(0.0f, tolerance), 100.0f));

    Steer(player, {});
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(PlayerMovementTest, KeepsAPlayerGoingWhoIsThrownOutOfTheWaterUntilOutOfIt)
  {
    const std::int32_t player = MakeJumper({0.0f, 0.0f, 225.0f});
    Set(player, QcFlag::WaterJump);
    _fields->waterlevel.Set(*_machine, player, 1.0f);
    _fields->teleport_time.Set(*_machine, player, 100.0f);
    _fields->movedir.Set(*_machine, player, {50.0f, 0.0f, 0.0f});

    // towards the edge, whatever is asked
    Steer(player, {.forward_move = -400.0f});
    EXPECT_THAT(VelocityOf(player), ElementsAre(50.0f, 0.0f, 225.0f));
    EXPECT_TRUE(Has(player, QcFlag::WaterJump));

    // out of the water it is over
    _fields->waterlevel.Set(*_machine, player, 0.0f);
    Steer(player, {});
    EXPECT_FALSE(Has(player, QcFlag::WaterJump));
    EXPECT_EQ(_fields->teleport_time.Get(*_machine, player), 0.0f);

    // and so it is when the time for it has passed
    Set(player, QcFlag::WaterJump);
    _fields->waterlevel.Set(*_machine, player, 1.0f);
    _fields->teleport_time.Set(*_machine, player, 0.5f);
    Steer(player, {});
    EXPECT_FALSE(Has(player, QcFlag::WaterJump));
  }

  TEST_F(PlayerMovementTest, DoesNotLetAPlayerBackIntoATeleporter)
  {
    const std::int32_t player = MakeStander();
    _fields->teleport_time.Set(*_machine, player, 100.0f);

    Steer(player, {.forward_move = -400.0f});
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST(PlayerMovementSettingsTest, StartsWithTheNumbersOfTheOriginal)
  {
    const PlayerMovementSettings settings;
    EXPECT_EQ(settings.max_speed, 320.0f);
    EXPECT_EQ(settings.accelerate, 10.0f);
    EXPECT_EQ(settings.friction, 4.0f);
    EXPECT_EQ(settings.stop_speed, 100.0f);
    EXPECT_EQ(settings.edge_friction, 2.0f);
    EXPECT_EQ(settings.air_speed, 30.0f);
    EXPECT_EQ(settings.roll_angle, 2.0f);
    EXPECT_EQ(settings.roll_speed, 200.0f);
  }

  TEST(PlayerMovementSettingsTest, IsReadFromTheConsoleVariablesThatWereSet)
  {
    QcConsoleVariables variables;
    variables.Set("sv_maxspeed", "400");
    variables.Set("sv_friction", "6");
    variables.Set("edgefriction", "3");

    const PlayerMovementSettings settings = PlayerMovementSettings::From(variables);
    EXPECT_EQ(settings.max_speed, 400.0f);
    EXPECT_EQ(settings.friction, 6.0f);
    EXPECT_EQ(settings.edge_friction, 3.0f);

    // those the variables start with, and one nobody set
    EXPECT_EQ(settings.accelerate, 10.0f);
    EXPECT_EQ(settings.stop_speed, 100.0f);
    EXPECT_EQ(settings.roll_angle, 2.0f);
  }

  TEST_F(PlayerMovementTest, SteersByTheSettingsItIsGiven)
  {
    const std::int32_t player = MakeStander();
    PlayerMovementSettings settings;
    settings.max_speed = 100.0f;
    _movement->SetSettings(settings);
    EXPECT_EQ(_movement->GetSettings().max_speed, 100.0f);

    for (int frame = 0; frame < 144; frame++) { Steer(player, {.forward_move = 400.0f}); }
    EXPECT_NEAR(VelocityOf(player)[0], 100.0f, tolerance);
  }
}
