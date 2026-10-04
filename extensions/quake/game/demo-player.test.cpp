#include "demo-player.hpp"

#include <cstdint>
#include <span>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "demo-bytes.test.hpp"
#include "demo-recording-listener.test.hpp"
#include "qc-item.hpp"

// The tests of DemoPlayer with recordings made here, block by block.
namespace
{
  using quake::DemoBytes;
  using quake::DemoClientData;
  using quake::DemoEntity;
  using quake::DemoEntityState;
  using quake::DemoIntermission;
  using quake::DemoPlayer;
  using quake::DemoProtocol;
  using quake::DemoRecordingListener;
  using quake::LevelVector;
  using quake::PlayerStats;
  using quake::QcItem;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  class DemoPlayerTest : public ::testing::Test
  {
  protected:
    /// How close a number that was worked out has to be.
    static constexpr float close = 0.001f;

    DemoRecordingListener _listener;
    DemoPlayer _player{_listener};

    /// The recording under way: the line of the track, then blocks.
    DemoBytes _file;

    void SetUp() override
    {
      _file.Line("-1");
    }

    /// Adds a block to the recording.
    void Block(const DemoBytes &message, const LevelVector &view_angles = {})
    {
      _file.Block(view_angles, message);
    }

    /// The message that begins a level, with three models and two sounds.
    static DemoBytes ServerInfo(const std::int32_t version = DemoProtocol::netquake, const std::uint32_t flags = 0)
    {
      DemoBytes bytes;
      bytes.Byte(11).Long(version);
      if (version == DemoProtocol::rmq) { bytes.Long(flags); }
      bytes.Byte(2).Byte(0).Text("The Level");
      bytes.Text("maps/test.bsp").Text("*1").Text("progs/player.mdl").Text("");
      bytes.Text("misc/a.wav").Text("misc/b.wav").Text("");
      return bytes;
    }

    /// The baseline of an entity at the origin of the level.
    static DemoBytes Baseline(const std::int32_t entity, const std::int32_t model, const LevelVector &origin = {})
    {
      DemoBytes bytes;
      bytes.Byte(22).Short(entity).Byte(static_cast<std::uint32_t>(model)).Byte(0).Byte(0).Byte(0);
      bytes.Coord(origin[0]).Angle(0.0f).Coord(origin[1]).Angle(0.0f).Coord(origin[2]).Angle(0.0f);
      return bytes;
    }

    /// The update of an entity with its place and its yaw. With `step`
    /// it is one that moves in steps.
    static DemoBytes Update(
      const std::int32_t entity, const LevelVector &origin, const float yaw = 0.0f, const bool step = false)
    {
      DemoBytes bytes;
      bytes.Byte(0x80 | 0x02 | 0x04 | 0x08 | 0x10 | (step ? 0x20 : 0)).Byte(static_cast<std::uint32_t>(entity));
      bytes.Coord(origin[0]).Coord(origin[1]).Angle(yaw).Coord(origin[2]);
      return bytes;
    }

    /// The time of the server, which a packet of a running level starts
    /// with.
    static DemoBytes Time(const float seconds)
    {
      DemoBytes bytes;
      bytes.Byte(7).Float(seconds);
      return bytes;
    }

    /// The blocks a client joins a level with: the level, the baselines
    /// of the player as entity 1 and of a door as entity 2, and the view.
    void AddSignon(const std::int32_t version = DemoProtocol::netquake, const std::uint32_t flags = 0)
    {
      Block(ServerInfo(version, flags).Byte(25).Byte(1));
      Block(Baseline(1, 3).Add(Baseline(2, 2, {100.0f, 0.0f, 0.0f})).Byte(25).Byte(2));
      Block(DemoBytes().Byte(5).Short(1).Byte(25).Byte(3));
    }

    /// Opens the recording as it is by now.
    void Open()
    {
      std::string problem;
      ASSERT_TRUE(_player.Open(_file.Get(), problem)) << problem;
    }

    /// The entity of a number that has to be shown.
    const DemoEntity &Shown(const std::int32_t number)
    {
      static const DemoEntity none{};
      const DemoEntity *entity = _player.FindEntity(number);
      EXPECT_NE(entity, nullptr) << "entity " << number << " is not shown";
      return entity != nullptr ? *entity : none;
    }
  };

  // Opening.

  TEST_F(DemoPlayerTest, RefusesBytesThatAreNoRecordingAndHasNothingOpenThen)
  {
    const DemoBytes wrong = DemoBytes().Text("no recording");
    std::string problem;
    EXPECT_FALSE(_player.Open(wrong.Get(), problem));
    EXPECT_THAT(problem, HasSubstr("music track"));
    EXPECT_FALSE(_player.IsOpen());

    // nothing happens then
    _player.Advance(1.0f);
    EXPECT_FALSE(_player.IsOver());
    EXPECT_FALSE(_player.HasFailed());
    EXPECT_FALSE(_player.HasLevel());
    EXPECT_EQ(_player.GetTime(), 0.0f);
    EXPECT_THAT(_listener.events, IsEmpty());
  }

  TEST_F(DemoPlayerTest, ReadsNothingOfARecordingUntilTheClockIsMovedOn)
  {
    AddSignon();
    Open();

    EXPECT_TRUE(_player.IsOpen());
    EXPECT_FALSE(_player.HasLevel());
    EXPECT_FALSE(_player.IsOver());
    EXPECT_THAT(_listener.events, IsEmpty());
    EXPECT_THAT(_player.GetEntities(), IsEmpty());
    EXPECT_EQ(_player.GetMapModelName(), "");
  }

  TEST_F(DemoPlayerTest, KeepsItsOwnCopyOfTheBytes)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {1.0f, 2.0f, 3.0f})));
    Block(Time(1.1f));
    Open();
    _file.bytes.assign(_file.bytes.size(), 0xff);

    _player.Advance(0.0f);
    EXPECT_FALSE(_player.HasFailed()) << _player.GetProblem();
    EXPECT_EQ(_player.GetLevelName(), "The Level");
  }

  // Joining a level.

  TEST_F(DemoPlayerTest, ReadsEveryBlockOfJoiningALevelAtOnceAndTellsOfTheLevel)
  {
    AddSignon();
    Block(Time(5.0f).Add(Update(1, {10.0f, 20.0f, 30.0f}, 90.0f)));
    Block(Time(5.1f).Add(Update(1, {20.0f, 20.0f, 30.0f}, 90.0f)));
    Open();

    _player.Advance(0.0f);

    ASSERT_EQ(_listener.levels.size(), 1u);
    EXPECT_THAT(_listener.events, ElementsAre("level: The Level"));
    EXPECT_TRUE(_player.HasLevel());
    EXPECT_TRUE(_player.IsSignedOn());
    EXPECT_FALSE(_player.IsOver());
    EXPECT_EQ(_player.GetLevelName(), "The Level");
    EXPECT_EQ(_player.GetMapModelName(), "maps/test.bsp");
    EXPECT_THAT(_player.GetModelNames(), ElementsAre("", "maps/test.bsp", "*1", "progs/player.mdl"));
    EXPECT_THAT(_player.GetSoundNames(), ElementsAre("", "misc/a.wav", "misc/b.wav"));
    EXPECT_EQ(_player.GetModelName(3), "progs/player.mdl");
    EXPECT_EQ(_player.GetModelName(4), "");
    EXPECT_EQ(_player.GetModelName(-1), "");
    EXPECT_EQ(_player.GetSoundName(2), "misc/b.wav");
    EXPECT_EQ(_player.GetSoundName(3), "");
    EXPECT_EQ(_player.GetServerInfo().max_clients, 2);
    EXPECT_EQ(_player.GetScores().size(), 2u);
    EXPECT_EQ(_player.GetViewEntity(), 1);

    // the first packet of the running level was read, and no more: the
    // clock is put a tenth of a second before it, as in the original
    EXPECT_NEAR(_player.GetTime(), 4.9f, close);
    ASSERT_EQ(_player.GetEntities().size(), 1u);
    const DemoEntity &player = Shown(1);
    EXPECT_EQ(player.state.model, 3);
    EXPECT_EQ(player.state.origin, (LevelVector{10.0f, 20.0f, 30.0f}));
    EXPECT_EQ(player.state.angles, (LevelVector{0.0f, 90.0f, 0.0f}));
    EXPECT_TRUE(player.is_placed_anew);
  }

  TEST_F(DemoPlayerTest, IsNotJoinedBeforeTheFirstUpdateOfAnEntity)
  {
    AddSignon();
    Open();

    // the recording ends before the level runs
    _player.Advance(0.1f);
    EXPECT_TRUE(_player.HasLevel());
    EXPECT_FALSE(_player.IsSignedOn());
    EXPECT_TRUE(_player.IsOver());
    EXPECT_FALSE(_player.HasFailed());
    EXPECT_THAT(_player.GetEntities(), IsEmpty());
  }

  // The clock.

  TEST_F(DemoPlayerTest, ReadsABlockWhenTheClockIsPastTheTimeOfTheOneBefore)
  {
    AddSignon();
    for (std::int32_t packet = 0; packet < 5; packet++)
    {
      const float time = 2.0f + 0.1f * static_cast<float>(packet);
      Block(Time(time).Add(Update(1, {static_cast<float>(packet), 0.0f, 0.0f})).Byte(27));
    }
    Open();

    // every packet has a monster killed, which counts the packets read
    _player.Advance(0.0f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 1);
    EXPECT_NEAR(_player.GetTime(), 1.9f, close);

    // up to the time of the packet nothing more is read
    _player.Advance(0.04f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 1);
    _player.Advance(0.04f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 1);
    EXPECT_NEAR(_player.GetTime(), 1.98f, close);

    // past it the next one is
    _player.Advance(0.04f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 2);
    EXPECT_NEAR(_player.GetTime(), 2.02f, close);
    _player.Advance(0.07f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 2);

    // a long frame reads all the packets it went past, and one more
    _player.Advance(0.23f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 5);
    EXPECT_NEAR(_player.GetTime(), 2.32f, close);
    EXPECT_FALSE(_player.IsOver());

    // and with no block left the recording is over
    _player.Advance(0.2f);
    EXPECT_TRUE(_player.IsOver());
    EXPECT_FALSE(_player.HasFailed());
    EXPECT_NEAR(_player.GetTime(), 2.4f, close);

    // what was shown last stays, and the clock stands
    _player.Advance(5.0f);
    EXPECT_NEAR(_player.GetTime(), 2.4f, close);
    EXPECT_EQ(_player.GetEntities().size(), 1u);
  }

  TEST_F(DemoPlayerTest, DoesNotMoveTheClockBack)
  {
    AddSignon();
    Block(Time(2.0f).Add(Update(1, {})));
    Block(Time(2.1f).Add(Update(1, {})));
    Open();
    _player.Advance(0.0f);
    const float time = _player.GetTime();

    _player.Advance(-1.0f);
    EXPECT_EQ(_player.GetTime(), time);
  }

  // Where the entities are.

  TEST_F(DemoPlayerTest, PutsAnEntityBetweenItsLastTwoUpdatesForTheClock)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {0.0f, 0.0f, 0.0f}, 0.0f)));
    Block(Time(1.1f).Add(Update(1, {10.0f, -20.0f, 40.0f}, 90.0f)));
    Block(Time(1.2f).Add(Update(1, {10.0f, -20.0f, 40.0f}, 90.0f)));
    Open();

    _player.Advance(0.0f);
    _player.Advance(0.09f);
    EXPECT_EQ(Shown(1).state.origin, (LevelVector{}));

    // a quarter of the way to the second packet
    _player.Advance(0.035f);
    EXPECT_NEAR(Shown(1).state.origin[0], 2.5f, 0.01f);
    EXPECT_NEAR(Shown(1).state.origin[1], -5.0f, 0.01f);
    EXPECT_NEAR(Shown(1).state.origin[2], 10.0f, 0.01f);
    EXPECT_NEAR(Shown(1).state.angles[1], 22.5f, 0.01f);
    EXPECT_FALSE(Shown(1).is_placed_anew);

    // three quarters
    _player.Advance(0.05f);
    EXPECT_NEAR(Shown(1).state.origin[0], 7.5f, 0.01f);
    EXPECT_NEAR(Shown(1).state.angles[1], 67.5f, 0.01f);

    // past it: all the way, as the third packet has it where the second had
    _player.Advance(0.05f);
    EXPECT_NEAR(Shown(1).state.origin[0], 10.0f, close);
    EXPECT_NEAR(Shown(1).state.angles[1], 90.0f, close);
  }

  TEST_F(DemoPlayerTest, TurnsAnEntityTheShortWayAround)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {}, 170.0f)));
    Block(Time(1.1f).Add(Update(1, {}, -170.0f)));
    Open();

    _player.Advance(0.0f);
    _player.Advance(0.15f);

    // the angles are bytes: 170 is written as 168.75, -170 as -168.75
    EXPECT_NEAR(Shown(1).state.angles[1], 180.0f, 0.01f);
  }

  TEST_F(DemoPlayerTest, DoesNotPutAnEntityBetweenTheTwoEndsOfATeleporter)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {0.0f, 0.0f, 0.0f})));
    Block(Time(1.1f).Add(Update(1, {0.0f, 100.5f, 0.0f})));
    Block(Time(1.2f).Add(Update(1, {0.0f, 100.5f, 0.0f})));
    Open();

    _player.Advance(0.0f);
    _player.Advance(0.09f);
    _player.Advance(0.02f);

    // more than a hundred units on one axis: it is there at once
    EXPECT_EQ(Shown(1).state.origin, (LevelVector{0.0f, 100.5f, 0.0f}));
    EXPECT_TRUE(Shown(1).is_placed_anew);

    _player.Advance(0.1f);
    EXPECT_EQ(Shown(1).state.origin, (LevelVector{0.0f, 100.5f, 0.0f}));
    EXPECT_FALSE(Shown(1).is_placed_anew);
  }

  TEST_F(DemoPlayerTest, TakesAnEntityAwayThatTheLastPacketHadNoUpdateFor)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})).Add(Update(2, {50.0f, 0.0f, 0.0f})));
    Block(Time(1.1f).Add(Update(1, {})));
    Block(Time(1.2f).Add(Update(1, {})));
    Block(Time(1.3f).Add(Update(1, {})).Add(Update(2, {90.0f, 0.0f, 0.0f})));
    Block(Time(1.4f).Add(Update(1, {})).Add(Update(2, {90.0f, 0.0f, 0.0f})));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(_player.GetEntities().size(), 2u);
    EXPECT_EQ(Shown(2).state.model, 2);
    EXPECT_EQ(Shown(2).number, 2);

    _player.Advance(0.15f);
    ASSERT_EQ(_player.GetEntities().size(), 1u);
    EXPECT_EQ(_player.GetEntities()[0].number, 1);
    EXPECT_EQ(_player.FindEntity(2), nullptr);
    _player.Advance(0.1f);
    EXPECT_EQ(_player.FindEntity(2), nullptr);

    // back again it is where the update has it, not on its way from where
    // it was last
    _player.Advance(0.1f);
    EXPECT_EQ(Shown(2).state.origin, (LevelVector{90.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(Shown(2).is_placed_anew);
  }

  TEST_F(DemoPlayerTest, TakesWhatAnUpdateDoesNotSayFromTheBaselineNotFromTheUpdateBefore)
  {
    AddSignon();
    // the door, entity 2, with a frame, a skin, a colormap, and effects
    Block(Time(1.0f).Add(Update(1, {})).Byte(0x81 | 0x40).Byte(0x38).Byte(2).Byte(7).Byte(5).Byte(6).Byte(8));
    // and then with nothing but its number
    Block(Time(1.1f).Add(Update(1, {})).Byte(0x80).Byte(2));
    Block(Time(1.2f).Add(Update(1, {})).Byte(0x80).Byte(2));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(Shown(2).state.model, 2);
    EXPECT_EQ(Shown(2).state.frame, 7);
    EXPECT_EQ(Shown(2).state.colormap, 5);
    EXPECT_EQ(Shown(2).state.skin, 6);
    EXPECT_EQ(Shown(2).state.effects, 8);
    EXPECT_EQ(Shown(2).state.origin, (LevelVector{100.0f, 0.0f, 0.0f}));

    _player.Advance(0.15f);
    EXPECT_EQ(Shown(2).state.frame, 0);
    EXPECT_EQ(Shown(2).state.colormap, 0);
    EXPECT_EQ(Shown(2).state.skin, 0);
    EXPECT_EQ(Shown(2).state.effects, 0);
    EXPECT_EQ(Shown(2).state.alpha, DemoEntityState::default_alpha);
    EXPECT_EQ(Shown(2).state.scale, DemoEntityState::default_scale);
  }

  TEST_F(DemoPlayerTest, DoesNotShowAnEntityWhoseModelIsNone)
  {
    AddSignon();
    // the model of the player is set to none
    Block(Time(1.0f).Byte(0x81).Byte(0x04).Byte(1).Byte(0));
    Block(Time(1.1f).Byte(0x81).Byte(0x04).Byte(1).Byte(0));
    Open();

    _player.Advance(0.0f);
    EXPECT_TRUE(_player.IsSignedOn());
    EXPECT_THAT(_player.GetEntities(), IsEmpty());
    EXPECT_EQ(_player.FindEntity(1), nullptr);
  }

  TEST_F(DemoPlayerTest, MovesAnEntityThatMovesInStepsFromTheStepBeforeOverATenthOfASecond)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})).Add(Update(2, {0.0f, 0.0f, 0.0f}, 0.0f, true)));
    Block(Time(1.1f).Add(Update(1, {})).Add(Update(2, {0.0f, 0.0f, 0.0f}, 0.0f, true)));
    Block(Time(1.2f).Add(Update(1, {})).Add(Update(2, {16.0f, 0.0f, 0.0f}, 90.0f, true)));
    Block(Time(1.3f).Add(Update(1, {})).Add(Update(2, {16.0f, 0.0f, 0.0f}, 90.0f, true)));
    Block(Time(1.4f).Add(Update(1, {})).Add(Update(2, {16.0f, 0.0f, 0.0f}, 90.0f, true)));
    Open();

    _player.Advance(0.0f);
    EXPECT_TRUE(Shown(2).moves_in_steps);
    EXPECT_FALSE(Shown(1).moves_in_steps);
    _player.Advance(0.09f);
    EXPECT_EQ(Shown(2).state.origin, (LevelVector{}));

    // the step comes with the third packet, which is read when the clock
    // is past the second: it starts from where the entity stood
    _player.Advance(0.1f);
    _player.Advance(0.05f);
    EXPECT_NEAR(_player.GetTime(), 1.14f, close);
    EXPECT_NEAR(Shown(2).state.origin[0], 0.0f, 0.01f);

    // from when it came, the step is taken over a tenth of a second
    _player.Advance(0.05f);
    EXPECT_NEAR(Shown(2).state.origin[0], 8.0f, 0.1f);
    EXPECT_NEAR(Shown(2).state.angles[1], 45.0f, 0.5f);
    _player.Advance(0.05f);
    EXPECT_NEAR(Shown(2).state.origin[0], 16.0f, 0.1f);
    EXPECT_NEAR(Shown(2).state.angles[1], 90.0f, 0.5f);
  }

  TEST_F(DemoPlayerTest, ShowsWhatFitzQuakeAddedToAnEntity)
  {
    AddSignon(DemoProtocol::fitzquake);
    // entity 300 with a baseline of the newer form: model 2, frame 258, alpha 200
    Block(
      DemoBytes().Byte(42).Short(300).Byte(2 | 4).Byte(2).Short(258).Byte(0).Byte(0).Coord(0.0f).Angle(0.0f)
                 .Coord(0.0f).Angle(0.0f).Coord(0.0f).Angle(0.0f).Byte(200));

    // an update with nothing: all of the baseline
    DemoBytes plain;
    plain.Byte(0x81).Byte(0x40).Short(300);
    // one with an alpha, the high bytes of frame and model, and a time for the next frame
    DemoBytes rich;
    rich.Byte(0x81).Byte(0xc0).Byte(0x0f).Short(300).Byte(100).Byte(3).Byte(0).Byte(51);

    Block(Time(1.0f).Add(Update(1, {})).Add(plain));
    Block(Time(1.1f).Add(Update(1, {})).Add(rich));
    Block(Time(1.2f).Add(Update(1, {})).Add(rich));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(Shown(300).state.model, 2);
    EXPECT_EQ(Shown(300).state.frame, 258);
    EXPECT_EQ(Shown(300).state.alpha, 200);
    EXPECT_EQ(Shown(300).frame_finish_time, 0.0f);
    EXPECT_EQ(Shown(1).state.alpha, 0);

    _player.Advance(0.15f);
    EXPECT_EQ(Shown(300).state.alpha, 100);
    // the low bytes are those of the baseline, the high ones of the update
    EXPECT_EQ(Shown(300).state.frame, 2 + 3 * 256);
    EXPECT_EQ(Shown(300).state.model, 2);
    EXPECT_NEAR(Shown(300).frame_finish_time, 1.3f, close);

    EXPECT_FLOAT_EQ(DemoEntityState::Opacity(0), 1.0f);
    EXPECT_FLOAT_EQ(DemoEntityState::Opacity(255), 1.0f);
    EXPECT_FLOAT_EQ(DemoEntityState::Opacity(1), 0.0f);
    EXPECT_FLOAT_EQ(DemoEntityState::Opacity(128), 0.5f);
  }

  TEST_F(DemoPlayerTest, ReadsTheUpdatesOfRmqWithItsCoordinatesAndAngles)
  {
    // a coordinate is a long and an angle a short, in the baseline too
    Block(ServerInfo(DemoProtocol::rmq, DemoProtocol::short_angle | DemoProtocol::int32_coord).Byte(25).Byte(1));
    DemoBytes baseline;
    baseline.Byte(22).Short(1).Byte(3).Byte(0).Byte(0).Byte(0);
    baseline.Long(0).Short(0).Long(0).Short(0).Long(0).Short(0);
    Block(baseline.Byte(25).Byte(2).Byte(25).Byte(3));

    DemoBytes update;
    update.Byte(0x80 | 0x02 | 0x04 | 0x08 | 0x10).Byte(1).Long(16 * 5000).Long(-16 * 6000).Short(16384).Long(8);
    Block(Time(1.0f).Add(update));
    Block(Time(1.1f).Add(update));
    Open();

    _player.Advance(0.0f);
    EXPECT_FALSE(_player.HasFailed()) << _player.GetProblem();
    EXPECT_EQ(Shown(1).state.origin, (LevelVector{5000.0f, -6000.0f, 0.5f}));
    EXPECT_EQ(Shown(1).state.angles, (LevelVector{0.0f, 90.0f, 0.0f}));
  }

  TEST_F(DemoPlayerTest, KeepsTheStaticEntitiesAndSounds)
  {
    AddSignon();
    DemoBytes statics;
    statics.Byte(20).Byte(2).Byte(1).Byte(0).Byte(3).Coord(8.0f).Angle(0.0f).Coord(16.0f).Angle(90.0f).Coord(24.0f)
           .Angle(0.0f);
    statics.Byte(29).Place({1.0f, 2.0f, 3.0f}).Byte(2).Byte(255).Byte(128);
    Block(statics);
    Block(Time(1.0f).Add(Update(1, {})));
    Open();

    _player.Advance(0.0f);

    ASSERT_EQ(_player.GetStaticEntities().size(), 1u);
    const DemoEntityState &torch = _player.GetStaticEntities()[0];
    EXPECT_EQ(torch.model, 2);
    EXPECT_EQ(torch.frame, 1);
    EXPECT_EQ(torch.skin, 3);
    EXPECT_EQ(torch.origin, (LevelVector{8.0f, 16.0f, 24.0f}));
    EXPECT_EQ(torch.angles, (LevelVector{0.0f, 90.0f, 0.0f}));

    ASSERT_EQ(_player.GetStaticSounds().size(), 1u);
    EXPECT_EQ(_player.GetStaticSounds()[0].sound, 2);
    EXPECT_EQ(_player.GetStaticSounds()[0].origin, (LevelVector{1.0f, 2.0f, 3.0f}));
    EXPECT_FLOAT_EQ(_player.GetStaticSounds()[0].volume, 1.0f);
    EXPECT_FLOAT_EQ(_player.GetStaticSounds()[0].attenuation, 2.0f);

    // a static entity is no entity with a number
    EXPECT_EQ(_player.GetEntities().size(), 1u);
  }

  // The view.

  TEST_F(DemoPlayerTest, PutsTheViewAnglesBetweenThoseOfTheLastTwoBlocks)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})), {10.0f, 350.0f, 0.0f});
    Block(Time(1.1f).Add(Update(1, {})), {20.0f, 10.0f, 4.0f});
    Block(Time(1.2f).Add(Update(1, {})), {20.0f, 10.0f, 4.0f});
    Block(Time(1.3f).Add(Update(1, {})), {20.0f, 10.0f, 4.0f});
    Open();

    _player.Advance(0.0f);
    _player.Advance(0.09f);

    // halfway, the short way around from 350 to 10
    _player.Advance(0.06f);
    EXPECT_NEAR(_player.GetViewAngles()[0], 15.0f, 0.01f);
    EXPECT_NEAR(_player.GetViewAngles()[1], 360.0f, 0.01f);
    EXPECT_NEAR(_player.GetViewAngles()[2], 2.0f, 0.01f);

    _player.Advance(0.1f);
    EXPECT_NEAR(_player.GetViewAngles()[0], 20.0f, 0.01f);
    EXPECT_NEAR(_player.GetViewAngles()[1], 10.0f, 0.01f);
    EXPECT_NEAR(_player.GetViewAngles()[2], 4.0f, 0.01f);
  }

  TEST_F(DemoPlayerTest, KeepsWhatTheServerSaysOfThePlayerForTheViewAndTheStatusBar)
  {
    AddSignon();
    DemoBytes packet = Time(30.0f);
    packet.Add(Update(1, {}));
    // the totals of the level, and what was found of them
    packet.Byte(3).Byte(11).Long(4).Byte(3).Byte(12).Long(20).Byte(3).Byte(13).Long(1).Byte(3).Byte(14).Long(7);
    packet.Byte(27).Byte(27).Byte(28);
    // a view height, a kick, a weapon with its frame, and an armour
    const std::uint32_t items =
      static_cast<std::uint32_t>(QcItem::Shotgun) | static_cast<std::uint32_t>(QcItem::RocketLauncher) |
      static_cast<std::uint32_t>(QcItem::Sigil4);
    packet.Byte(15).Short(0x7005).Char(18).Char(-2).Long(items);
    packet.Byte(4).Byte(150).Byte(3).Short(88).Byte(12).Byte(25).Byte(30).Byte(12).Byte(0).Byte(32);
    Block(packet);
    Block(Time(30.1f).Add(Update(1, {})));
    Open();

    _player.Advance(0.0f);
    _player.Advance(0.05f);

    const DemoClientData &data = _player.GetClientData();
    EXPECT_EQ(data.view_height, 18.0f);
    EXPECT_EQ(data.punch_angles, (LevelVector{-2.0f, 0.0f, 0.0f}));
    EXPECT_EQ(_player.GetWeaponModel(), 3);
    EXPECT_EQ(_player.GetWeaponFrame(), 4);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_health), 88);
    EXPECT_EQ(_player.GetStat(99), 0);

    const PlayerStats stats = _player.GetStats();
    EXPECT_EQ(stats.health, 88);
    EXPECT_EQ(stats.armor, 150);
    EXPECT_EQ(stats.ammo, 12);
    EXPECT_EQ(stats.shells, 25);
    EXPECT_EQ(stats.nails, 30);
    EXPECT_EQ(stats.rockets, 12);
    EXPECT_EQ(stats.cells, 0);
    EXPECT_EQ(stats.weapon, static_cast<std::uint32_t>(QcItem::RocketLauncher));
    EXPECT_EQ(stats.items, items);
    EXPECT_EQ(stats.total_secrets, 4);
    EXPECT_EQ(stats.total_monsters, 20);
    EXPECT_EQ(stats.found_secrets, 2);
    EXPECT_EQ(stats.killed_monsters, 9);
    EXPECT_EQ(stats.level_name, "The Level");
    EXPECT_EQ(stats.map_name, "test");
    EXPECT_NEAR(stats.time, 29.95f, close);
  }

  TEST_F(DemoPlayerTest, KeepsTheLightStylesTheScoreboardAndThePause)
  {
    AddSignon();
    DemoBytes packet = Time(1.0f);
    packet.Add(Update(1, {}));
    packet.Byte(12).Byte(0).Text("m").Byte(12).Byte(255).Text("az");
    packet.Byte(13).Byte(1).Text("ranger").Byte(14).Byte(1).Short(-3).Byte(17).Byte(1).Byte(0x4d);
    packet.Byte(24).Byte(1);
    // a number of the status bar that there is not is skipped
    packet.Byte(3).Byte(200).Long(5);
    Block(packet);
    Block(Time(1.1f).Add(Update(1, {})).Byte(24).Byte(0));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(_player.GetLightStyle(0), "m");
    EXPECT_EQ(_player.GetLightStyle(1), "");
    EXPECT_EQ(_player.GetLightStyle(255), "az");
    EXPECT_EQ(_player.GetLightStyle(256), "");
    ASSERT_EQ(_player.GetScores().size(), 2u);
    EXPECT_EQ(_player.GetScores()[0].name, "");
    EXPECT_EQ(_player.GetScores()[1].name, "ranger");
    EXPECT_EQ(_player.GetScores()[1].frags, -3);
    EXPECT_EQ(_player.GetScores()[1].colors, 0x4d);
    EXPECT_TRUE(_player.IsPaused());

    _player.Advance(0.15f);
    EXPECT_FALSE(_player.IsPaused());
    EXPECT_FALSE(_player.HasFailed()) << _player.GetProblem();
  }

  // What the listener is told.

  TEST_F(DemoPlayerTest, TellsTheListenerOfWhatHappensAtAMomentInItsOrder)
  {
    AddSignon(DemoProtocol::fitzquake);
    DemoBytes packet = Time(1.0f);
    packet.Add(Update(1, {}));
    packet.Byte(6).Byte(3).Byte(255).Byte(32).Short(2 << 3 | 4).Byte(1).Place({1.0f, 2.0f, 3.0f});
    packet.Byte(16).Short(2 << 3 | 4);
    packet.Byte(23).Byte(3).Place({4.0f, 5.0f, 6.0f});
    packet.Byte(23).Byte(12).Place({4.0f, 5.0f, 6.0f}).Byte(9).Byte(8);
    packet.Byte(23).Byte(6).Short(1).Place({1.0f, 1.0f, 1.0f}).Place({2.0f, 2.0f, 2.0f});
    packet.Byte(18).Place({0.0f, 0.0f, 8.0f}).Char(16).Char(0).Char(0).Byte(10).Byte(73);
    packet.Byte(8).Text("a line").Byte(26).Text("the middle").Byte(9).Text("bf\n");
    packet.Byte(19).Byte(5).Byte(6).Place({7.0f, 8.0f, 9.0f});
    packet.Byte(32).Byte(4).Byte(5);
    packet.Byte(41).Byte(255).Byte(0).Byte(0).Byte(255).Short(100);
    packet.Byte(37).Text("night").Byte(40).Byte(33);
    Block(packet);
    Block(Time(1.1f).Add(Update(1, {})));
    Open();

    _player.Advance(0.0f);

    EXPECT_THAT(
      _listener.events,
      ElementsAre(
        "level: The Level", "sound: 2 4 1 1 0.5 at 1 2 3", "stop: 2 4", "point: 3 at 4 5 6",
        "explosion: 9 8 at 4 5 6", "beam: 6 of 1 from 1 1 1 to 2 2 2", "particles: 10 of 73 at 0 0 8 to 1 0 0",
        "print: a line", "center: the middle", "command: bf\n", "damage: 5 6 from 7 8 9", "music: 4 5",
        "fog: 1 0 0 1 over 1", "sky: night", "flash", "sell"));

    // nothing is told twice
    _player.Advance(0.05f);
    EXPECT_EQ(_listener.events.size(), 16u);
  }

  TEST_F(DemoPlayerTest, PlaysTheTrackTheRecordingForcesInPlaceOfTheOneOfTheServer)
  {
    _file = {};
    _file.Line("9");
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})).Byte(32).Byte(4).Byte(5));
    Open();

    _player.Advance(0.0f);
    EXPECT_THAT(_listener.events, ElementsAre("level: The Level", "music: 9 5"));
  }

  TEST_F(DemoPlayerTest, RemembersTheScreenOverTheLevelAndWhenItCame)
  {
    AddSignon();
    Block(Time(7.0f).Add(Update(1, {})));
    Block(Time(7.1f).Add(Update(1, {})).Byte(30));
    Block(Time(7.2f).Add(Update(1, {})).Byte(31).Text("the end"));
    Block(Time(7.3f).Add(Update(1, {})).Byte(34).Text("a camera"));
    Block(Time(7.4f).Add(Update(1, {})));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::None);

    _player.Advance(0.15f);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::Tally);
    EXPECT_NEAR(_player.GetCompletedTime(), 7.05f, close);

    _player.Advance(0.1f);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::Finale);
    EXPECT_NEAR(_player.GetCompletedTime(), 7.15f, close);

    _player.Advance(0.1f);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::Cutscene);
    EXPECT_THAT(
      _listener.events, ElementsAre("level: The Level", "intermission", "finale: the end", "cutscene: a camera"));
  }

  // The end.

  TEST_F(DemoPlayerTest, IsOverWhenTheServerGoes)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})));
    Block(Time(1.1f).Add(Update(1, {})).Byte(2).Byte(27));
    Block(Time(1.2f).Add(Update(1, {})).Byte(27));
    Open();

    _player.Advance(0.0f);
    EXPECT_FALSE(_player.IsOver());
    _player.Advance(0.15f);
    EXPECT_TRUE(_player.IsOver());
    EXPECT_FALSE(_player.HasFailed());

    // nothing after it is read
    _player.Advance(1.0f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 0);
  }

  TEST_F(DemoPlayerTest, FailsWithATextForAMessageItCannotRead)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})).Byte(27));
    Block(Time(1.1f).Add(Update(1, {})).Byte(27).Byte(99).Byte(27));
    Block(Time(1.2f).Add(Update(1, {})).Byte(27));
    Open();

    _player.Advance(0.0f);
    EXPECT_FALSE(_player.HasFailed());
    EXPECT_EQ(_player.GetProblem(), "");

    _player.Advance(0.15f);
    EXPECT_TRUE(_player.IsOver());
    EXPECT_TRUE(_player.HasFailed());
    EXPECT_THAT(_player.GetProblem(), HasSubstr("Block 4"));
    EXPECT_THAT(_player.GetProblem(), HasSubstr("Message 99 is not known"));

    // what came before it counts, nothing after it does
    _player.Advance(1.0f);
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 2);
    EXPECT_EQ(_player.GetEntities().size(), 1u);
  }

  TEST_F(DemoPlayerTest, FailsForALevelOfAProtocolItDoesNotKnow)
  {
    Block(DemoBytes().Byte(11).Long(3).Byte(1).Byte(0).Text("old").Text("").Text(""));
    Open();

    _player.Advance(0.1f);
    EXPECT_TRUE(_player.HasFailed());
    EXPECT_THAT(_player.GetProblem(), HasSubstr("protocol 3"));
    EXPECT_FALSE(_player.HasLevel());
    EXPECT_THAT(_listener.events, IsEmpty());
  }

  TEST_F(DemoPlayerTest, IsOverAtOnceForARecordingWithoutABlock)
  {
    Open();
    _player.Advance(0.1f);
    EXPECT_TRUE(_player.IsOver());
    EXPECT_FALSE(_player.HasFailed());
    EXPECT_FALSE(_player.HasLevel());
  }

  // More than one level, and more than one recording.

  TEST_F(DemoPlayerTest, ForgetsALevelWhenTheNextOneBegins)
  {
    AddSignon();
    DemoBytes packet = Time(50.0f);
    packet.Add(Update(1, {})).Add(Update(2, {}));
    packet.Byte(27).Byte(30).Byte(24).Byte(1).Byte(12).Byte(5).Text("abc");
    packet.Byte(20).Byte(2).Byte(0).Byte(0).Byte(0).Coord(0.0f).Angle(0.0f).Coord(0.0f).Angle(0.0f).Coord(0.0f)
          .Angle(0.0f);
    Block(packet);
    Block(Time(50.1f).Add(Update(1, {})).Add(Update(2, {})));

    // the next level, of another protocol, with other names
    DemoBytes next;
    next.Byte(11).Long(666).Byte(1).Byte(0).Text("The Next").Text("maps/next.bsp").Text("");
    next.Text("ambience/wind.wav").Text("").Byte(25).Byte(1);
    Block(next);
    Block(DemoBytes().Byte(25).Byte(2));
    Block(DemoBytes().Byte(25).Byte(3));
    Block(Time(3.0f).Byte(0x80).Byte(1));
    Open();

    _player.Advance(0.0f);
    EXPECT_EQ(_player.GetEntities().size(), 2u);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::Tally);

    // past the last packet of the first level: all of the second is read
    _player.Advance(0.25f);
    EXPECT_THAT(_listener.events, ElementsAre("level: The Level", "intermission", "level: The Next"));
    EXPECT_EQ(_player.GetLevelName(), "The Next");
    EXPECT_EQ(_player.GetMapModelName(), "maps/next.bsp");
    EXPECT_EQ(_player.GetServerInfo().protocol.version, 666);
    EXPECT_THAT(_player.GetModelNames(), ElementsAre("", "maps/next.bsp"));
    EXPECT_TRUE(_player.IsSignedOn());
    EXPECT_NEAR(_player.GetTime(), 2.9f, close);

    // nothing of the first is left: entity 1 has no baseline, so no model
    EXPECT_THAT(_player.GetEntities(), IsEmpty());
    EXPECT_THAT(_player.GetStaticEntities(), IsEmpty());
    EXPECT_EQ(_player.GetStat(DemoPlayer::stat_monsters), 0);
    EXPECT_EQ(_player.GetIntermission(), DemoIntermission::None);
    EXPECT_FALSE(_player.IsPaused());
    EXPECT_EQ(_player.GetLightStyle(5), "");
    EXPECT_EQ(_player.GetScores().size(), 1u);
    EXPECT_EQ(_player.GetViewEntity(), 0);
  }

  TEST_F(DemoPlayerTest, PlaysAnotherRecordingInPlaceOfTheOneBefore)
  {
    AddSignon();
    Block(Time(1.0f).Add(Update(1, {})).Byte(2));
    Open();
    _player.Advance(0.0f);
    EXPECT_TRUE(_player.IsOver());

    Open();
    EXPECT_FALSE(_player.IsOver());
    EXPECT_FALSE(_player.HasLevel());
    EXPECT_EQ(_player.GetTime(), 0.0f);
    EXPECT_THAT(_player.GetEntities(), IsEmpty());

    _player.Advance(0.0f);
    EXPECT_EQ(_listener.levels.size(), 2u);
    EXPECT_TRUE(_player.IsOver());

    _player.Close();
    EXPECT_FALSE(_player.IsOpen());
    EXPECT_FALSE(_player.IsOver());
    EXPECT_FALSE(_player.HasLevel());
    EXPECT_THAT(_player.GetModelNames(), IsEmpty());
  }
}
