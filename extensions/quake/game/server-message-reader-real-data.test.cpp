#include "server-message-reader.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "level-collision.hpp"
#include "level-physics.hpp"
#include "level-running.hpp"
#include "level-spawning.hpp"
#include "level-stepping.hpp"
#include "level-touching.hpp"
#include "level-world-host.test.hpp"
#include "qc-core-builtins.hpp"
#include "qc-flag.hpp"
#include "qc-solid.hpp"
#include "qc-world-builtins.hpp"
#include "server-message-feeding-host.test.hpp"
#include "server-message-recording-listener.test.hpp"

// The tests of ServerMessageReader with the game code and the levels of a
// real game: levels are started with the real builtins, and a player fires
// every weapon, kills every monster, is put into every trigger and onto
// every item, and leaves through the exit, while every part of a message
// the game code writes goes to the reader.
namespace
{
  using quake::BspFile;
  using quake::ClientThink;
  using quake::EntityText;
  using quake::HasFlag;
  using quake::LevelCollision;
  using quake::LevelFailure;
  using quake::LevelPhysics;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelSpawningReport;
  using quake::LevelStepping;
  using quake::LevelTouching;
  using quake::LevelVector;
  using quake::LevelWorldHost;
  using quake::Progs;
  using quake::QcCoreBuiltins;
  using quake::QcFields;
  using quake::QcFlag;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcSolid;
  using quake::QcWorldBuiltins;
  using quake::RealData;
  using quake::ServerMessageFeedingHost;
  using quake::ServerMessageReader;
  using quake::ServerMessageRecordingListener;
  using ::testing::IsEmpty;

  class ServerMessageReaderRealDataTest : public ::testing::Test
  {
  protected:
    static constexpr std::int32_t player = 1;

    /// The length of a frame: that of the original at its usual rate, or a
    /// longer one a test sets to get through many levels.
    float _frame_time = 1.0f / 72.0f;

    Progs _progs;
    BspFile _level;
    std::unique_ptr<QcMachine> _machine;
    std::unique_ptr<QcFields> _fields;
    std::unique_ptr<QcGlobals> _globals;
    std::unique_ptr<ServerMessageRecordingListener> _listener;
    std::unique_ptr<ServerMessageReader> _reader;
    std::unique_ptr<ServerMessageFeedingHost> _host;
    std::unique_ptr<QcCoreBuiltins> _core;
    std::unique_ptr<LevelCollision> _collision;
    std::unique_ptr<LevelRunning> _running;
    std::unique_ptr<LevelTouching> _touching;
    std::unique_ptr<LevelStepping> _stepping;
    std::unique_ptr<LevelPhysics> _physics;
    std::unique_ptr<QcWorldBuiltins> _world;
    std::unique_ptr<LevelWorldHost> _world_host;
    LevelSpawningReport _report;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      std::string error;
      ASSERT_TRUE(_progs.Read(data.GetBytes("progs.dat"), error)) << error;
    }

    /// Starts a level of the paks on a machine made now, as a host does,
    /// with a host that feeds a reader, and lets a player in. False for a
    /// level that is not there or is refused, which fails the test.
    bool Start(const std::string_view name)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      _level = {};
      if (!_level.Read(RealData::Get().GetBytes(path), error))
      {
        ADD_FAILURE() << path << ": " << error;
        return false;
      }

      EntityText text;
      EXPECT_TRUE(text.Read(_level.entities, error)) << path << ": " << error;

      // the order matters: what was made for the machine before goes first
      _world_host.reset();
      _world.reset();
      _physics.reset();
      _stepping.reset();
      _touching.reset();
      _running.reset();
      _collision.reset();
      _core.reset();
      _machine = std::make_unique<QcMachine>(_progs);
      _fields = std::make_unique<QcFields>(_machine->GetProgs());
      _globals = std::make_unique<QcGlobals>(_machine->GetProgs());
      _listener = std::make_unique<ServerMessageRecordingListener>();
      _reader = std::make_unique<ServerMessageReader>(*_listener);
      _host = std::make_unique<ServerMessageFeedingHost>(*_reader);

      _core = std::make_unique<QcCoreBuiltins>(*_host);
      _core->Register(*_machine);
      _core->SeedRandom(1);
      _core->GetVariables().Set("skill", "1");
      _core->GetModels().Add(path);

      _collision = std::make_unique<LevelCollision>(*_machine);
      EXPECT_TRUE(_collision->Build(_level, error)) << path << ": " << error;
      _running = std::make_unique<LevelRunning>(*_machine);
      _touching = std::make_unique<LevelTouching>(*_collision, *_running);
      _stepping = std::make_unique<LevelStepping>(*_collision, *_touching, [this] { return _core->NextRandom(); });
      _physics = std::make_unique<LevelPhysics>(*_collision, *_touching);
      _running->SetMover(_physics.get());
      _world = std::make_unique<QcWorldBuiltins>(*_collision, *_stepping);
      _world->Register(*_machine);
      _world_host = std::make_unique<LevelWorldHost>(_level, *_collision);
      _world_host->Register(*_machine);

      LevelSpawning spawning(*_machine);
      _report = spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = 1});

      _running->Advance(0.1f);
      _running->Advance(0.1f);
      return _running->ConnectClient(player, "player");
    }

    /// Lets time pass with the player in, a frame at a time. The game code
    /// writes a message whole within a frame, so after each the reader is
    /// told that nothing follows.
    void Run(const float seconds)
    {
      const int frames = static_cast<int>(seconds / _frame_time + 0.5f);
      for (int frame = 0; frame < frames; frame++)
      {
        _running->RunClientThink(player, ClientThink::Before);
        _running->Advance(_frame_time);
        _running->RunClientThink(player, ClientThink::After);
        _reader->Flush();
      }
    }

    /// The failures that are the machine's: a run it stopped itself, not
    /// one the game code ended with `error`.
    [[nodiscard]] static std::vector<std::string> OfTheMachine(const std::vector<LevelFailure> &failures)
    {
      std::vector<std::string> messages;
      for (const LevelFailure &failure : failures)
      {
        if (failure.error.message.starts_with(QcCoreBuiltins::error_start)) { continue; }

        messages.push_back(std::format(
          "entity {} ({}), {}: stopped in {}: {}",
          failure.entity, failure.classname, failure.function, failure.error.function_name, failure.error.message));
      }
      return messages;
    }

    /// Makes the player whole again and keeps it from harm, as the cheat
    /// of the game does, so that what it is put into next takes it for
    /// alive.
    void Heal() const
    {
      if (_fields->health.Get(*_machine, player) <= 0.0f) { return; }

      _fields->health.Set(*_machine, player, 100.0f);
      const auto flags = static_cast<std::int32_t>(_fields->flags.Get(*_machine, player));
      _fields->flags.Set(*_machine, player, static_cast<float>(flags | static_cast<std::int32_t>(QcFlag::GodMode)));
    }

    /// Has the player hold the trigger of a weapon for a while. The
    /// weapon is chosen by the number the game code has for its key.
    void Fire(const float weapon, const float seconds)
    {
      _fields->impulse.Set(*_machine, player, weapon);
      _fields->button0.Set(*_machine, player, 1.0f);
      Run(seconds);
      _fields->button0.Set(*_machine, player, 0.0f);
      Heal();
    }

    /// Has the player hurt everything that is a monster by a little more
    /// than it has left, with the function of the game code that hurts: it
    /// falls, and is not torn to pieces, which take long to move when there
    /// are hundreds. Gives how many there were.
    std::size_t KillMonsters()
    {
      std::size_t monsters = 0;
      for (std::int32_t entity = 2; entity < _machine->GetEntityCount(); entity++)
      {
        if (_machine->IsEntityFree(entity)) { continue; }
        if (!HasFlag(_fields->flags.Get(*_machine, entity), QcFlag::Monster)) { continue; }
        if (_fields->health.Get(*_machine, entity) <= 0.0f) { continue; }

        _globals->self.Set(*_machine, player);
        _machine->SetParameterInteger(0, entity);
        _machine->SetParameterInteger(1, player);
        _machine->SetParameterInteger(2, player);
        _machine->SetParameterFloat(3, _fields->health.Get(*_machine, entity) + 1.0f);
        EXPECT_TRUE(_machine->Call("T_Damage")) << _machine->GetError().message;
        _reader->Flush();
        monsters++;
      }
      return monsters;
    }

    /// Puts the player into the middle of every entity that is touched and
    /// not stopped by, one after the other, with a frame after each that is
    /// no item: the
    /// items, the secrets, the teleporters, and whatever else a level sets
    /// off when a player comes by. The exits are told apart by their
    /// classname and are either the only ones or left out. Gives how many
    /// there were.
    std::size_t TouchTriggers(const bool exits)
    {
      std::size_t triggers = 0;
      const std::int32_t count = _machine->GetEntityCount();
      for (std::int32_t entity = 2; entity < count; entity++)
      {
        if (_machine->IsEntityFree(entity)) { continue; }
        if (_fields->solid.Get(*_machine, entity) != static_cast<float>(QcSolid::Trigger)) { continue; }
        if (_fields->touch.Get(*_machine, entity) == 0) { continue; }
        if ((_fields->classname.GetText(*_machine, entity) == "trigger_changelevel") != exits) { continue; }

        const LevelVector low = _fields->absmin.Get(*_machine, entity);
        const LevelVector high = _fields->absmax.Get(*_machine, entity);
        Heal();
        _fields->origin.Set(*_machine, player, quake::Scaled(quake::Sum(low, high), 0.5f));
        _fields->velocity.Set(*_machine, player, {});
        _collision->Link(player);
        _touching->TouchTriggers(player);
        _reader->Flush();
        // what is picked up is gone at once, and anything else may think
        if (!HasFlag(_fields->flags.Get(*_machine, entity), QcFlag::Item)) { Run(_frame_time); }
        triggers++;
      }
      return triggers;
    }

    /// Plays a level as far as a test can: every weapon is fired, every
    /// monster killed, every trigger and item touched, and the level left
    /// through its exit, with the tally waited out and a key pressed.
    void Play()
    {
      Heal();
      Run(2.0f);

      // The key 9 is the cheat that gives every weapon. Then the shotgun,
      // the two guns for nails, the lightning, grenades, and rockets.
      Fire(9.0f, 0.2f);
      for (const float weapon : {2.0f, 4.0f, 5.0f, 8.0f, 6.0f, 7.0f}) { Fire(weapon, 0.7f); }

      KillMonsters();
      Run(3.0f);
      TouchTriggers(false);
      Run(2.0f);
      // what the triggers woke or let in
      KillMonsters();
      Run(2.0f);

      if (TouchTriggers(true) == 0) { return; }
      Run(1.0f);

      // the tally stays for some seconds before a key ends it
      _running->SetTime(_running->GetTime() + 60.0);
      _fields->button0.Set(*_machine, player, 1.0f);
      Run(1.0f);
      _fields->button0.Set(*_machine, player, 0.0f);
      Run(1.0f);
    }

    /// The levels of the paks by their names, such as `start`.
    [[nodiscard]] static std::vector<std::string> ListLevels()
    {
      std::vector<std::string> levels;
      for (const std::string &path : RealData::Get().ListNames("maps"))
      {
        // the models of the boxes of ammunition are in the same folder and
        // format, and are no levels
        const std::string_view file = std::string_view(path).substr(std::string_view("maps/").size());
        if (!file.ends_with(".bsp") || file.starts_with("b_")) { continue; }
        levels.emplace_back(file.substr(0, file.size() - std::string_view(".bsp").size()));
      }
      return levels;
    }
  };

  TEST_F(ServerMessageReaderRealDataTest, ReadsWhatTheFirstLevelWritesWhenItIsPlayedAndLeft)
  {
    ASSERT_TRUE(Start("lq_e1m1"));
    Play();
    EXPECT_THAT(_listener->problems, IsEmpty());
    EXPECT_THAT(OfTheMachine(_running->GetFailures()), IsEmpty());

    // every monster of the level was counted as it died
    EXPECT_EQ(_listener->counts["killedmonster"], 49u);
    EXPECT_EQ(_globals->killed_monsters.Get(*_machine), 49.0f);
    // and every secret as it was found
    EXPECT_GT(_listener->counts["foundsecret"], 0u);
    EXPECT_EQ(static_cast<float>(_listener->counts["foundsecret"]), _globals->found_secrets.Get(*_machine));
    EXPECT_EQ(_globals->found_secrets.Get(*_machine), _globals->total_secrets.Get(*_machine));

    // the shots of the player hit walls, and its rockets went off
    EXPECT_GT(_listener->counts["point 2"], 0u);
    EXPECT_GT(_listener->counts["point 3"], 0u);

    // the exit set the music of the tally and started it, once
    EXPECT_EQ(_listener->counts["intermission"], 1u);
    EXPECT_EQ(_listener->counts["cdtrack"], 1u);
    // and the key after it asked for the next level
    EXPECT_THAT(_host->levels, ::testing::Not(IsEmpty()));
  }

  TEST_F(ServerMessageReaderRealDataTest, ReadsWhatEveryLevelOfTheRealGameWritesWithoutAMessageItCannotRead)
  {
    std::size_t levels = 0;
    std::size_t written = 0;
    std::map<std::string, std::size_t> counts;
    std::map<std::string, std::size_t> destinations;
    std::vector<std::string> texts;
    // ten frames in a second, the fewest the original runs with, to get
    // through every level in little time
    _frame_time = 0.1f;
    for (const std::string &level : ListLevels())
    {
      if (!Start(level)) { continue; }

      Play();
      levels++;
      written += _host->written;
      EXPECT_THAT(_listener->problems, IsEmpty()) << level;
      EXPECT_THAT(OfTheMachine(_running->GetFailures()), IsEmpty()) << level;

      for (const auto &[name, count] : _listener->counts) { counts[name] += count; }
      for (const std::string &message : _listener->messages)
      {
        const std::size_t to = message.find(" to ");
        destinations[message.substr(to + 4, 1)]++;
        if (message.starts_with("finale") || message.starts_with("cutscene") || message.starts_with("cdtrack") ||
            message.starts_with("sellscreen") || message.starts_with("setv") || message.starts_with("updatestat"))
        {
          texts.push_back(level + ": " + message.substr(0, 100));
        }
      }
    }
    EXPECT_GT(levels, 3u);
    EXPECT_GT(counts["killedmonster"], 100u);
    EXPECT_GT(counts["intermission"], 3u);

    std::cout << levels << " levels, " << written << " parts written. Messages by kind:\n";
    for (const auto &[name, count] : counts) { std::cout << "  " << name << ": " << count << "\n"; }
    std::cout << "Messages by destination:\n";
    for (const auto &[name, count] : destinations) { std::cout << "  " << name << ": " << count << "\n"; }
    std::cout << "The rarer messages:\n";
    for (const std::string &text : texts) { std::cout << "  " << text << "\n"; }
  }
}
