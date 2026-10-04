#include "qc-machine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bsp-file.hpp"
#include "entity-text.hpp"
#include "game/qc-builtin-number.hpp"
#include "real-data.test.hpp"

// The tests of QcMachine with the game code of a real game: the start of a
// level as the engine of the original does it, with builtins that stand in
// for an engine. They are not a game. They are there to find where the
// machine does not agree with what a real compiler wrote.
namespace
{
  using quake::BspEntity;
  using quake::BspFile;
  using quake::EntityText;
  using quake::Progs;
  using quake::ProgsDefinition;
  using quake::ProgsFunction;
  using quake::ProgsType;
  using quake::QcCell;
  using quake::QcMachine;
  using quake::RealData;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  using Vector = std::array<float, 3>;

  constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;

  float Length(const Vector &vector)
  {
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
  }

  /// The angle around the up axis a direction points at, in whole degrees
  /// from 0 to 359, as the game counts it.
  float YawOf(const Vector &direction)
  {
    if (direction[0] == 0.0f && direction[1] == 0.0f) { return 0.0f; }

    const float yaw = std::trunc(std::atan2(direction[1], direction[0]) / degrees_to_radians);
    return yaw < 0.0f ? yaw + 360.0f : yaw;
  }

  /// The angle a direction points up by, in whole degrees from 0 to 359.
  float PitchOf(const Vector &direction)
  {
    if (direction[0] == 0.0f && direction[1] == 0.0f) { return direction[2] > 0.0f ? 90.0f : 270.0f; }

    const float flat = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
    const float pitch = std::trunc(std::atan2(direction[2], flat) / degrees_to_radians);
    return pitch < 0.0f ? pitch + 360.0f : pitch;
  }

  /// A number as the game code gets it from `ftos`: without a fraction when
  /// it has none.
  std::string TextOf(const float value)
  {
    return value == std::trunc(value) ? std::format("{:.0f}", value) : std::format("{:5.1f}", value);
  }

  /// A level that starts on the machine, with the game code of the real
  /// game and builtins that do as little as lets the code run.
  class QcMachineRealDataTest : public ::testing::Test
  {
  protected:
    /// The entity the only player of the tests is, as in the original, which
    /// keeps the first entities after the world for the players.
    static constexpr std::int32_t player = 1;

    /// The game code as it was read, of which every machine gets a copy.
    Progs _progs;

    /// What the message of an error starts with that the game code raised
    /// itself.
    static constexpr std::string_view game_error = "The game code says: ";

    /// The number the game code has for how a door or a lift moves.
    static constexpr float move_type_push = 7.0f;

    std::unique_ptr<QcMachine> _machine;

    /// The level that was started, and its name.
    BspFile _level;
    std::string _level_name;

    /// How many entities of each classname were handed to the function of
    /// that name, and the classnames the game code has no function for.
    std::map<std::string, std::size_t> _spawned;
    std::set<std::string> _without;

    /// Why runs were stopped, each with how often, where a test goes on
    /// after one to see them all.
    std::map<std::string, std::size_t> _stops;

    /// The same for the errors the game code raised itself, with the
    /// builtins `error` and `objerror`: no fault of the machine.
    std::map<std::string, std::size_t> _game_errors;

    /// Everything the game code printed, in the order it did.
    std::string _printed;

    /// How often each builtin was called, by its number.
    std::map<std::int32_t, std::size_t> _builtin_calls;

    /// The stand-ins, by the name of the builtin.
    std::map<std::string_view, QcMachine::Builtin> _stubs;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      const auto bytes = data.GetBytes("progs.dat");
      ASSERT_FALSE(bytes.empty()) << "The pak has no progs.dat. " << data.GetProblem();

      std::string error;
      ASSERT_TRUE(_progs.Read(bytes, error)) << error;

      MakeStubs();
      MakeMachine();
    }

    /// Makes a machine that has run nothing yet, with the builtins and the
    /// entity of the player, in the place of the one there was.
    void MakeMachine()
    {
      _machine = std::make_unique<QcMachine>(_progs);
      ASSERT_EQ(_machine->CreateEntity(), player);
      RegisterStubs();
    }

    // The globals and fields of the game, by their names.

    /// Where a global is, or -1, which the machine reads as zero and
    /// refuses to write.
    [[nodiscard]] std::int32_t GlobalAt(const std::string_view name) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindGlobal(name);
      return definition == nullptr ? -1 : definition->offset;
    }

    /// Sets cells of a field of an entity, where the game has a field of
    /// that name.
    void SetField(const std::int32_t entity, const std::string_view name, const std::vector<QcCell> &cells) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindField(name);
      const std::span<QcCell> fields = _machine->GetEntity(entity);
      if (definition == nullptr || definition->offset + cells.size() > fields.size()) { return; }

      for (std::size_t i = 0; i < cells.size(); i++) { fields[definition->offset + i] = cells[i]; }
    }

    void SetVectorField(const std::int32_t entity, const std::string_view name, const Vector &value) const
    {
      SetField(entity, name, {QcCell::OfFloat(value[0]), QcCell::OfFloat(value[1]), QcCell::OfFloat(value[2])});
    }

    /// A cell of a field of an entity. Zero where there is no such field.
    [[nodiscard]] QcCell GetField(const std::int32_t entity, const std::string_view name) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindField(name);
      const std::span<QcCell> fields = _machine->GetEntity(entity);
      if (definition == nullptr || definition->offset >= fields.size()) { return {}; }

      return fields[definition->offset];
    }

    [[nodiscard]] Vector GetVectorField(const std::int32_t entity, const std::string_view name) const
    {
      const ProgsDefinition *definition = _machine->GetProgs().FindField(name);
      const std::span<QcCell> fields = _machine->GetEntity(entity);
      if (definition == nullptr || definition->offset + 3u > fields.size()) { return {}; }

      return {
        fields[definition->offset].AsFloat(),
        fields[definition->offset + 1u].AsFloat(),
        fields[definition->offset + 2u].AsFloat(),
      };
    }

    /// Frees an entity as the engine of the original does. The machine
    /// leaves a free entity as it was; the engine takes it out of the world
    /// with these fields, and has it think no more.
    void RemoveEntity(const std::int32_t entity) const
    {
      if (!_machine->FreeEntity(entity)) { return; }

      for (const std::string_view name : {"model", "takedamage", "modelindex", "colormap", "skin", "frame", "solid"})
      {
        SetField(entity, name, {QcCell{}});
      }
      SetVectorField(entity, "origin", {});
      SetVectorField(entity, "angles", {});
      SetField(entity, "nextthink", {QcCell::OfFloat(-1.0f)});
    }

    // The builtins.

    /// Makes the stand-ins. One that is not made here does nothing.
    void MakeStubs()
    {
      // the entities
      _stubs["spawn"] = [](QcMachine &machine)
      {
        const std::optional<std::int32_t> entity = machine.CreateEntity();
        if (!entity) { return machine.Stop("spawn: there is no room for another entity"); }
        machine.SetReturnInteger(*entity);
      };
      _stubs["remove"] = [this](QcMachine &machine) { RemoveEntity(machine.GetParameterInteger(0)); };
      _stubs["setmodel"] = [this](QcMachine &machine)
      {
        const std::int32_t entity = machine.GetParameterInteger(0);
        SetField(entity, "model", {QcCell::OfInteger(machine.GetParameterInteger(1))});

        // A part of the level, which its text names `*1`, `*2`, and so on,
        // gives the entity its size. The game code tells by it which doors
        // touch and are one door.
        const std::string name(machine.GetParameterString(1));
        if (!name.starts_with('*')) { return; }
        const auto model = static_cast<std::size_t>(std::atoi(name.c_str() + 1));
        if (model >= _level.models.size()) { return; }

        const quake::BspModel &part = _level.models[model];
        SetVectorField(entity, "mins", {part.mins.x, part.mins.y, part.mins.z});
        SetVectorField(entity, "maxs", {part.maxs.x, part.maxs.y, part.maxs.z});
        SetVectorField(entity, "size", {
          part.maxs.x - part.mins.x, part.maxs.y - part.mins.y, part.maxs.z - part.mins.z,
        });
      };
      _stubs["setsize"] = [this](QcMachine &machine)
      {
        const std::int32_t entity = machine.GetParameterInteger(0);
        const Vector mins = machine.GetParameterVector(1);
        const Vector maxs = machine.GetParameterVector(2);
        SetVectorField(entity, "mins", mins);
        SetVectorField(entity, "maxs", maxs);
        SetVectorField(entity, "size", {maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]});
      };
      _stubs["setorigin"] = [this](QcMachine &machine)
      {
        SetVectorField(machine.GetParameterInteger(0), "origin", machine.GetParameterVector(1));
      };

      // The next entity after one, and the next whose field of a string
      // has a text. The world stands for no entity.
      _stubs["nextent"] = [](QcMachine &machine)
      {
        std::int32_t entity = machine.GetParameterInteger(0) + 1;
        while (entity < machine.GetEntityCount() && machine.IsEntityFree(entity)) { entity++; }
        machine.SetReturnInteger(entity > 0 && entity < machine.GetEntityCount() ? entity : 0);
      };
      _stubs["find"] = [](QcMachine &machine)
      {
        const std::int32_t field = machine.GetParameterInteger(1);
        const std::string_view wanted = machine.GetParameterString(2);
        machine.SetReturnInteger(0);
        for (std::int32_t entity = std::max(machine.GetParameterInteger(0), -1) + 1;
          entity < machine.GetEntityCount(); entity++)
        {
          const std::span<QcCell> fields = machine.GetEntity(entity);
          if (machine.IsEntityFree(entity) || field < 0 || static_cast<std::size_t>(field) >= fields.size())
          {
            continue;
          }
          if (machine.GetString(fields[static_cast<std::size_t>(field)].AsInteger()) == wanted)
          {
            machine.SetReturnInteger(entity);
            return;
          }
        }
      };

      // The entities whose origin is within a distance of a place, each
      // naming the next in its field `chain`, the last one found first.
      _stubs["findradius"] = [this](QcMachine &machine)
      {
        const Vector place = machine.GetParameterVector(0);
        const float radius = machine.GetParameterFloat(1);
        std::int32_t chain = 0;
        for (std::int32_t entity = 1; entity < machine.GetEntityCount(); entity++)
        {
          if (machine.IsEntityFree(entity) || GetField(entity, "solid").AsFloat() == 0.0f) { continue; }

          const Vector origin = GetVectorField(entity, "origin");
          if (Length({origin[0] - place[0], origin[1] - place[1], origin[2] - place[2]}) > radius) { continue; }

          SetField(entity, "chain", {QcCell::OfInteger(chain)});
          chain = entity;
        }
        machine.SetReturnInteger(chain);
      };

      // every monster may see the player
      _stubs["checkclient"] = [](QcMachine &machine) { machine.SetReturnInteger(player); };

      // The world is empty space with a floor wherever one is asked for: a
      // line hits nothing on its way, and a thing lands and walks where it
      // is.
      _stubs["traceline"] = [this](QcMachine &machine)
      {
        machine.SetFloat(GlobalAt("trace_allsolid"), 0.0f);
        machine.SetFloat(GlobalAt("trace_startsolid"), 0.0f);
        machine.SetFloat(GlobalAt("trace_fraction"), 1.0f);
        machine.SetVector(GlobalAt("trace_endpos"), machine.GetParameterVector(1));
        machine.SetInteger(GlobalAt("trace_ent"), 0);
        machine.SetFloat(GlobalAt("trace_inopen"), 1.0f);
        machine.SetFloat(GlobalAt("trace_inwater"), 0.0f);
      };
      const auto return_true = [](QcMachine &machine) { machine.SetReturnFloat(1.0f); };
      for (const std::string_view name : {"walkmove", "droptofloor", "checkbottom"}) { _stubs[name] = return_true; }
      // the number the game has for empty space
      _stubs["pointcontents"] = [](QcMachine &machine) { machine.SetReturnFloat(-1.0f); };
      _stubs["aim"] = [this](QcMachine &machine) { machine.SetReturnVector(machine.GetVector(GlobalAt("v_forward"))); };

      // a file that is asked for is there
      const auto return_parameter = [](QcMachine &machine)
      {
        machine.SetReturnInteger(machine.GetParameterInteger(0));
      };
      for (const std::string_view name : {
             "precache_model", "precache_sound", "precache_file", "precache_model2", "precache_sound2",
             "precache_file2",
           })
      {
        _stubs[name] = return_parameter;
      }

      // text
      _stubs["dprint"] = [this](QcMachine &machine) { _printed += machine.GetParameterString(0); };
      _stubs["bprint"] = [this](QcMachine &machine) { _printed += machine.GetParameterString(0); };
      _stubs["sprint"] = [this](QcMachine &machine) { _printed += machine.GetParameterString(1); };
      _stubs["centerprint"] = [this](QcMachine &machine)
      {
        // the game code passes the text in pieces, after the player
        for (std::int32_t i = 1; i < machine.GetArgumentCount(); i++) { _printed += machine.GetParameterString(i); }
        _printed += "\n";
      };
      _stubs["ftos"] = [](QcMachine &machine) { machine.SetReturnString(TextOf(machine.GetParameterFloat(0))); };
      _stubs["vtos"] = [](QcMachine &machine)
      {
        const Vector vector = machine.GetParameterVector(0);
        machine.SetReturnString(std::format("'{:5.1f} {:5.1f} {:5.1f}'", vector[0], vector[1], vector[2]));
      };

      // An error of the game code ends the run, as in the original. Here
      // it is an error of the machine with a start that tells it apart.
      _stubs["error"] = [](QcMachine &machine)
      {
        machine.Stop(std::format("{}{}", game_error, machine.GetParameterString(0)));
      };
      // An error of one entity ended the run in the original as well. The
      // engines the real game is made for only take the entity away and go
      // on, and its game code is written for that: levels of it raise this
      // error while they start.
      _stubs["objerror"] = [this](QcMachine &machine)
      {
        _game_errors[std::format("{}: objerror: {}", _level_name, machine.GetParameterString(0))]++;
        RemoveEntity(machine.GetInteger(GlobalAt("self")));
      };

      // numbers
      _stubs["random"] = [](QcMachine &machine) { machine.SetReturnFloat(0.5f); };
      _stubs["cvar"] = [](QcMachine &machine) { machine.SetReturnFloat(0.0f); };
      _stubs["rint"] = [](QcMachine &machine) { machine.SetReturnFloat(std::round(machine.GetParameterFloat(0))); };
      _stubs["floor"] = [](QcMachine &machine) { machine.SetReturnFloat(std::floor(machine.GetParameterFloat(0))); };
      _stubs["ceil"] = [](QcMachine &machine) { machine.SetReturnFloat(std::ceil(machine.GetParameterFloat(0))); };
      _stubs["fabs"] = [](QcMachine &machine) { machine.SetReturnFloat(std::fabs(machine.GetParameterFloat(0))); };
      _stubs["vlen"] = [](QcMachine &machine) { machine.SetReturnFloat(Length(machine.GetParameterVector(0))); };
      _stubs["normalize"] = [](QcMachine &machine)
      {
        const Vector vector = machine.GetParameterVector(0);
        const float length = Length(vector);
        machine.SetReturnVector(
          length == 0.0f ? Vector{} : Vector{vector[0] / length, vector[1] / length, vector[2] / length});
      };
      _stubs["vectoyaw"] = [](QcMachine &machine) { machine.SetReturnFloat(YawOf(machine.GetParameterVector(0))); };
      _stubs["vectoangles"] = [](QcMachine &machine)
      {
        const Vector direction = machine.GetParameterVector(0);
        machine.SetReturnVector({PitchOf(direction), YawOf(direction), 0.0f});
      };
      _stubs["makevectors"] = [this](QcMachine &machine)
      {
        // The angles are pitch, yaw, and roll in degrees. Forward is where
        // they look, with a positive pitch looking down; right and up are
        // the other two axes of the same turn.
        const Vector angles = machine.GetParameterVector(0);
        const float sin_pitch = std::sin(angles[0] * degrees_to_radians);
        const float cos_pitch = std::cos(angles[0] * degrees_to_radians);
        const float sin_yaw = std::sin(angles[1] * degrees_to_radians);
        const float cos_yaw = std::cos(angles[1] * degrees_to_radians);
        const float sin_roll = std::sin(angles[2] * degrees_to_radians);
        const float cos_roll = std::cos(angles[2] * degrees_to_radians);

        machine.SetVector(GlobalAt("v_forward"), {cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch});
        machine.SetVector(GlobalAt("v_right"), {
          cos_roll * sin_yaw - sin_roll * sin_pitch * cos_yaw,
          -cos_roll * cos_yaw - sin_roll * sin_pitch * sin_yaw,
          -sin_roll * cos_pitch,
        });
        machine.SetVector(GlobalAt("v_up"), {
          cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw,
          cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw,
          cos_roll * cos_pitch,
        });
      };

    }

    /// Registers a builtin for every number the game code names: the
    /// stand-in of its name, or one that does nothing. Every one counts
    /// its calls.
    void RegisterStubs()
    {
      for (const ProgsFunction &function : _machine->GetProgs().functions)
      {
        if (!function.IsBuiltin()) { continue; }

        const std::int32_t number = -function.first_statement;
        const std::string_view name = quake::QcBuiltinName(number);
        ASSERT_FALSE(name.empty()) << "Builtin " << number << " is none of the original";

        const auto stub = _stubs.find(name);
        QcMachine::Builtin does = stub == _stubs.end() ? QcMachine::Builtin([](QcMachine &) {}) : stub->second;
        _machine->SetBuiltin(number, [this, number, does = std::move(does)](QcMachine &machine)
        {
          _builtin_calls[number]++;
          does(machine);
        });
      }
    }

    // What the engine does when a level starts.

    /// Sets a field of an entity from a key and a value of the text of a
    /// level, by the type the game code gives the field. A key that is no
    /// field is left out, and so is one for the compiler of levels.
    void SetFieldFromText(const std::int32_t entity, std::string key, std::string value) const
    {
      if (key.starts_with('_')) { return; }

      // one number for where a thing looks is the yaw of its angles
      if (key == "angle")
      {
        key = "angles";
        value = "0 " + value + " 0";
      }
      // the brightness of a light is not the field `light`, which is a
      // function that makes one
      if (key == "light") { key = "light_lev"; }

      const ProgsDefinition *definition = _machine->GetProgs().FindField(key);
      if (definition == nullptr) { return; }

      switch (definition->GetType())
      {
        case ProgsType::String:
        {
          // the two characters of a new line in the text of a level are one
          std::string text;
          for (std::size_t i = 0; i < value.size(); i++)
          {
            if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == 'n')
            {
              text += '\n';
              i++;
              continue;
            }
            text += value[i];
          }
          SetField(entity, key, {QcCell::OfInteger(_machine->AddString(text))});
          break;
        }
        case ProgsType::Float:
        {
          SetField(entity, key, {QcCell::OfFloat(std::strtof(value.c_str(), nullptr))});
          break;
        }
        case ProgsType::Vector:
        {
          Vector vector{};
          const char *at = value.c_str();
          for (float &part : vector)
          {
            char *end = nullptr;
            part = std::strtof(at, &end);
            at = end;
          }
          SetVectorField(entity, key, vector);
          break;
        }
        case ProgsType::Entity:
        {
          SetField(entity, key, {QcCell::OfInteger(std::atoi(value.c_str()))});
          break;
        }
        case ProgsType::Function:
        {
          const std::optional<std::int32_t> function = _machine->GetProgs().FindFunction(value);
          if (function) { SetField(entity, key, {QcCell::OfInteger(*function)}); }
          break;
        }
        default:
        {
          break;
        }
      }
    }

    /// Runs a function by its number for an entity, with another as
    /// `other`. A run that is stopped is kept in `_stops` under what was
    /// being done, and false is returned.
    bool Run(const std::int32_t entity, const std::int32_t function, const std::string_view what,
      const std::int32_t other = 0)
    {
      _machine->SetInteger(GlobalAt("self"), entity);
      _machine->SetInteger(GlobalAt("other"), other);
      if (_machine->Call(function)) { return true; }

      const quake::QcError &error = _machine->GetError();
      auto &kept = error.message.starts_with(game_error) ? _game_errors : _stops;
      kept[std::format("{}: {}: stopped in {}: {}", _level_name, what, error.function_name, error.message)]++;
      return false;
    }

    bool Run(const std::int32_t entity, const std::string_view function, const std::int32_t other = 0)
    {
      const std::optional<std::int32_t> number = _machine->GetProgs().FindFunction(function);
      EXPECT_TRUE(number.has_value()) << function;
      return number && Run(entity, *number, function, other);
    }

    /// The name of the function a field of an entity holds, for saying
    /// what was run.
    [[nodiscard]] std::string NameOfFunction(const std::int32_t function) const
    {
      const auto &functions = _machine->GetProgs().functions;
      if (function <= 0 || static_cast<std::size_t>(function) >= functions.size()) { return "nothing"; }

      return std::string(_machine->GetProgs().GetString(functions[static_cast<std::size_t>(function)].name));
    }

    /// Lets the player in, as the engine does when one connects.
    void ConnectPlayer()
    {
      SetField(player, "netname", {QcCell::OfInteger(_machine->AddString("player"))});
      Run(player, "SetNewParms");
      Run(player, "ClientConnect");
      Run(player, "PutClientInServer");
    }

    /// One frame of the game at a time: the start of the frame, the player
    /// before and after, and every entity whose time to think has come.
    void RunFrame(const float time)
    {
      _machine->SetFloat(GlobalAt("time"), time);
      _machine->SetFloat(GlobalAt("frametime"), 0.1f);
      Run(0, "StartFrame");
      Run(player, "PlayerPreThink");

      const std::int32_t count = _machine->GetEntityCount();
      for (std::int32_t entity = 1; entity < count; entity++)
      {
        // What pushes, a door or a lift, has a time of its own, which the
        // engine moves on with it. Here it moves on every frame.
        float now = time;
        if (GetField(entity, "movetype").AsFloat() == move_type_push)
        {
          now = GetField(entity, "ltime").AsFloat() + 0.1f;
          SetField(entity, "ltime", {QcCell::OfFloat(now)});
        }

        const float next_think = GetField(entity, "nextthink").AsFloat();
        if (_machine->IsEntityFree(entity) || next_think <= 0.0f || next_think > now) { continue; }

        // the engine forgets the time before the entity thinks, so that it
        // thinks again only when it asks to
        SetField(entity, "nextthink", {QcCell::OfFloat(0.0f)});
        const std::int32_t think = GetField(entity, "think").AsInteger();
        if (think != 0) { Run(entity, think, "think " + NameOfFunction(think)); }
      }

      Run(player, "PlayerPostThink");
    }

    /// Runs the function a field holds for every entity that has one, with
    /// the player as `other`: touching or using everything of a level.
    void RunFieldOfEveryEntity(const std::string_view field)
    {
      const std::int32_t count = _machine->GetEntityCount();
      for (std::int32_t entity = 1; entity < count; entity++)
      {
        const std::int32_t function = GetField(entity, field).AsInteger();
        if (_machine->IsEntityFree(entity) || function == 0) { continue; }

        _machine->SetInteger(GlobalAt("activator"), player);
        Run(entity, function, std::format("{} {}", field, NameOfFunction(function)), player);
      }
    }

    /// Has the player do a great damage to every entity that takes any.
    void DamageEveryEntity()
    {
      const std::int32_t count = _machine->GetEntityCount();
      for (std::int32_t entity = player + 1; entity < count; entity++)
      {
        if (_machine->IsEntityFree(entity) || GetField(entity, "takedamage").AsFloat() == 0.0f) { continue; }

        _machine->SetParameterInteger(0, entity);
        _machine->SetParameterInteger(1, player);
        _machine->SetParameterInteger(2, player);
        _machine->SetParameterFloat(3, 1000.0f);
        Run(player, "T_Damage");
      }
    }

    /// Starts a level of the paks: the world first, then every other
    /// entity of its text, each made, given its fields, and handed to the
    /// function its classname names. False, with nothing run, for a level
    /// that is not there or is refused, which fails the test.
    bool StartLevel(const std::string_view name)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      if (!_level.Read(RealData::Get().GetBytes(path), error))
      {
        ADD_FAILURE() << path << ": " << error;
        return false;
      }

      EntityText entity_text;
      EXPECT_TRUE(entity_text.Read(_level.entities, error)) << path << ": " << error;
      const std::vector<BspEntity> &entities = entity_text.entities;
      _level_name = name;

      // what the engine sets before any game code runs
      _machine->SetFloat(GlobalAt("time"), 1.0f);
      _machine->SetInteger(GlobalAt("mapname"), _machine->AddString(name));
      SetField(0, "model", {QcCell::OfInteger(_machine->AddString(path))});

      for (std::size_t i = 0; i < entities.size(); i++)
      {
        std::int32_t entity = 0;
        if (i > 0)
        {
          const std::optional<std::int32_t> made = _machine->CreateEntity();
          EXPECT_TRUE(made.has_value()) << "There is no room for entity " << i << " of the level";
          if (!made) { break; }
          entity = *made;
        }

        for (const auto &[key, value] : entities[i].pairs) { SetFieldFromText(entity, key, value); }

        const std::string *classname = entities[i].Find("classname");
        if (classname == nullptr || !_machine->GetProgs().FindFunction(*classname))
        {
          _without.insert(classname == nullptr ? "" : *classname);
          _machine->FreeEntity(entity);
          continue;
        }

        Run(entity, *classname);
        _spawned[*classname]++;
      }
      return true;
    }
  };

  TEST_F(QcMachineRealDataTest, RunsMain)
  {
    // the engine never calls it; in the sources it names the files of the
    // game, which the compiler left out
    EXPECT_TRUE(Run(0, "main"));
    EXPECT_FALSE(_machine->HasFailed());
    EXPECT_THAT(_stops, IsEmpty());
    EXPECT_THAT(_printed, HasSubstr("main function"));
  }

  TEST_F(QcMachineRealDataTest, StartsALevelOfTheRealGame)
  {
    ASSERT_TRUE(StartLevel("start"));
    EXPECT_FALSE(_machine->HasFailed()) << _machine->GetError().message;
    EXPECT_THAT(_stops, IsEmpty());
    const std::int64_t statements_of_level = _machine->GetStatementsRun();

    // the world ran, and the things a level of the game has
    EXPECT_EQ(_spawned["worldspawn"], 1u);
    EXPECT_EQ(_spawned["info_player_start"], 1u);
    EXPECT_GT(_spawned.size(), 5u);
    EXPECT_THAT(_without, IsEmpty());
    EXPECT_GT(statements_of_level, 1000);

    // worldspawn names the files the game needs and sets the lights up
    EXPECT_GT(_builtin_calls[19], 20u);
    EXPECT_GT(_builtin_calls[20], 5u);
    EXPECT_GT(_builtin_calls[35], 10u);

    // then a frame, as the engine starts each one
    _machine->SetFloat(GlobalAt("time"), 1.1f);
    _machine->SetFloat(GlobalAt("frametime"), 0.1f);
    EXPECT_TRUE(Run(0, "StartFrame"));
    EXPECT_FALSE(_machine->HasFailed());
    EXPECT_THAT(_stops, IsEmpty());
    EXPECT_EQ(_machine->GetFloat(GlobalAt("framecount")), 1.0f);

    std::cout << "The level start: " << statements_of_level << " statements for " << _machine->GetEntityCount()
              << " entities, then " << _machine->GetStatementsRun() - statements_of_level
              << " for StartFrame.\nFunctions that ran, and for how many entities:\n";
    for (const auto &[classname, count] : _spawned) { std::cout << "  " << classname << " " << count << "\n"; }
    std::cout << "Builtins called:";
    for (const auto &[number, count] : _builtin_calls)
    {
      std::cout << " " << quake::QcBuiltinName(number) << " " << count << ",";
    }
    std::cout << "\nWhat the game code printed:\n" << _printed << "\n";
  }

  TEST_F(QcMachineRealDataTest, PutsThePlayerWhereTheLevelStartsIt)
  {
    ASSERT_TRUE(StartLevel("lq_e1m1"));
    ConnectPlayer();
    ASSERT_THAT(_stops, IsEmpty());
    ASSERT_THAT(_game_errors, IsEmpty());

    // the text of the level, through the fields of the world
    EXPECT_EQ(_machine->GetString(GetField(0, "message").AsInteger()), "Rats Behind Bars");
    EXPECT_EQ(GetField(0, "worldtype").AsFloat(), 2.0f);

    // every monster of the level counted itself: 57 soldiers and 15 dogs
    EXPECT_EQ(_machine->GetFloat(GlobalAt("total_monsters")), 72.0f);

    // the level has `"origin" "968 1456 88"` and `"angle" "80"` for where a
    // player starts, and the game code puts the player one unit above
    EXPECT_THAT(GetVectorField(player, "origin"), ::testing::ElementsAre(968.0f, 1456.0f, 89.0f));
    EXPECT_THAT(GetVectorField(player, "angles"), ::testing::ElementsAre(0.0f, 80.0f, 0.0f));
    EXPECT_EQ(_machine->GetString(GetField(player, "classname").AsInteger()), "player");
    EXPECT_EQ(_machine->GetString(GetField(player, "model").AsInteger()), "progs/player.mdl");
    EXPECT_EQ(GetField(player, "health").AsFloat(), 100.0f);
    EXPECT_THAT(GetVectorField(player, "view_ofs"), ::testing::ElementsAre(0.0f, 0.0f, 22.0f));

    // the size of a player, which reached the builtin as two vectors among
    // the parameters
    EXPECT_THAT(GetVectorField(player, "mins"), ::testing::ElementsAre(-16.0f, -16.0f, -24.0f));
    EXPECT_THAT(GetVectorField(player, "size"), ::testing::ElementsAre(32.0f, 32.0f, 56.0f));

    // what SetNewParms left in the globals for a new player: an axe and a
    // shotgun, and 25 shells, which the shotgun in the hand makes the
    // ammunition that shows
    EXPECT_EQ(GetField(player, "items").AsFloat(), 4096.0f + 1.0f + 256.0f);
    EXPECT_EQ(GetField(player, "ammo_shells").AsFloat(), 25.0f);
    EXPECT_EQ(GetField(player, "currentammo").AsFloat(), 25.0f);

    // the player stands, which the game code says with a statement of
    // State: a frame, and the function to think with a tenth of a second on
    EXPECT_EQ(NameOfFunction(GetField(player, "think").AsInteger()), "player_stand1");
    EXPECT_FLOAT_EQ(GetField(player, "nextthink").AsFloat(), 1.1f);
    EXPECT_THAT(_printed, HasSubstr("player joined the server"));
    EXPECT_EQ(GetField(player, "frame").AsFloat(), 12.0f);
    EXPECT_EQ(_machine->GetString(GetField(player, "weaponmodel").AsInteger()), "progs/v_shot.mdl");
  }

  TEST_F(QcMachineRealDataTest, KillsEveryMonsterOfALevel)
  {
    ASSERT_TRUE(StartLevel("lq_e1m1"));
    ConnectPlayer();

    // the monsters find their feet in their first thought
    float time = 1.0f;
    for (int frame = 0; frame < 5; frame++) { RunFrame(time += 0.1f); }
    EXPECT_EQ(_machine->GetFloat(GlobalAt("killed_monsters")), 0.0f);

    DamageEveryEntity();
    EXPECT_EQ(_machine->GetFloat(GlobalAt("killed_monsters")), _machine->GetFloat(GlobalAt("total_monsters")));

    // dying is a row of frames, one statement of State after another
    for (int frame = 0; frame < 30; frame++) { RunFrame(time += 0.1f); }
    std::size_t dead = 0;
    for (std::int32_t entity = player + 1; entity < _machine->GetEntityCount(); entity++)
    {
      const std::string_view classname = _machine->GetString(GetField(entity, "classname").AsInteger());
      if (_machine->IsEntityFree(entity) || !classname.starts_with("monster_")) { continue; }

      // on the ground for good: it takes no more damage and thinks no more
      EXPECT_EQ(GetField(entity, "takedamage").AsFloat(), 0.0f) << classname << " " << entity;
      EXPECT_LE(GetField(entity, "nextthink").AsFloat(), 0.0f) << classname << " " << entity;
      EXPECT_LE(GetField(entity, "health").AsFloat(), 0.0f) << classname << " " << entity;
      dead++;
    }
    EXPECT_EQ(dead, 72u);
    EXPECT_THAT(_stops, IsEmpty());
    EXPECT_THAT(_game_errors, IsEmpty());
  }

  TEST_F(QcMachineRealDataTest, PlaysEveryLevelOfTheRealGameWithoutAnErrorOfTheMachine)
  {
    std::int64_t statements = 0;
    std::size_t levels = 0;
    for (const std::string &path : RealData::Get().ListNames("maps"))
    {
      // the models of the boxes of ammunition are in the same folder and
      // format, and are no levels
      const std::string_view file = std::string_view(path).substr(std::string_view("maps/").size());
      if (!file.ends_with(".bsp") || file.starts_with("b_")) { continue; }
      const std::string_view level = file.substr(0, file.size() - std::string_view(".bsp").size());

      MakeMachine();
      if (!StartLevel(level)) { continue; }
      ConnectPlayer();

      // The player asks for every weapon, takes each in turn, and holds the
      // trigger and jumps every other frame, while everything thinks. Half
      // way through it touches and uses everything, and near the end it
      // kills what can die, which then has frames left to die in.
      float time = 1.0f;
      for (int frame = 0; frame < 120; frame++)
      {
        time += 0.1f;
        const float impulse = frame == 0 ? 9.0f : static_cast<float>(frame % 10);
        SetField(player, "impulse", {QcCell::OfFloat(impulse)});
        SetField(player, "button0", {QcCell::OfFloat(frame % 2 == 0 ? 1.0f : 0.0f)});
        SetField(player, "button2", {QcCell::OfFloat(frame % 2 == 0 ? 0.0f : 1.0f)});
        RunFrame(time);

        if (frame == 40) { RunFieldOfEveryEntity("touch"); }
        if (frame == 60) { RunFieldOfEveryEntity("use"); }
        if (frame == 80) { DamageEveryEntity(); }
      }

      statements += _machine->GetStatementsRun();
      levels++;
    }
    ASSERT_GT(levels, 0u);

    std::cout << levels << " levels, " << _spawned.size() << " kinds of entities, " << statements
              << " statements.\nKinds:";
    for (const auto &[kind, count] : _spawned) { std::cout << " " << kind << " " << count << ","; }
    std::cout << "\nClassnames without a function:";
    for (const std::string &classname : _without) { std::cout << " \"" << classname << "\""; }
    std::cout << "\nBuiltins called:";
    for (const auto &[number, count] : _builtin_calls)
    {
      std::cout << " " << quake::QcBuiltinName(number) << " " << count << ",";
    }
    std::cout << "\nRuns the machine stopped:\n";
    for (const auto &[stop, count] : _stops) { std::cout << "  " << count << " times: " << stop << "\n"; }
    std::cout << "Errors the game code raised itself:\n";
    for (const auto &[stop, count] : _game_errors) { std::cout << "  " << count << " times: " << stop << "\n"; }
    std::cout << "What the game code printed, the first of it:\n" << _printed.substr(0, 4000) << "\n";

    EXPECT_THAT(_stops, IsEmpty());
  }
}
