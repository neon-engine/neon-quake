#include "qc-core-builtins.hpp"

#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "qc-builtin-number.hpp"
#include "qc-recording-host.test.hpp"

// The builtins with the game code of a real game: a level of it starts, a
// player joins, and a frame begins, with the builtins of QcCoreBuiltins as
// they are and nothing in the place of those that need a world.
namespace
{
  using quake::BspFile;
  using quake::EntityText;
  using quake::Progs;
  using quake::ProgsDefinition;
  using quake::ProgsFunction;
  using quake::ProgsType;
  using quake::QcBuiltinNumber;
  using quake::QcCell;
  using quake::QcCoreBuiltins;
  using quake::QcMachine;
  using quake::QcRecordingHost;
  using quake::RealData;
  using ::testing::Contains;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;
  using ::testing::StartsWith;

  class QcCoreBuiltinsRealDataTest : public ::testing::Test
  {
  protected:
    /// The entity of the only player, the first after the world.
    static constexpr std::int32_t player = 1;

    QcRecordingHost _host;
    QcCoreBuiltins _builtins{_host};
    std::unique_ptr<QcMachine> _machine;

    /// Why runs were stopped, each with what was being run.
    std::vector<std::string> _stops;

    /// How often each builtin that needs a world was called.
    std::map<std::int32_t, std::size_t> _world_calls;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      Progs progs;
      std::string error;
      ASSERT_TRUE(progs.Read(data.GetBytes("progs.dat"), error)) << error;
      _machine = std::make_unique<QcMachine>(std::move(progs));
      ASSERT_EQ(_machine->CreateEntity(), player);

      // The builtins that need a world do nothing here, and return zero.
      // Every other number the game code names has to be one of
      // QcCoreBuiltins, or the machine stops the run that calls it.
      for (const QcBuiltinNumber number : {
             QcBuiltinNumber::SetOrigin, QcBuiltinNumber::SetModel, QcBuiltinNumber::SetSize,
             QcBuiltinNumber::Sound, QcBuiltinNumber::TraceLine, QcBuiltinNumber::CheckClient,
             QcBuiltinNumber::FindRadius, QcBuiltinNumber::WalkMove, QcBuiltinNumber::DropToFloor,
             QcBuiltinNumber::CheckBottom, QcBuiltinNumber::PointContents, QcBuiltinNumber::Aim,
             QcBuiltinNumber::Particle, QcBuiltinNumber::ChangeYaw, QcBuiltinNumber::MoveToGoal,
             QcBuiltinNumber::MakeStatic, QcBuiltinNumber::AmbientSound,
           })
      {
        const auto builtin = static_cast<std::int32_t>(number);
        _machine->SetBuiltin(builtin, [this, builtin](QcMachine &) { _world_calls[builtin]++; });
      }
      _builtins.Register(*_machine);
      _builtins.SeedRandom(1);
    }

    [[nodiscard]] std::int32_t GlobalAt(const std::string_view name) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindGlobal(name);
      return definition == nullptr ? -1 : definition->offset;
    }

    [[nodiscard]] QcCell GetField(const std::int32_t entity, const std::string_view name) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindField(name);
      const std::span<QcCell> fields = _machine->GetEntity(entity);
      return definition == nullptr || definition->offset >= fields.size() ? QcCell{} : fields[definition->offset];
    }

    /// Sets a field of an entity from a key and a value of the text of a
    /// level, by the type the game code gives the field, as the engine of
    /// the original does. A key that is no field is left out.
    void SetFieldFromText(const std::int32_t entity, std::string key, std::string value) const
    {
      // one number for where a thing looks is the yaw of its angles
      if (key == "angle")
      {
        key = "angles";
        value = "0 " + value + " 0";
      }
      // the brightness of a light is not the field `light`, a function
      if (key == "light") { key = "light_lev"; }

      const ProgsDefinition *definition = _machine->GetProgs().FindField(key);
      const std::span<QcCell> fields = _machine->GetEntity(entity);
      if (key.starts_with('_') || definition == nullptr || definition->offset + 3u > fields.size()) { return; }

      const std::span<QcCell> cells = fields.subspan(definition->offset);
      switch (definition->GetType())
      {
        case ProgsType::String:
        {
          cells[0] = QcCell::OfInteger(_machine->AddString(value));
          break;
        }
        case ProgsType::Float:
        {
          cells[0] = QcCell::OfFloat(std::strtof(value.c_str(), nullptr));
          break;
        }
        case ProgsType::Vector:
        {
          const char *at = value.c_str();
          for (std::size_t i = 0; i < 3; i++)
          {
            char *end = nullptr;
            cells[i] = QcCell::OfFloat(std::strtof(at, &end));
            at = end;
          }
          break;
        }
        default:
        {
          break;
        }
      }
    }

    /// Runs a function of the game code for an entity. A run that is
    /// stopped is kept in `_stops`.
    void Run(const std::int32_t entity, const std::string_view function)
    {
      _machine->SetInteger(GlobalAt("self"), entity);
      _machine->SetInteger(GlobalAt("other"), 0);
      if (_machine->Call(function)) { return; }

      const quake::QcError &error = _machine->GetError();
      _stops.push_back(std::format("{}: stopped in {}: {}", function, error.function_name, error.message));
    }

    /// Starts a level of the paks as the engine of the original does: the
    /// world first, then every other entity of its text, each made, given
    /// its fields, and handed to the function its classname names.
    void StartLevel(const std::string_view name)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      BspFile level;
      std::string error;
      ASSERT_TRUE(level.Read(RealData::Get().GetBytes(path), error)) << path << ": " << error;
      EntityText text;
      ASSERT_TRUE(text.Read(level.entities, error)) << path << ": " << error;

      // what the engine sets before any game code runs
      _builtins.GetModels().Add(path);
      _machine->SetFloat(GlobalAt("time"), 1.0f);
      _machine->SetInteger(GlobalAt("mapname"), _machine->AddString(name));
      SetFieldFromText(0, "model", path);

      for (std::size_t i = 0; i < text.entities.size(); i++)
      {
        std::int32_t entity = 0;
        if (i > 0)
        {
          const std::optional<std::int32_t> made = _machine->CreateEntity();
          ASSERT_TRUE(made.has_value());
          entity = *made;
        }
        for (const auto &[key, value] : text.entities[i].pairs) { SetFieldFromText(entity, key, value); }

        const std::string *classname = text.entities[i].Find("classname");
        if (classname == nullptr || !_machine->GetProgs().FindFunction(*classname))
        {
          _builtins.RemoveEntity(*_machine, entity);
          continue;
        }
        Run(entity, *classname);
      }
    }
  };

  TEST_F(QcCoreBuiltinsRealDataTest, RegistersEveryBuiltinTheRealGameCodeNamesThatNeedsNoWorld)
  {
    // every builtin of the file is called once by the host: one that was
    // not registered stops the run
    const std::vector<ProgsFunction> &functions = _machine->GetProgs().functions;
    for (std::size_t i = 1; i < functions.size(); i++)
    {
      if (!functions[i].IsBuiltin()) { continue; }

      const std::int32_t number = -functions[i].first_statement;
      // `error` stops the run by what it is, and a message needs a destination
      _machine->Call(static_cast<std::int32_t>(i));
      if (_machine->HasFailed())
      {
        EXPECT_THAT(_machine->GetError().message, ::testing::Not(HasSubstr("not registered")))
          << quake::QcBuiltinName(number);
      }
    }
  }

  TEST_F(QcCoreBuiltinsRealDataTest, StartsALevelOfTheRealGameWithTheBuiltins)
  {
    _builtins.GetVariables().Set("skill", "2");
    StartLevel("start");
    ASSERT_FALSE(HasFatalFailure());
    EXPECT_THAT(_stops, IsEmpty());

    // worldspawn named the files the game needs, after the level itself
    const quake::QcPrecacheList &models = _builtins.GetModels();
    EXPECT_EQ(models.GetName(1), "maps/start.bsp");
    EXPECT_GT(models.GetCount(), 10);
    EXPECT_TRUE(models.Find("progs/player.mdl").has_value());
    EXPECT_GT(_builtins.GetSounds().GetCount(), 20);
    EXPECT_THAT(_builtins.GetSounds().GetNames(), Contains(StartsWith("weapons/")));

    // and set the lights up: style 0 is steady, the others flicker, and the
    // last is for the lights a switch turns on
    EXPECT_EQ(_builtins.GetLightStyle(0), "m");
    EXPECT_GT(_builtins.GetLightStyle(1).size(), 1u);
    EXPECT_EQ(_builtins.GetLightStyle(63), "a");

    // it made entities of its own, which the host was told of
    EXPECT_THAT(_host.calls, Contains(StartsWith("made ")));

    // a player joins
    _machine->GetEntity(player)[_machine->GetProgs().FindField("netname")->offset] =
      QcCell::OfInteger(_machine->AddString("player"));
    Run(player, "SetNewParms");
    Run(player, "ClientConnect");
    Run(player, "PutClientInServer");
    EXPECT_THAT(_stops, IsEmpty());
    EXPECT_THAT(_host.printed, HasSubstr("player joined the server"));
    EXPECT_EQ(_machine->GetString(GetField(player, "classname").AsInteger()), "player");
    EXPECT_EQ(GetField(player, "health").AsFloat(), 100.0f);

    // then a frame, which reads the console variables the host set
    _machine->SetFloat(GlobalAt("time"), 1.1f);
    _machine->SetFloat(GlobalAt("frametime"), 0.1f);
    Run(0, "StartFrame");
    EXPECT_THAT(_stops, IsEmpty());
    EXPECT_EQ(_machine->GetFloat(GlobalAt("framecount")), 1.0f);
    EXPECT_EQ(_machine->GetFloat(GlobalAt("skill")), 2.0f);

    std::cout << "The level start with the builtins: " << _machine->GetStatementsRun() << " statements, "
              << _machine->GetEntityCount() << " entities, " << models.GetCount() - 1 << " models, "
              << _builtins.GetSounds().GetCount() - 1 << " sounds, " << _host.calls.size()
              << " calls of the host.\nBuiltins that need a world, which did nothing:";
    for (const auto &[number, count] : _world_calls)
    {
      std::cout << " " << quake::QcBuiltinName(number) << " " << count << ",";
    }
    std::cout << "\nWhat the game code printed:\n" << _host.printed << "\n";
  }
}
