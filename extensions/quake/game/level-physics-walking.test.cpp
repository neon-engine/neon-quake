#include "level-physics.hpp"

#include <cmath>
#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-contents.hpp"
#include "player-moving.test.hpp"

// A player moved by the mover of a level, as the original moves one: steered
// by `PlayerMovement` and walked by `LevelPhysics`, frame after frame.
namespace
{
  using quake::BspContents;
  using quake::Length;
  using quake::LevelPhysics;
  using quake::LevelVector;
  using quake::LevelWorld;
  using quake::PlayerCommand;
  using quake::PlayerWorld;
  using quake::QcFlag;
  using quake::QcMoveType;
  using quake::QcSolid;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::FloatNear;
  using ::testing::IsEmpty;

  class LevelPhysicsWalkingTest : public quake::PlayerMovingTest
  {
  protected:
    /// As fast as a player goes, to each of the four sides.
    static constexpr PlayerCommand east{.forward_move = 400.0f};
    static constexpr PlayerCommand north{.forward_move = 400.0f, .view_angles = {0.0f, 90.0f, 0.0f}};
    static constexpr PlayerCommand west{.forward_move = 400.0f, .view_angles = {0.0f, 180.0f, 0.0f}};

    /// How far east a player gets before the side of something that
    /// starts at `edge`.
    [[nodiscard]] static float Before(const float edge)
    {
      return edge - 16.0f - epsilon;
    }
  };

  TEST_F(LevelPhysicsWalkingTest, LeavesAPlayerAloneUntilItIsToldToWalkThem)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 100.0f});
    _physics->SetWalksClients(false);
    EXPECT_FALSE(_physics->GetWalksClients());

    Play(player, east, 10);
    EXPECT_THAT(OriginOf(player), ElementsAre(0.0f, 0.0f, 100.0f));
  }

  TEST_F(LevelPhysicsWalkingTest, HasAPlayerStandStillOnTheFloor)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});

    Play(player, {}, 72);
    EXPECT_THAT(OriginOf(player), ElementsAre(0.0f, 0.0f, FloatNear(standing, tolerance)));
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, player), 0);
    EXPECT_EQ(_fields->waterlevel.Get(*_machine, player), 0.0f);
    EXPECT_EQ(_fields->watertype.Get(*_machine, player), static_cast<float>(BspContents::Empty));

    // where the player was free last is noted in every frame
    EXPECT_THAT(_fields->oldorigin.Get(*_machine, player), ElementsAre(0.0f, 0.0f, FloatNear(standing, tolerance)));
  }

  TEST_F(LevelPhysicsWalkingTest, WalksAPlayerAlongTheFloorAsFastAsAPlayerGoes)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});

    // a second to get going, and a second at full speed
    Play(player, north, 72);
    const float after_a_second = OriginOf(player)[1];
    Play(player, north, 72);

    EXPECT_NEAR(Length(VelocityOf(player)), 320.0f, tolerance);
    EXPECT_NEAR(OriginOf(player)[1] - after_a_second, 320.0f, 0.5f);
    EXPECT_NEAR(OriginOf(player)[0], 0.0f, tolerance);
    EXPECT_NEAR(OriginOf(player)[2], standing, tolerance);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));

    // and to a stop when nothing is asked any more
    Play(player, {}, 72);
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(LevelPhysicsWalkingTest, WalksAPlayerUpAStepOfSixteenAndNotUpOneOfTwenty)
  {
    const std::int32_t player = MakePlayer({50.0f, 0.0f, standing});

    // onto the first step
    Play(player, east, 36);
    EXPECT_GT(OriginOf(player)[0], PlayerWorld::step_edge);
    EXPECT_NEAR(OriginOf(player)[2], PlayerWorld::step_height + standing, tolerance);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));

    // and against the second, which is two units too high
    Play(player, east, 72);
    EXPECT_NEAR(OriginOf(player)[0], Before(PlayerWorld::high_step_edge), tolerance);
    EXPECT_NEAR(OriginOf(player)[2], PlayerWorld::step_height + standing, tolerance);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
    EXPECT_FALSE(_collision->TestPosition(player));
  }

  TEST_F(LevelPhysicsWalkingTest, DoesNotWalkAPlayerUpAStepWhoIsInTheAir)
  {
    // coming down next to the step, with the feet under its top, and
    // going towards it
    const std::int32_t player = MakePlayer({Before(PlayerWorld::step_edge) - 4.0f, 0.0f, standing + 12.0f});
    _fields->velocity.Set(*_machine, player, {320.0f, 0.0f, 0.0f});

    Play(player, east, 2);
    EXPECT_NEAR(OriginOf(player)[0], Before(PlayerWorld::step_edge), tolerance);
    EXPECT_LT(OriginOf(player)[2], standing + 12.0f);

    // once the player stands, the step is taken
    Play(player, east, 36);
    EXPECT_GT(OriginOf(player)[0], PlayerWorld::step_edge);
  }

  TEST_F(LevelPhysicsWalkingTest, SlidesAPlayerAlongAWall)
  {
    // on the first step, going north-east into the side of the second
    const float height = PlayerWorld::step_height + standing;
    const std::int32_t player = MakePlayer({150.0f, 0.0f, height});
    const PlayerCommand north_east{.forward_move = 400.0f, .view_angles = {0.0f, 45.0f, 0.0f}};

    Play(player, north_east, 144);
    EXPECT_NEAR(OriginOf(player)[0], Before(PlayerWorld::high_step_edge), tolerance);
    EXPECT_GT(OriginOf(player)[1], 200.0f);
    EXPECT_NEAR(OriginOf(player)[2], height, tolerance);

    // what went into the wall is gone, what goes along it is kept
    EXPECT_NEAR(VelocityOf(player)[0], 0.0f, tolerance);
    EXPECT_GT(VelocityOf(player)[1], 50.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, DropsAPlayerOffAStepWithGravityOntoTheFloor)
  {
    const float height = PlayerWorld::step_height + standing;
    const std::int32_t player = MakePlayer({PlayerWorld::step_edge + 20.0f, 0.0f, height});
    _fields->groundentity.Set(*_machine, player, 5);

    // over the edge: in the air the player does not stand, and falls
    // faster with every frame
    bool was_in_the_air = false;
    float fastest_fall = 0.0f;
    for (int frame = 0; frame < 72 && OriginOf(player)[0] > 0.0f; frame++)
    {
      Play(player, west, 1);
      if (!Has(player, QcFlag::OnGround))
      {
        was_in_the_air = true;
        fastest_fall = std::min(fastest_fall, VelocityOf(player)[2]);
      }
    }
    EXPECT_TRUE(was_in_the_air);

    // sixteen units down take a fifth of a second, which is 160 a second
    EXPECT_LT(fastest_fall, -140.0f);
    EXPECT_GT(fastest_fall, -180.0f);

    EXPECT_NEAR(OriginOf(player)[2], standing, tolerance);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, player), 0);
    EXPECT_EQ(VelocityOf(player)[2], 0.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, FallsByTheGravityOfTheLevel)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 200.0f});

    _running->Advance(0.05f);
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, -40.0f));
    EXPECT_THAT(OriginOf(player), ElementsAre(0.0f, 0.0f, 198.0f));
    EXPECT_FALSE(Has(player, QcFlag::OnGround));
  }

  TEST_F(LevelPhysicsWalkingTest, NotesHowDeepAPlayerIsInWater)
  {
    const auto level_at = [this](const float height)
    {
      const std::int32_t player = MakePlayer({-200.0f, 0.0f, height});
      _physics->MoveEntity(player, 0.001f);
      const float level = _fields->waterlevel.Get(*_machine, player);
      const float type = _fields->watertype.Get(*_machine, player);
      EXPECT_EQ(type, static_cast<float>(level > 0.0f ? BspContents::Water : BspContents::Empty)) << height;
      _machine->FreeEntity(player);
      return level;
    };
    const float surface = PlayerWorld::water_surface;

    // the feet are a unit over the lowest of the box, the waist is its
    // middle, and the eyes are 22 over the origin
    EXPECT_EQ(level_at(surface + 24.0f), 0.0f);
    EXPECT_EQ(level_at(surface + 22.0f), 1.0f);
    EXPECT_EQ(level_at(surface - 3.0f), 1.0f);
    EXPECT_EQ(level_at(surface - 5.0f), 2.0f);
    EXPECT_EQ(level_at(surface - 21.0f), 2.0f);
    EXPECT_EQ(level_at(surface - 23.0f), 3.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, SinksAPlayerInWaterSlowlyAndWithoutGravity)
  {
    const std::int32_t player = MakePlayer({-200.0f, 0.0f, -100.0f});

    // the first frame notes the water, and falls as on land
    Play(player, {}, 1);
    EXPECT_EQ(_fields->waterlevel.Get(*_machine, player), 3.0f);

    _fields->velocity.Set(*_machine, player, {});
    const float before = OriginOf(player)[2];
    Play(player, {}, 72);

    // no faster than the water lets a player sink
    EXPECT_NEAR(VelocityOf(player)[2], -42.0f, tolerance);
    EXPECT_LT(OriginOf(player)[2], before - 30.0f);
    EXPECT_GT(OriginOf(player)[2], before - 42.0f);
    EXPECT_FALSE(Has(player, QcFlag::OnGround));
    EXPECT_FALSE(_collision->TestPosition(player));
  }

  TEST_F(LevelPhysicsWalkingTest, HasAPlayerSwimUpAndAlong)
  {
    const std::int32_t player = MakePlayer({-200.0f, 0.0f, -150.0f});
    Play(player, {}, 1);
    _fields->velocity.Set(*_machine, player, {});
    const float before = OriginOf(player)[2];

    // up, at seven tenths of what is asked
    const PlayerCommand up{.up_move = 200.0f};
    Play(player, up, 36);
    EXPECT_NEAR(VelocityOf(player)[2], 140.0f, 0.5f);
    EXPECT_GT(OriginOf(player)[2], before + 50.0f);
    EXPECT_EQ(_fields->waterlevel.Get(*_machine, player), 3.0f);
    EXPECT_FALSE(Has(player, QcFlag::OnGround));

    // and from rest north, looking a little down
    _fields->velocity.Set(*_machine, player, {});
    const PlayerCommand dive{.forward_move = 400.0f, .view_angles = {20.0f, 90.0f, 0.0f}};
    const LevelVector from = OriginOf(player);
    Play(player, dive, 36);
    EXPECT_GT(OriginOf(player)[1], from[1] + 60.0f);
    EXPECT_LT(OriginOf(player)[2], from[2] - 20.0f);
    EXPECT_NEAR(OriginOf(player)[0], from[0], tolerance);
    EXPECT_FALSE(Has(player, QcFlag::OnGround));
  }

  TEST_F(LevelPhysicsWalkingTest, HoldsAPlayerWhoSwimsUpAtTheSurface)
  {
    const std::int32_t player = MakePlayer({-200.0f, 0.0f, -80.0f});

    // Up for three seconds: out of the water to the waist the player
    // falls again, and so stays about where the water ends.
    Play(player, {.up_move = 200.0f}, 216);
    EXPECT_GT(OriginOf(player)[2], PlayerWorld::water_surface - 24.0f);
    EXPECT_LT(OriginOf(player)[2], PlayerWorld::water_surface + 24.0f);
    EXPECT_GE(_fields->waterlevel.Get(*_machine, player), 1.0f);
    EXPECT_FALSE(Has(player, QcFlag::OnGround));
  }

  TEST_F(LevelPhysicsWalkingTest, HasAPlayerWhoWalksIntoATriggerTouchIt)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});
    const std::int32_t trigger =
      Make({0.0f, 150.0f, 0.0f}, {-20.0f, -20.0f, 0.0f}, {20.0f, 20.0f, 60.0f}, QcSolid::Trigger);
    _fields->touch.Set(*_machine, trigger, FunctionOf("touch"));

    Play(player, {}, 10);
    EXPECT_THAT(_touches, IsEmpty());

    // through it without being held up
    Play(player, north, 72);
    EXPECT_THAT(_touches, Contains(::testing::Field(&Call::self, trigger)));
    for (const Call &call : _touches) { EXPECT_EQ(call.other, player); }
    EXPECT_GT(OriginOf(player)[1], 200.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, TellsAPlayerAndWhatThePlayerRunsIntoOfEachOther)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});
    const std::int32_t crate = Make({0.0f, 100.0f, 0.0f});
    _fields->touch.Set(*_machine, crate, FunctionOf("touch"));

    Play(player, north, 72);
    EXPECT_THAT(_touches, Contains(::testing::AllOf(
                  ::testing::Field(&Call::self, crate), ::testing::Field(&Call::other, player))));
    EXPECT_NEAR(OriginOf(player)[1], 100.0f - 32.0f - epsilon, tolerance);
  }

  TEST_F(LevelPhysicsWalkingTest, CarriesAPlayerOnALift)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 40.0f + standing});
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 40.0f}, {0.0f, 0.0f, 40.0f});

    Play(player, {}, 72);
    EXPECT_NEAR(OriginOf(lift)[2], 80.0f, 0.01f);
    EXPECT_NEAR(OriginOf(player)[2], 80.0f + standing, 0.1f);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
    EXPECT_EQ(_fields->groundentity.Get(*_machine, player), lift);
    EXPECT_THAT(_blocks, IsEmpty());
    EXPECT_FALSE(_collision->TestPosition(player));

    // and down again, the player following by falling
    _fields->velocity.Set(*_machine, lift, {0.0f, 0.0f, -40.0f});
    Play(player, {}, 36);
    EXPECT_NEAR(OriginOf(player)[2], OriginOf(lift)[2] + standing, 1.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, StopsALiftThatWouldSqueezeAPlayer)
  {
    // the lift comes down on a player who stands on the floor
    const std::int32_t player = MakePlayer({0.0f, 0.0f, standing});
    const std::int32_t lift = MakeLift({0.0f, 0.0f, 100.0f}, {0.0f, 0.0f, -40.0f});

    Play(player, {}, 144);
    EXPECT_THAT(_blocks, Contains(::testing::AllOf(
                  ::testing::Field(&Call::self, lift), ::testing::Field(&Call::other, player))));

    // it stays over the player's head, and the player where the player was
    EXPECT_GT(OriginOf(lift)[2] - LevelWorld::slab_thickness, 56.0f);
    EXPECT_LT(OriginOf(lift)[2] - LevelWorld::slab_thickness, 58.0f);
    EXPECT_THAT(OriginOf(player), ElementsAre(0.0f, 0.0f, FloatNear(standing, tolerance)));
    EXPECT_FALSE(_collision->TestPosition(player));
  }

  TEST_F(LevelPhysicsWalkingTest, GetsAPlayerOutOfWhatIsSolid)
  {
    // half a unit into the floor
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 23.5f});
    ASSERT_TRUE(_collision->TestPosition(player));

    // back to where the player was free last
    _fields->oldorigin.Set(*_machine, player, {10.0f, 0.0f, standing});
    Play(player, {}, 1);
    EXPECT_FALSE(_collision->TestPosition(player));
    EXPECT_NEAR(OriginOf(player)[0], 10.0f, tolerance);

    // or, when that is solid as well, up by a unit
    _fields->origin.Set(*_machine, player, {0.0f, 0.0f, 23.5f});
    _fields->oldorigin.Set(*_machine, player, {0.0f, 0.0f, 0.0f});
    Play(player, {}, 1);
    EXPECT_FALSE(_collision->TestPosition(player));
    EXPECT_NEAR(OriginOf(player)[0], 0.0f, 1.0f + tolerance);
    EXPECT_NEAR(OriginOf(player)[1], 0.0f, 1.0f + tolerance);
    EXPECT_LT(OriginOf(player)[2], 25.0f);
  }

  TEST_F(LevelPhysicsWalkingTest, TossesAPlayerWhoDied)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 100.0f});
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::Toss));
    _fields->health.Set(*_machine, player, 0.0f);
    _fields->velocity.Set(*_machine, player, {0.0f, 100.0f, 200.0f});

    // asking for something changes nothing
    Play(player, north, 144);
    EXPECT_TRUE(Has(player, QcFlag::OnGround));
    EXPECT_NEAR(OriginOf(player)[2], standing, tolerance);
    EXPECT_GT(OriginOf(player)[1], 50.0f);
    EXPECT_THAT(VelocityOf(player), ElementsAre(0.0f, 0.0f, 0.0f));
  }

  TEST_F(LevelPhysicsWalkingTest, FliesAPlayerWithoutGravityAndThroughWallsWhenAskedTo)
  {
    const std::int32_t player = MakePlayer({0.0f, 0.0f, 100.0f});
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::Fly));

    // flying: no gravity, steered as in the air, and stopped by the wall
    // of the second step
    Play(player, {}, 36);
    EXPECT_THAT(OriginOf(player), ElementsAre(0.0f, 0.0f, 100.0f));
    _fields->origin.Set(*_machine, player, {150.0f, 0.0f, 45.0f});
    Play(player, east, 144);
    EXPECT_NEAR(VelocityOf(player)[0], 0.0f, tolerance);
    EXPECT_NEAR(OriginOf(player)[0], Before(PlayerWorld::high_step_edge), tolerance);
    EXPECT_NEAR(OriginOf(player)[2], 45.0f, tolerance);

    // through it for one who goes through everything, and up as asked
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::NoClip));
    Play(player, {.forward_move = 200.0f, .up_move = 100.0f}, 72);
    EXPECT_GT(OriginOf(player)[0], PlayerWorld::wall_edge);
    EXPECT_NEAR(OriginOf(player)[2], 145.0f, 0.5f);

    // and one that never moves stays
    _fields->movetype.Set(*_machine, player, static_cast<float>(QcMoveType::None));
    const LevelVector place = OriginOf(player);
    Play(player, east, 10);
    EXPECT_EQ(OriginOf(player), place);
  }

  TEST_F(LevelPhysicsWalkingTest, WalksUpStepsNoHigherThanEighteen)
  {
    EXPECT_EQ(LevelPhysics::step_height, 18.0f);
  }
}
