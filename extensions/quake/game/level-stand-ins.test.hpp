#ifndef QUAKE_LEVEL_STAND_INS_TEST_HPP
#define QUAKE_LEVEL_STAND_INS_TEST_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "formats/bsp-file.hpp"
#include "formats/progs-function.hpp"
#include "qc-builtin-number.hpp"
#include "formats/qc-machine.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// Builtins that stand in for an engine, for the tests that start and run
  /// a level with the game code of a real game. They do as little as lets
  /// the code run: the world is empty space with a floor wherever one is
  /// asked for, every file is there, and one player is seen by every
  /// monster. They are not the builtins of the game.
  class LevelStandIns final
  {
    using Vector = std::array<float, 3>;

    static constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;

    QcGlobals _globals;
    QcFields _fields;

    /// The level, for the sizes of its doors and lifts.
    const BspFile &_level;

    std::map<std::string_view, QcMachine::Builtin> _stand_ins;

    static float Length(const Vector &vector)
    {
      return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
    }

    /// The angle around the up axis a direction points at, in whole degrees
    /// from 0 to 359.
    static float YawOf(const Vector &direction)
    {
      if (direction[0] == 0.0f && direction[1] == 0.0f) { return 0.0f; }

      const float yaw = std::trunc(std::atan2(direction[1], direction[0]) / degrees_to_radians);
      return yaw < 0.0f ? yaw + 360.0f : yaw;
    }

    /// The angle a direction points up by, in whole degrees from 0 to 359.
    static float PitchOf(const Vector &direction)
    {
      if (direction[0] == 0.0f && direction[1] == 0.0f) { return direction[2] > 0.0f ? 90.0f : 270.0f; }

      const float flat = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
      const float pitch = std::trunc(std::atan2(direction[2], flat) / degrees_to_radians);
      return pitch < 0.0f ? pitch + 360.0f : pitch;
    }

    void SetSize(QcMachine &machine, const std::int32_t entity, const Vector &mins, const Vector &maxs) const
    {
      _fields.mins.Set(machine, entity, mins);
      _fields.maxs.Set(machine, entity, maxs);
      _fields.size.Set(machine, entity, {maxs[0] - mins[0], maxs[1] - mins[1], maxs[2] - mins[2]});
    }

    /// Frees an entity with the fields the engine of the original resets,
    /// so that it shows nothing and thinks no more.
    void Remove(QcMachine &machine, const std::int32_t entity) const
    {
      if (!machine.FreeEntity(entity)) { return; }

      _fields.model.Set(machine, entity, 0);
      for (const QcFields::Float *field : {
             &_fields.takedamage, &_fields.modelindex, &_fields.colormap, &_fields.skin, &_fields.frame,
             &_fields.solid,
           })
      {
        field->Set(machine, entity, 0.0f);
      }
      _fields.origin.Set(machine, entity, {});
      _fields.angles.Set(machine, entity, {});
      _fields.nextthink.Set(machine, entity, -1.0f);
    }

    void MakeEntities()
    {
      _stand_ins["spawn"] = [](QcMachine &machine)
      {
        const std::optional<std::int32_t> entity = machine.CreateEntity();
        if (!entity) { return machine.Stop("spawn: there is no room for another entity"); }
        machine.SetReturnInteger(*entity);
      };
      _stand_ins["remove"] = [this](QcMachine &machine) { Remove(machine, machine.GetParameterInteger(0)); };
      _stand_ins["setmodel"] = [this](QcMachine &machine)
      {
        const std::int32_t entity = machine.GetParameterInteger(0);
        _fields.model.Set(machine, entity, machine.GetParameterInteger(1));

        // a part of the level, `*1`, `*2`, and so on, gives the entity its
        // size, by which the game code tells which doors are one door
        const std::string name(machine.GetParameterString(1));
        if (!name.starts_with('*')) { return; }
        const auto model = static_cast<std::size_t>(std::atoi(name.c_str() + 1));
        if (model >= _level.models.size()) { return; }

        const BspModel &part = _level.models[model];
        SetSize(machine, entity, {part.mins.x, part.mins.y, part.mins.z}, {part.maxs.x, part.maxs.y, part.maxs.z});
      };
      _stand_ins["setsize"] = [this](QcMachine &machine)
      {
        SetSize(machine, machine.GetParameterInteger(0), machine.GetParameterVector(1),
                machine.GetParameterVector(2));
      };
      _stand_ins["setorigin"] = [this](QcMachine &machine)
      {
        _fields.origin.Set(machine, machine.GetParameterInteger(0), machine.GetParameterVector(1));
      };
      _stand_ins["nextent"] = [](QcMachine &machine)
      {
        std::int32_t entity = machine.GetParameterInteger(0) + 1;
        while (entity < machine.GetEntityCount() && machine.IsEntityFree(entity)) { entity++; }
        machine.SetReturnInteger(entity > 0 && entity < machine.GetEntityCount() ? entity : 0);
      };
      _stand_ins["find"] = [](QcMachine &machine)
      {
        const std::int32_t field = machine.GetParameterInteger(1);
        const std::string_view wanted = machine.GetParameterString(2);
        machine.SetReturnInteger(0);
        for (std::int32_t entity = std::max(machine.GetParameterInteger(0), -1) + 1;
          entity < machine.GetEntityCount(); entity++)
        {
          const std::span<QcCell> cells = machine.GetEntity(entity);
          if (machine.IsEntityFree(entity) || field < 0 || static_cast<std::size_t>(field) >= cells.size())
          {
            continue;
          }
          if (machine.GetString(cells[static_cast<std::size_t>(field)].AsInteger()) == wanted)
          {
            machine.SetReturnInteger(entity);
            return;
          }
        }
      };
      // the entities near a place, each naming the next in its `chain`
      _stand_ins["findradius"] = [this](QcMachine &machine)
      {
        const Vector place = machine.GetParameterVector(0);
        const float radius = machine.GetParameterFloat(1);
        std::int32_t chain = 0;
        for (std::int32_t entity = 1; entity < machine.GetEntityCount(); entity++)
        {
          if (machine.IsEntityFree(entity) || _fields.solid.Get(machine, entity) == 0.0f) { continue; }

          const Vector origin = _fields.origin.Get(machine, entity);
          if (Length({origin[0] - place[0], origin[1] - place[1], origin[2] - place[2]}) > radius) { continue; }

          _fields.chain.Set(machine, entity, chain);
          chain = entity;
        }
        machine.SetReturnInteger(chain);
      };
      _stand_ins["checkclient"] = [](QcMachine &machine) { machine.SetReturnInteger(player); };
    }

    void MakeWorld()
    {
      // a line hits nothing on its way
      _stand_ins["traceline"] = [this](QcMachine &machine)
      {
        _globals.trace_allsolid.Set(machine, 0.0f);
        _globals.trace_startsolid.Set(machine, 0.0f);
        _globals.trace_fraction.Set(machine, 1.0f);
        _globals.trace_endpos.Set(machine, machine.GetParameterVector(1));
        _globals.trace_ent.Set(machine, 0);
        _globals.trace_inopen.Set(machine, 1.0f);
        _globals.trace_inwater.Set(machine, 0.0f);
      };
      const auto return_true = [](QcMachine &machine) { machine.SetReturnFloat(1.0f); };
      for (const std::string_view name : {"walkmove", "droptofloor", "checkbottom"}) { _stand_ins[name] = return_true; }
      // the number the game has for empty space
      _stand_ins["pointcontents"] = [](QcMachine &machine) { machine.SetReturnFloat(-1.0f); };
      _stand_ins["aim"] = [this](QcMachine &machine) { machine.SetReturnVector(_globals.v_forward.Get(machine)); };

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
        _stand_ins[name] = return_parameter;
      }
    }

    void MakeText()
    {
      _stand_ins["dprint"] = [this](QcMachine &machine) { printed += machine.GetParameterString(0); };
      _stand_ins["bprint"] = [this](QcMachine &machine) { printed += machine.GetParameterString(0); };
      _stand_ins["sprint"] = [this](QcMachine &machine) { printed += machine.GetParameterString(1); };
      _stand_ins["ftos"] = [](QcMachine &machine)
      {
        const float value = machine.GetParameterFloat(0);
        machine.SetReturnString(
          value == std::trunc(value) ? std::format("{:.0f}", value) : std::format("{:5.1f}", value));
      };
      _stand_ins["vtos"] = [](QcMachine &machine)
      {
        const Vector vector = machine.GetParameterVector(0);
        machine.SetReturnString(std::format("'{:5.1f} {:5.1f} {:5.1f}'", vector[0], vector[1], vector[2]));
      };

      // An error of the game code ends the run, as in the original, with a
      // message that tells it from an error of the machine.
      _stand_ins["error"] = [](QcMachine &machine)
      {
        machine.Stop(std::format("{}{}", game_error, machine.GetParameterString(0)));
      };
      // An error of one entity: the engines the real game is made for take
      // the entity away and go on, and its levels count on that.
      _stand_ins["objerror"] = [this](QcMachine &machine)
      {
        object_errors.emplace_back(machine.GetParameterString(0));
        Remove(machine, _globals.self.Get(machine));
      };
    }

    void MakeNumbers()
    {
      _stand_ins["random"] = [](QcMachine &machine) { machine.SetReturnFloat(0.5f); };
      _stand_ins["cvar"] = [this](QcMachine &machine)
      {
        machine.SetReturnFloat(machine.GetParameterString(0) == "skill" ? skill : 0.0f);
      };
      _stand_ins["rint"] = [](QcMachine &machine) { machine.SetReturnFloat(std::round(machine.GetParameterFloat(0))); };
      _stand_ins["floor"] = [](QcMachine &machine) { machine.SetReturnFloat(std::floor(machine.GetParameterFloat(0))); };
      _stand_ins["ceil"] = [](QcMachine &machine) { machine.SetReturnFloat(std::ceil(machine.GetParameterFloat(0))); };
      _stand_ins["fabs"] = [](QcMachine &machine) { machine.SetReturnFloat(std::fabs(machine.GetParameterFloat(0))); };
      _stand_ins["vlen"] = [](QcMachine &machine) { machine.SetReturnFloat(Length(machine.GetParameterVector(0))); };
      _stand_ins["normalize"] = [](QcMachine &machine)
      {
        const Vector vector = machine.GetParameterVector(0);
        const float length = Length(vector);
        machine.SetReturnVector(
          length == 0.0f ? Vector{} : Vector{vector[0] / length, vector[1] / length, vector[2] / length});
      };
      _stand_ins["vectoyaw"] = [](QcMachine &machine) { machine.SetReturnFloat(YawOf(machine.GetParameterVector(0))); };
      _stand_ins["vectoangles"] = [](QcMachine &machine)
      {
        const Vector direction = machine.GetParameterVector(0);
        machine.SetReturnVector({PitchOf(direction), YawOf(direction), 0.0f});
      };
      // The angles are pitch, yaw, and roll in degrees. Forward is where
      // they look, with a positive pitch looking down.
      _stand_ins["makevectors"] = [this](QcMachine &machine)
      {
        const Vector angles = machine.GetParameterVector(0);
        const float sin_pitch = std::sin(angles[0] * degrees_to_radians);
        const float cos_pitch = std::cos(angles[0] * degrees_to_radians);
        const float sin_yaw = std::sin(angles[1] * degrees_to_radians);
        const float cos_yaw = std::cos(angles[1] * degrees_to_radians);
        const float sin_roll = std::sin(angles[2] * degrees_to_radians);
        const float cos_roll = std::cos(angles[2] * degrees_to_radians);

        _globals.v_forward.Set(machine, {cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch});
        _globals.v_right.Set(machine, {
                               cos_roll * sin_yaw - sin_roll * sin_pitch * cos_yaw,
                               -cos_roll * cos_yaw - sin_roll * sin_pitch * sin_yaw,
                               -sin_roll * cos_pitch,
                             });
        _globals.v_up.Set(machine, {
                            cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw,
                            cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw,
                            cos_roll * cos_pitch,
                          });
      };
    }

  public:
    /// The entity of the only player, whom every monster sees.
    static constexpr std::int32_t player = 1;

    /// What the message of an error starts with that the game code raised
    /// itself with `error`.
    static constexpr std::string_view game_error = "The game code says: ";

    /// What the game code is told the skill is.
    float skill = 1.0f;

    /// Everything the game code printed.
    std::string printed;

    /// What the game code said of the entities it gave up on with
    /// `objerror`.
    std::vector<std::string> object_errors;

    /// Registers a builtin for every number the game code names: the
    /// stand-in of its name, or one that does nothing. The machine must not
    /// outlive this, nor this the level.
    LevelStandIns(QcMachine &machine, const BspFile &level)
      : _globals(machine.GetProgs()), _fields(machine.GetProgs()), _level(level)
    {
      MakeEntities();
      MakeWorld();
      MakeText();
      MakeNumbers();

      for (const ProgsFunction &function : machine.GetProgs().functions)
      {
        if (!function.IsBuiltin()) { continue; }

        const std::int32_t number = -function.first_statement;
        const auto stand_in = _stand_ins.find(QcBuiltinName(number));
        machine.SetBuiltin(number, stand_in == _stand_ins.end() ? [](QcMachine &) {} : stand_in->second);
      }
    }

    LevelStandIns(const LevelStandIns &) = delete;

    LevelStandIns &operator=(const LevelStandIns &) = delete;
  };
} // quake

#endif //QUAKE_LEVEL_STAND_INS_TEST_HPP
