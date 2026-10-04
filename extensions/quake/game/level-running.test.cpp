#include "level-running.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "level-program.test.hpp"
#include "qc-move-type.hpp"

namespace
{
  using quake::ClientThink;
  using quake::LevelMover;
  using quake::LevelProgram;
  using quake::LevelRunning;
  using quake::QcFields;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcMoveType;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  /// A mover that notes what it is asked, and lets what pushes move or not.
  class NotingMover final : public LevelMover
  {
  public:
    bool lets_pushers_move = true;
    std::vector<std::array<float, 3>> pusher_places;
    std::vector<float> pusher_times;
    std::vector<std::int32_t> moved;

    bool MovePusher(
      const std::int32_t entity,
      const std::array<float, 3> &origin,
      const std::array<float, 3> &angles,
      const float dt) override
    {
      pusher_places.push_back(origin);
      pusher_times.push_back(dt);
      return lets_pushers_move;
    }

    void MoveEntity(const std::int32_t entity, const float dt) override
    {
      moved.push_back(entity);
    }
  };

  /// A level that runs on a small program. Its function `think` notes for
  /// which entity and at what time it was called; the other functions do
  /// what a test gives them to do.
  class LevelRunningTest : public ::testing::Test
  {
  protected:
    /// A step that floats hold exactly, so that the tests compare times
    /// with `==`.
    static constexpr float step = 0.25f;

    LevelProgram _program;
    std::int32_t _start_frame = _program.EngineFunction("StartFrame");
    std::int32_t _think = _program.Function("think");
    std::int32_t _special = _program.Function("special");
    std::int32_t _broken = _program.Function("broken");

    QcMachine _machine = _program.Make();
    QcGlobals _globals{_machine.GetProgs()};
    QcFields _fields{_machine.GetProgs()};

    /// Who thought, and the global `time` each saw.
    std::vector<std::pair<std::int32_t, float>> _thoughts;
    int _frames_started = 0;

    void SetUp() override
    {
      _machine.SetBuiltin(_start_frame, [this](QcMachine &) { _frames_started++; });
      _machine.SetBuiltin(_think, [this](QcMachine &machine)
      {
        _thoughts.emplace_back(_globals.self.Get(machine), _globals.time.Get(machine));
      });
      _machine.SetBuiltin(_special, [](QcMachine &) {});
      _machine.SetBuiltin(_broken, [](QcMachine &machine) { machine.Stop("it is broken"); });
    }

    [[nodiscard]] std::int32_t FunctionOf(const std::string_view name) const
    {
      return _machine.GetProgs().FindFunction(name).value_or(0);
    }

    /// Makes an entity that thinks with a function at a time.
    std::int32_t MakeThinker(const float next_think, const std::string_view think = "think")
    {
      const std::int32_t entity = _machine.CreateEntity().value_or(0);
      _fields.classname.SetText(_machine, entity, "thinker");
      _fields.nextthink.Set(_machine, entity, next_think);
      _fields.think.Set(_machine, entity, FunctionOf(think));
      return entity;
    }

    /// The same for one that pushes, which moves up with a speed.
    std::int32_t MakePusher(const float next_think, const float speed)
    {
      const std::int32_t entity = MakeThinker(next_think);
      _fields.movetype.Set(_machine, entity, static_cast<float>(QcMoveType::Push));
      _fields.velocity.Set(_machine, entity, {0.0f, 0.0f, speed});
      return entity;
    }
  };

  TEST_F(LevelRunningTest, StartsAFrameForTheWorldAndMovesTheTimeOn)
  {
    LevelRunning running(_machine);
    float time_seen = 0.0f;
    float frame_time_seen = 0.0f;
    _globals.self.Set(_machine, 5);
    _globals.other.Set(_machine, 5);
    _machine.SetBuiltin(_start_frame, [&](QcMachine &machine)
    {
      time_seen = _globals.time.Get(machine);
      frame_time_seen = _globals.frametime.Get(machine);
      EXPECT_EQ(_globals.self.Get(machine), 0);
      EXPECT_EQ(_globals.other.Get(machine), 0);
    });

    EXPECT_EQ(running.GetTime(), 1.0);
    running.Advance(step);
    EXPECT_EQ(time_seen, 1.0f);
    EXPECT_EQ(frame_time_seen, step);
    EXPECT_EQ(running.GetTime(), 1.25);
    EXPECT_EQ(_globals.time.Get(_machine), 1.25f);

    running.Advance(step);
    EXPECT_EQ(time_seen, 1.25f);
    EXPECT_THAT(running.GetFailures(), IsEmpty());
  }

  TEST_F(LevelRunningTest, RunsWithoutTheFunctionsAProgramLacks)
  {
    LevelProgram bare;
    QcMachine machine = bare.Make();
    LevelRunning running(machine);

    running.Advance(step);
    EXPECT_FALSE(running.ConnectClient(0));
    EXPECT_FALSE(running.RunClientThink(0, ClientThink::Before));
    EXPECT_EQ(running.GetFailureCount(), 0u);
    EXPECT_EQ(running.GetTime(), 1.25);
  }

  TEST_F(LevelRunningTest, HasAnEntityThinkWhenItsTimeHasComeAndAtTheTimeItAskedFor)
  {
    LevelRunning running(_machine);
    const std::int32_t soon = MakeThinker(1.125f);
    const std::int32_t later = MakeThinker(1.5f);
    const std::int32_t never = MakeThinker(0.0f);

    // the first frame reaches from 1 to 1.25
    running.Advance(step);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(soon, 1.125f)));
    EXPECT_EQ(_fields.nextthink.Get(_machine, soon), 0.0f);
    EXPECT_EQ(_fields.nextthink.Get(_machine, later), 1.5f);

    // the second to 1.5, and a time at the end of a frame is inside it
    running.Advance(step);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(soon, 1.125f), std::pair(later, 1.5f)));

    // who thought does not think again without asking to
    running.Advance(step);
    EXPECT_EQ(_thoughts.size(), 2u);
    EXPECT_EQ(_fields.nextthink.Get(_machine, never), 0.0f);
  }

  TEST_F(LevelRunningTest, TakesATimeToThinkThatIsPastAsNow)
  {
    LevelRunning running(_machine);
    const std::int32_t late = MakeThinker(0.5f);

    running.Advance(step);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(late, 1.0f)));
  }

  TEST_F(LevelRunningTest, ForgetsTheTimeOfAnEntityThatHasNoFunctionToThinkWith)
  {
    LevelRunning running(_machine);
    const std::int32_t entity = MakeThinker(1.125f);
    _fields.think.Set(_machine, entity, 0);

    running.Advance(step);
    EXPECT_EQ(_fields.nextthink.Get(_machine, entity), 0.0f);
    EXPECT_EQ(running.GetFailureCount(), 0u);
  }

  TEST_F(LevelRunningTest, HasAnEntityThinkAgainInTheNextFrameWhenItAsksTo)
  {
    LevelRunning running(_machine);
    const std::int32_t entity = MakeThinker(1.125f, "special");
    int thoughts = 0;
    _machine.SetBuiltin(_special, [&](QcMachine &machine)
    {
      thoughts++;
      _fields.nextthink.Set(machine, entity, _globals.time.Get(machine) + 0.125f);
    });

    // it asked for 1.25, which the frame it thought in reached; the entity
    // has had its turn in that frame all the same
    running.Advance(step);
    EXPECT_EQ(thoughts, 1);
    running.Advance(step);
    EXPECT_EQ(thoughts, 2);
  }

  TEST_F(LevelRunningTest, MovesWhatPushesByItsOwnClockAndHasItThinkByThatClock)
  {
    LevelRunning running(_machine);
    // it thinks when its own clock, which starts at 0, shows a half
    const std::int32_t door = MakePusher(0.5f, 100.0f);
    _fields.avelocity.Set(_machine, door, {0.0f, 40.0f, 0.0f});

    running.Advance(step);
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 25.0f));
    EXPECT_THAT(_fields.angles.Get(_machine, door), ElementsAre(0.0f, 10.0f, 0.0f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.25f);
    EXPECT_THAT(_thoughts, IsEmpty());

    // the time of the level, not the clock of the door, is what it sees
    running.Advance(step);
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 50.0f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.5f);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(door, 1.25f)));
    EXPECT_EQ(_fields.nextthink.Get(_machine, door), 0.0f);
  }

  TEST_F(LevelRunningTest, MovesWhatPushesNoFurtherThanToTheTimeItThinksAt)
  {
    LevelRunning running(_machine);
    const std::int32_t door = MakePusher(0.375f, 100.0f);
    // a door stops when it thinks
    _fields.think.Set(_machine, door, FunctionOf("special"));
    _machine.SetBuiltin(_special, [&](QcMachine &machine) { _fields.velocity.Set(machine, door, {}); });

    running.Advance(step);
    running.Advance(step);
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 37.5f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.375f);

    // With no time to think at, it does not move, whatever its velocity,
    // and its clock stands: the clock is how long it has been on its way.
    _fields.velocity.Set(_machine, door, {0.0f, 0.0f, 100.0f});
    running.Advance(step);
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 37.5f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.375f);
  }

  TEST_F(LevelRunningTest, AsksTheMoverBeforeWhatPushesMovesAndLeavesItWhenTheMoverSaysNo)
  {
    NotingMover mover;
    LevelRunning running(_machine, &mover);
    const std::int32_t door = MakePusher(0.25f, 100.0f);

    mover.lets_pushers_move = false;
    running.Advance(step);
    ASSERT_EQ(mover.pusher_places.size(), 1u);
    EXPECT_THAT(mover.pusher_places[0], ElementsAre(0.0f, 0.0f, 25.0f));
    EXPECT_THAT(mover.pusher_times, ElementsAre(step));

    // blocked: it stays, its clock stays, and it has not thought
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.0f);
    EXPECT_THAT(_thoughts, IsEmpty());
    EXPECT_EQ(_fields.nextthink.Get(_machine, door), 0.25f);

    mover.lets_pushers_move = true;
    running.Advance(step);
    EXPECT_THAT(_fields.origin.Get(_machine, door), ElementsAre(0.0f, 0.0f, 25.0f));
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.25f);
    EXPECT_EQ(_thoughts.size(), 1u);

    // what pushes is never handed over as what the mover moves itself: only
    // the world was, which has no movetype in this test
    EXPECT_THAT(mover.moved, ElementsAre(0, 0));
  }

  TEST_F(LevelRunningTest, DoesNotAskTheMoverForWhatPushesAndStandsStill)
  {
    NotingMover mover;
    LevelRunning running(_machine, &mover);
    // it waits, as an open door does before it closes
    const std::int32_t door = MakePusher(5.0f, 0.0f);

    running.Advance(step);
    EXPECT_THAT(mover.pusher_places, IsEmpty());
    EXPECT_EQ(_fields.ltime.Get(_machine, door), 0.25f);
  }

  TEST_F(LevelRunningTest, HandsEveryOtherEntityToTheMoverAfterItThought)
  {
    NotingMover mover;
    LevelRunning running(_machine, &mover);
    const std::int32_t falling = MakeThinker(1.125f, "special");
    _fields.movetype.Set(_machine, falling, static_cast<float>(QcMoveType::Toss));
    _machine.SetBuiltin(_special, [&](QcMachine &) { EXPECT_THAT(mover.moved, ElementsAre(0)); });

    running.Advance(step);
    // the world has no movetype in this test, and is handed over too
    EXPECT_THAT(mover.moved, ElementsAre(0, falling));
  }

  TEST_F(LevelRunningTest, VisitsAnEntityMadeDuringTheFrameInTheSameFrame)
  {
    NotingMover mover;
    LevelRunning running(_machine, &mover);
    const std::int32_t parent = MakeThinker(1.125f, "special");
    std::int32_t child = 0;
    _machine.SetBuiltin(_special, [&](QcMachine &machine)
    {
      child = machine.CreateEntity().value_or(0);
      _fields.nextthink.Set(machine, child, 1.25f);
      _fields.think.Set(machine, child, FunctionOf("think"));
    });

    running.Advance(step);
    ASSERT_EQ(child, 2);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(child, 1.25f)));
    EXPECT_THAT(mover.moved, ElementsAre(0, parent, child));
  }

  TEST_F(LevelRunningTest, GoesOnAfterAThinkThatRemovesItsEntityAndSkipsWhatIsFree)
  {
    NotingMover mover;
    LevelRunning running(_machine, &mover);
    const std::int32_t removed = MakeThinker(1.125f, "special");
    const std::int32_t freed_before = MakeThinker(1.125f);
    const std::int32_t last = MakeThinker(1.125f);
    _machine.SetBuiltin(_special, [&](QcMachine &machine) { machine.FreeEntity(removed); });
    _machine.FreeEntity(freed_before);

    running.Advance(step);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(last, 1.125f)));
    // what removed itself is not moved, and what is free was not looked at
    EXPECT_THAT(mover.moved, ElementsAre(0, last));
    EXPECT_EQ(_fields.nextthink.Get(_machine, freed_before), 1.125f);
    EXPECT_EQ(running.GetFailureCount(), 0u);
  }

  TEST_F(LevelRunningTest, KeepsAThinkThatIsStoppedAndGoesOnWithTheFrame)
  {
    LevelRunning running(_machine);
    const std::int32_t broken = MakeThinker(1.125f, "broken");
    const std::int32_t next = MakeThinker(1.125f);

    running.Advance(step);
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(next, 1.125f)));
    EXPECT_EQ(running.GetTime(), 1.25);

    ASSERT_EQ(running.GetFailures().size(), 1u);
    EXPECT_EQ(running.GetFailureCount(), 1u);
    EXPECT_EQ(running.GetFailures()[0].entity, broken);
    EXPECT_EQ(running.GetFailures()[0].classname, "thinker");
    EXPECT_EQ(running.GetFailures()[0].function, "broken");
    EXPECT_THAT(running.GetFailures()[0].error.message, HasSubstr("it is broken"));

    // the frame after starts as any other
    running.Advance(step);
    EXPECT_EQ(_frames_started, 2);
    EXPECT_EQ(running.GetFailureCount(), 1u);

    running.ClearFailures();
    EXPECT_THAT(running.GetFailures(), IsEmpty());
    EXPECT_EQ(running.GetFailureCount(), 0u);
  }

  TEST_F(LevelRunningTest, KeepsTheFirstFailuresAndCountsTheRest)
  {
    LevelRunning running(_machine);
    const std::int32_t entity = MakeThinker(1.0f, "broken");

    const std::size_t frames = LevelRunning::max_failures_kept + 10;
    for (std::size_t frame = 0; frame < frames; frame++)
    {
      _fields.nextthink.Set(_machine, entity, 1.0f);
      running.Advance(step);
    }
    EXPECT_EQ(running.GetFailureCount(), frames);
    EXPECT_EQ(running.GetFailures().size(), LevelRunning::max_failures_kept);
  }

  TEST_F(LevelRunningTest, CountsDownTheFramesEverythingIsToTouchAnewIn)
  {
    LevelRunning running(_machine);
    _globals.force_retouch.Set(_machine, 2.0f);

    running.Advance(step);
    EXPECT_EQ(_globals.force_retouch.Get(_machine), 1.0f);
    running.Advance(step);
    running.Advance(step);
    EXPECT_EQ(_globals.force_retouch.Get(_machine), 0.0f);
  }

  TEST_F(LevelRunningTest, RunsAFunctionForAHostWithTheOtherEntityAndKeepsItsFailure)
  {
    LevelRunning running(_machine);
    const std::int32_t trigger = MakeThinker(0.0f);
    const std::int32_t toucher = MakeThinker(0.0f);
    running.SetTime(4.0);
    std::int32_t other_seen = -1;
    _machine.SetBuiltin(_special, [&](QcMachine &machine)
    {
      other_seen = _globals.other.Get(machine);
      EXPECT_EQ(_globals.self.Get(machine), trigger);
      EXPECT_EQ(_globals.time.Get(machine), 4.0f);
    });

    EXPECT_TRUE(running.RunFunction(FunctionOf("special"), trigger, toucher));
    EXPECT_EQ(other_seen, toucher);

    // no function is nothing to run, and no failure
    EXPECT_TRUE(running.RunFunction(0, trigger, toucher));
    EXPECT_FALSE(running.RunFunction(FunctionOf("broken"), trigger, toucher));
    EXPECT_EQ(running.GetFailureCount(), 1u);
  }

  /// The same with the functions the game code has for a player.
  class LevelRunningClientTest : public LevelRunningTest
  {
  protected:
    /// The functions that were called, with the entity each was called for.
    std::vector<std::pair<std::string, std::int32_t>> _calls;

    void Add(const std::string &name)
    {
      const std::int32_t builtin = _program.EngineFunction(name);
      _noting.emplace_back(builtin, name);
    }

    std::vector<std::pair<std::int32_t, std::string>> _noting;

    /// Makes the machine anew with the functions of a player in it.
    void SetUp() override
    {
      for (const char *name : {
             "SetNewParms", "ClientConnect", "PutClientInServer", "PlayerPreThink", "PlayerPostThink",
           })
      {
        Add(name);
      }
      _machine = _program.Make();
      _globals = QcGlobals(_machine.GetProgs());
      _fields = QcFields(_machine.GetProgs());
      LevelRunningTest::SetUp();
      for (const auto &[builtin, name] : _noting)
      {
        _machine.SetBuiltin(builtin, [this, name](QcMachine &machine)
        {
          _calls.emplace_back(name, _globals.self.Get(machine));
          if (name == "SetNewParms") { _globals.parms[0].Set(machine, 100.0f); }
          if (name == "PutClientInServer") { EXPECT_EQ(_globals.parms[0].Get(machine), 100.0f); }
        });
      }
    }
  };

  TEST_F(LevelRunningClientTest, LetsAPlayerInWithTheThreeFunctionsOfTheGameCode)
  {
    LevelRunning running(_machine);
    const std::int32_t player = _machine.CreateEntity().value_or(0);

    EXPECT_TRUE(running.ConnectClient(player, "ranger"));
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("SetNewParms", player),
                  std::pair<std::string, std::int32_t>("ClientConnect", player),
                  std::pair<std::string, std::int32_t>("PutClientInServer", player)));
    EXPECT_EQ(_fields.netname.GetText(_machine, player), "ranger");
    EXPECT_EQ(_globals.time.Get(_machine), 1.0f);
  }

  TEST_F(LevelRunningClientTest, LetsAPlayerInWithTheNumbersBroughtFromAnotherLevel)
  {
    LevelRunning running(_machine);
    const std::int32_t player = _machine.CreateEntity().value_or(0);

    // PutClientInServer of the fixture expects 100 in the first, which
    // only SetNewParms puts there otherwise
    std::array<float, QcGlobals::parm_count> parms{};
    parms[0] = 100.0f;

    EXPECT_TRUE(running.ConnectClient(player, "ranger", parms));
    // the numbers of a new player are not asked for
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("ClientConnect", player),
                  std::pair<std::string, std::int32_t>("PutClientInServer", player)));
    EXPECT_EQ(_globals.parms[0].Get(_machine), 100.0f);
  }

  TEST_F(LevelRunningClientTest, TakesTheNumbersAPlayerBringsToTheNextLevel)
  {
    LevelRunning running(_machine);
    const std::int32_t player = _machine.CreateEntity().value_or(0);

    // the fixture has no SetChangeParms, so a player leaves as a new one
    EXPECT_THAT(running.SaveClient(player), ::testing::Each(0.0f));
    EXPECT_THAT(running.SaveClient(99), ::testing::Each(0.0f));
  }

  TEST_F(LevelRunningClientTest, RefusesAPlayerWhoseEntityIsFree)
  {
    LevelRunning running(_machine);

    EXPECT_FALSE(running.ConnectClient(1));
    EXPECT_FALSE(running.RunClientThink(1, ClientThink::Before));
    EXPECT_THAT(_calls, IsEmpty());
  }

  TEST_F(LevelRunningClientTest, RunsWhatThePlayerDoesBeforeAndAfterMoving)
  {
    LevelRunning running(_machine);
    const std::int32_t player = _machine.CreateEntity().value_or(0);

    EXPECT_TRUE(running.RunClientThink(player, ClientThink::Before));
    running.Advance(step);
    EXPECT_TRUE(running.RunClientThink(player, ClientThink::After));
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("PlayerPreThink", player),
                  std::pair<std::string, std::int32_t>("PlayerPostThink", player)));
    EXPECT_EQ(_frames_started, 1);
  }

  /// A mover that notes the entities it moves among the calls of the game
  /// code, to see the order of a player's frame.
  class OrderNotingMover final : public LevelMover
  {
    std::vector<std::pair<std::string, std::int32_t>> &_calls;

  public:
    explicit OrderNotingMover(std::vector<std::pair<std::string, std::int32_t>> &calls) : _calls(calls)
    {
    }

    void MoveEntity(const std::int32_t entity, const float dt) override
    {
      _calls.emplace_back("moved", entity);
    }
  };

  TEST_F(LevelRunningClientTest, GivesAPlayerItCountsTheWholeFrameAtThePlayersTurn)
  {
    OrderNotingMover mover(_calls);
    LevelRunning running(_machine, &mover);
    const std::int32_t player = MakeThinker(1.0f);
    const std::int32_t other = MakeThinker(1.0f);
    EXPECT_EQ(running.GetClientCount(), 0);
    running.SetClientCount(1);
    EXPECT_EQ(running.GetClientCount(), 1);

    // before, the thought, the move, after; and only then the next entity
    running.Advance(step);
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("moved", 0),
                  std::pair<std::string, std::int32_t>("PlayerPreThink", player),
                  std::pair<std::string, std::int32_t>("moved", player),
                  std::pair<std::string, std::int32_t>("PlayerPostThink", player),
                  std::pair<std::string, std::int32_t>("moved", other)));
    EXPECT_THAT(_thoughts, ElementsAre(std::pair(player, 1.0f), std::pair(other, 1.0f)));
    EXPECT_EQ(_frames_started, 1);
  }

  TEST_F(LevelRunningClientTest, GivesAPlayerThatPushesTheFrameOfAPlayer)
  {
    OrderNotingMover mover(_calls);
    LevelRunning running(_machine, &mover);
    const std::int32_t player = MakePusher(10.0f, 8.0f);
    running.SetClientCount(1);

    // whatever the game code made of the player, the mover is asked
    running.Advance(step);
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("moved", 0),
                  std::pair<std::string, std::int32_t>("PlayerPreThink", player),
                  std::pair<std::string, std::int32_t>("moved", player),
                  std::pair<std::string, std::int32_t>("PlayerPostThink", player)));
  }

  TEST_F(LevelRunningClientTest, EndsTheFrameOfAPlayerTheGameCodeRemoves)
  {
    OrderNotingMover mover(_calls);
    LevelRunning running(_machine, &mover);
    const std::int32_t player = MakeThinker(1.0f, "special");
    _machine.SetBuiltin(_special, [player](QcMachine &machine) { machine.FreeEntity(player); });
    running.SetClientCount(1);

    running.Advance(step);
    EXPECT_THAT(_calls, ElementsAre(
                  std::pair<std::string, std::int32_t>("moved", 0),
                  std::pair<std::string, std::int32_t>("PlayerPreThink", player)));

    // and a place for a player that is free has no frame at all
    _calls.clear();
    running.Advance(step);
    EXPECT_THAT(_calls, ElementsAre(std::pair<std::string, std::int32_t>("moved", 0)));
  }
}
