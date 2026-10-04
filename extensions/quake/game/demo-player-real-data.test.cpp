#include "demo-player.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <set>
#include <span>
#include <string>

#include <gtest/gtest.h>

#include "demo-recording-listener.test.hpp"
#include "formats/real-data.test.hpp"

// The tests of DemoPlayer with the recordings of a real game: the three
// its title screen plays, each played from its start to its end.
namespace
{
  using quake::DemoEntity;
  using quake::DemoIntermission;
  using quake::DemoPlayer;
  using quake::DemoProtocol;
  using quake::DemoRecordingListener;
  using quake::PlayerStats;
  using quake::RealData;

  /// What was seen of a recording while it played.
  struct Played
  {
    /// How many times the clock was moved on.
    std::size_t frames = 0;

    /// The time of the clock at the first frame and at the end.
    float first_time = 0.0f;
    float last_time = 0.0f;

    /// The most entities shown at once, and the frames the entity the
    /// player sees through was not among them.
    std::size_t most_entities = 0;
    std::size_t frames_without_view = 0;

    /// What the entities had at any time.
    std::set<std::int32_t> effects;
    std::set<std::int32_t> alphas;
    std::set<std::string> models;
  };

  class DemoPlayerRealDataTest : public ::testing::Test
  {
  protected:
    /// The clock moves on in steps of this many seconds.
    static constexpr float frame_seconds = 0.1f;

    /// No recording of a title screen is longer than this many frames.
    static constexpr std::size_t most_frames = 20000;

    DemoRecordingListener _listener;
    DemoPlayer _player{_listener};

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }
      ASSERT_TRUE(data.GetProblem().empty()) << data.GetProblem();
    }

    /// Plays a recording of the paks to its end, checking with every
    /// frame that what is shown makes sense.
    Played Play(const std::string &name)
    {
      Played played;
      const std::span<const std::uint8_t> bytes = RealData::Get().GetBytes(name);
      EXPECT_FALSE(bytes.empty()) << name;

      std::string problem;
      EXPECT_TRUE(_player.Open(bytes, problem)) << problem;
      EXPECT_FALSE(_player.HasLevel());

      while (!_player.IsOver() && played.frames < most_frames)
      {
        const float time_before = _player.GetTime();
        _player.Advance(frame_seconds);
        if (played.frames == 0) { played.first_time = _player.GetTime(); }
        else if (!_player.IsOver()) { EXPECT_GE(_player.GetTime(), time_before) << "frame " << played.frames; }
        played.frames++;

        const std::span<const DemoEntity> entities = _player.GetEntities();
        played.most_entities = std::max(played.most_entities, entities.size());
        if (_player.FindEntity(_player.GetViewEntity()) == nullptr) { played.frames_without_view++; }

        std::int32_t number_before = 0;
        for (const DemoEntity &entity : entities)
        {
          // in the order of their numbers, each with a model the level names
          EXPECT_GT(entity.number, number_before);
          number_before = entity.number;
          EXPECT_FALSE(_player.GetModelName(entity.state.model).empty())
            << "entity " << entity.number << " has model " << entity.state.model;

          played.effects.insert(entity.state.effects);
          played.alphas.insert(entity.state.alpha);
          played.models.insert(std::string(_player.GetModelName(entity.state.model)));
        }
      }
      played.last_time = _player.GetTime();

      std::cout << std::format(
        "{}: protocol {} flags {:#x}, level \"{}\" in {}, {} models, {} sounds, {} frames, clock {} to {}, at most "
        "{} entities, {} static entities, {} static sounds\n",
        name, _player.GetServerInfo().protocol.version, _player.GetServerInfo().protocol.flags,
        _player.GetLevelName(), _player.GetMapModelName(), _player.GetModelNames().size(),
        _player.GetSoundNames().size(), played.frames, played.first_time, played.last_time, played.most_entities,
        _player.GetStaticEntities().size(), _player.GetStaticSounds().size());
      std::cout << "  effects:";
      for (const std::int32_t effects : played.effects) { std::cout << ' ' << effects; }
      std::cout << "\n  alphas:";
      for (const std::int32_t alpha : played.alphas) { std::cout << ' ' << alpha; }
      std::cout << "\n  models shown:";
      for (const std::string &model : played.models) { std::cout << ' ' << model; }
      std::cout << "\n  told:";
      for (const auto &[what, count] : _listener.counts) { std::cout << ' ' << what << ' ' << count; }
      std::cout << '\n';
      return played;
    }

    /// What every recording of the title screen has to be after Play().
    void ExpectPlayedToTheEnd(const Played &played)
    {
      EXPECT_TRUE(_player.IsOver());
      EXPECT_FALSE(_player.HasFailed()) << _player.GetProblem();
      EXPECT_LT(played.frames, most_frames);

      // one level, with what it is made of
      ASSERT_EQ(_listener.levels.size(), 1u);
      EXPECT_TRUE(_player.HasLevel());
      EXPECT_TRUE(_player.IsSignedOn());
      EXPECT_GT(_player.GetModelNames().size(), 2u);
      EXPECT_GT(_player.GetSoundNames().size(), 2u);
      EXPECT_TRUE(_player.GetModelNames()[0].empty());
      EXPECT_EQ(_player.GetLightStyle(0), "m");
      EXPECT_FALSE(_player.GetStaticEntities().empty());
      EXPECT_FALSE(_player.GetStaticSounds().empty());
      EXPECT_EQ(_listener.CountOf("music"), 1u);

      // the clock starts where the server was, and goes on for half a minute
      EXPECT_GT(played.first_time, 0.0f);
      EXPECT_GT(played.last_time, played.first_time + 25.0f);
      EXPECT_LT(played.last_time, played.first_time + 60.0f);

      // what was shown: the player is seen through in every frame
      EXPECT_GE(played.most_entities, 3u);
      EXPECT_LT(played.most_entities, 600u);
      EXPECT_EQ(_player.GetViewEntity(), 1);
      EXPECT_EQ(played.frames_without_view, 0u);

      // each is a flight of a camera, which the game code starts as a
      // cutscene without a text, with the eyes at the origin of the player
      EXPECT_EQ(_player.GetIntermission(), DemoIntermission::Cutscene);
      EXPECT_EQ(_listener.CountOf("cutscene"), 1u);
      EXPECT_EQ(_player.GetClientData().view_height, 0.0f);
      EXPECT_FALSE(_player.IsPaused());

      // the status bar has a player that is alive
      const PlayerStats stats = _player.GetStats();
      EXPECT_GT(stats.health, 0);
      EXPECT_LE(stats.killed_monsters, stats.total_monsters);
      EXPECT_EQ(stats.level_name, _player.GetLevelName());
      EXPECT_EQ("maps/" + stats.map_name + ".bsp", _player.GetMapModelName());
      ASSERT_EQ(_player.GetScores().size(), 1u);
      EXPECT_EQ(_player.GetScores()[0].name, "player");
    }
  };

  TEST_F(DemoPlayerRealDataTest, PlaysTheFirstRecordingToItsEnd)
  {
    const Played played = Play("demo1.dem");
    ExpectPlayedToTheEnd(played);

    EXPECT_EQ(_player.GetServerInfo().protocol.version, DemoProtocol::netquake);
    EXPECT_EQ(_player.GetLevelName(), "Dismal Shores");
    EXPECT_EQ(_player.GetMapModelName(), "maps/lq_e1m4.bsp");
    EXPECT_GT(played.most_entities, 50u);
    EXPECT_GT(_player.GetStats().total_monsters, 0);
    EXPECT_GT(_listener.CountOf("sound"), 0u);

    const DemoEntity *view = _player.FindEntity(_player.GetViewEntity());
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(_player.GetModelName(view->state.model), "progs/player.mdl");
    EXPECT_TRUE(played.models.contains("progs/ogre.mdl"));
  }

  TEST_F(DemoPlayerRealDataTest, PlaysTheSecondRecordingToItsEnd)
  {
    const Played played = Play("demo2.dem");
    ExpectPlayedToTheEnd(played);

    EXPECT_EQ(_player.GetServerInfo().protocol.version, DemoProtocol::netquake);
    EXPECT_EQ(_player.GetLevelName(), "Meeting of The Parasites");
    EXPECT_EQ(_player.GetMapModelName(), "maps/lq_e2m5.bsp");
    EXPECT_GT(_player.GetStats().total_monsters, 0);
    EXPECT_GT(_listener.CountOf("point"), 0u);

    // the level has more models than the protocol of the original can
    // name: the server cut the list off after 255, before it came to the
    // model of the player, and sent of every index only its low byte. So
    // the recording itself has the wrong models for what is no part of
    // the level, and the original shows them wrong too.
    EXPECT_EQ(_player.GetModelNames().size(), 256u);
    EXPECT_FALSE(played.models.contains("progs/player.mdl"));
  }

  TEST_F(DemoPlayerRealDataTest, PlaysTheThirdRecordingWhichIsOfAnotherProtocolToItsEnd)
  {
    const Played played = Play("demo3.dem");
    ExpectPlayedToTheEnd(played);

    // coordinates as longs and angles as shorts
    EXPECT_EQ(_player.GetServerInfo().protocol.version, DemoProtocol::rmq);
    EXPECT_EQ(
      _player.GetServerInfo().protocol.flags, DemoProtocol::short_angle | DemoProtocol::int32_coord);
    EXPECT_EQ(_player.GetLevelName(), "Hyperborea");
    EXPECT_EQ(_player.GetMapModelName(), "maps/lqdm3.bsp");

    const DemoEntity *view = _player.FindEntity(_player.GetViewEntity());
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(_player.GetModelName(view->state.model), "progs/player.mdl");
  }
}
