#include "qc-core-builtins.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <numbers>
#include <optional>
#include <span>
#include <utility>

#include "formats/progs-definition.hpp"
#include "qc-builtin-number.hpp"
#include "qc-message-destination.hpp"
#include "qc-message-value.hpp"

namespace quake
{
  // The arithmetic and the text of the builtins, which need nothing of the
  // object.
  namespace
  {
    using Vector = std::array<float, 3>;

    constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;
    constexpr double radians_to_degrees = 180.0 / std::numbers::pi;

    float Length(const Vector &vector)
    {
      return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
    }

    /// An angle from two sides in whole degrees from 0 to 359. The part
    /// after the point is cut off and not rounded, as the original does, and
    /// game code compares what it gets with whole numbers.
    float WholeDegrees(const float opposite, const float adjacent)
    {
      // in doubles, so that 45 degrees are not a hair less and cut to 44
      const double degrees = std::trunc(
        std::atan2(static_cast<double>(opposite), static_cast<double>(adjacent)) * radians_to_degrees);
      return static_cast<float>(degrees < 0.0 ? degrees + 360.0 : degrees);
    }

    /// The angle around the up axis a direction points at.
    float YawOf(const Vector &direction)
    {
      if (direction[0] == 0.0f && direction[1] == 0.0f) { return 0.0f; }

      return WholeDegrees(direction[1], direction[0]);
    }

    /// The angle a direction points up by. Straight up is 90 and straight
    /// down 270.
    float PitchOf(const Vector &direction)
    {
      if (direction[0] == 0.0f && direction[1] == 0.0f) { return direction[2] > 0.0f ? 90.0f : 270.0f; }

      const float flat = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
      return WholeDegrees(direction[2], flat);
    }

    /// A number as `ftos` writes it: without a fraction when it has none,
    /// and with one digit of it otherwise.
    std::string TextOf(const float value)
    {
      // adding zero makes a minus zero a zero, which is written without sign
      return value == std::trunc(value) ? std::format("{:.0f}", value + 0.0f) : std::format("{:5.1f}", value);
    }

    /// The parameters of a call from one on, each a string, as one text.
    /// The builtins that print take a text in as many pieces as the game
    /// code likes to pass.
    std::string JoinStrings(const QcMachine &machine, const std::int32_t first)
    {
      std::string text;
      for (std::int32_t i = first; i < machine.GetArgumentCount(); i++) { text += machine.GetParameterString(i); }
      return text;
    }

    /// Where a global of a name is, or -1.
    std::int32_t GlobalAt(const Progs &progs, const std::string_view name)
    {
      const ProgsDefinition *definition = progs.FindGlobal(name);
      return definition == nullptr ? -1 : definition->offset;
    }

    /// Where a field of a name is in an entity, or -1.
    std::int32_t FieldAt(const Progs &progs, const std::string_view name)
    {
      const ProgsDefinition *definition = progs.FindField(name);
      return definition == nullptr ? -1 : definition->offset;
    }
  }

  QcCoreBuiltins::QcCoreBuiltins(QcHost &host) : _host(host)
  {
  }

  void QcCoreBuiltins::FindPlaces(const Progs &progs)
  {
    _places.self = GlobalAt(progs, "self");
    _places.message_entity = GlobalAt(progs, "msg_entity");
    _places.forward = GlobalAt(progs, "v_forward");
    _places.right = GlobalAt(progs, "v_right");
    _places.up = GlobalAt(progs, "v_up");

    _places.model = FieldAt(progs, "model");
    _places.take_damage = FieldAt(progs, "takedamage");
    _places.model_index = FieldAt(progs, "modelindex");
    _places.color_map = FieldAt(progs, "colormap");
    _places.skin = FieldAt(progs, "skin");
    _places.frame = FieldAt(progs, "frame");
    _places.solid = FieldAt(progs, "solid");
    _places.origin = FieldAt(progs, "origin");
    _places.angles = FieldAt(progs, "angles");
    _places.next_think = FieldAt(progs, "nextthink");
  }

  void QcCoreBuiltins::SetCells(QcMachine &machine, const std::int32_t entity, const std::int32_t field,
    const std::initializer_list<QcCell> cells)
  {
    const std::span<QcCell> fields = machine.GetEntity(entity);
    if (field < 0 || static_cast<std::size_t>(field) + cells.size() > fields.size()) { return; }

    std::ranges::copy(cells, fields.begin() + field);
  }

  void QcCoreBuiltins::Register(QcMachine &machine)
  {
    FindPlaces(machine.GetProgs());

    const auto set = [&machine](const QcBuiltinNumber number, QcMachine::Builtin builtin)
    {
      machine.SetBuiltin(static_cast<std::int32_t>(number), std::move(builtin));
    };
    using Number = QcBuiltinNumber;

    // numbers and vectors
    set(Number::Random, [this](QcMachine &m) { m.SetReturnFloat(NextRandom()); });
    // a half goes away from zero, as in the original
    set(Number::RInt, [](QcMachine &m) { m.SetReturnFloat(std::round(m.GetParameterFloat(0))); });
    set(Number::Floor, [](QcMachine &m) { m.SetReturnFloat(std::floor(m.GetParameterFloat(0))); });
    set(Number::Ceil, [](QcMachine &m) { m.SetReturnFloat(std::ceil(m.GetParameterFloat(0))); });
    set(Number::FAbs, [](QcMachine &m) { m.SetReturnFloat(std::fabs(m.GetParameterFloat(0))); });
    set(Number::VLen, [](QcMachine &m) { m.SetReturnFloat(Length(m.GetParameterVector(0))); });
    set(Number::Normalize, [](QcMachine &m)
    {
      const Vector vector = m.GetParameterVector(0);
      const float length = Length(vector);
      // a vector of no length stays one, and is not divided by zero
      m.SetReturnVector(length == 0.0f ? Vector{} : Vector{vector[0] / length, vector[1] / length, vector[2] / length});
    });
    set(Number::VecToYaw, [](QcMachine &m) { m.SetReturnFloat(YawOf(m.GetParameterVector(0))); });
    set(Number::VecToAngles, [](QcMachine &m)
    {
      const Vector direction = m.GetParameterVector(0);
      m.SetReturnVector({PitchOf(direction), YawOf(direction), 0.0f});
    });
    set(Number::MakeVectors, [this](QcMachine &m) { MakeVectors(m); });

    // text
    set(Number::FToS, [](QcMachine &m) { m.SetReturnString(TextOf(m.GetParameterFloat(0))); });
    set(Number::VToS, [](QcMachine &m)
    {
      const Vector vector = m.GetParameterVector(0);
      m.SetReturnString(std::format("'{:5.1f} {:5.1f} {:5.1f}'", vector[0], vector[1], vector[2]));
    });
    set(Number::BPrint, [this](QcMachine &m) { _host.PrintToAll(JoinStrings(m, 0)); });
    set(Number::SPrint, [this](QcMachine &m) { _host.PrintToClient(m.GetParameterInteger(0), JoinStrings(m, 1)); });
    set(Number::DPrint, [this](QcMachine &m) { _host.PrintToConsole(JoinStrings(m, 0)); });
    set(Number::CenterPrint, [this](QcMachine &m)
    {
      _host.PrintToCenter(m.GetParameterInteger(0), JoinStrings(m, 1));
    });
    set(Number::EPrint, [this](QcMachine &m) { _host.PrintEntity(m.GetParameterInteger(0)); });
    set(Number::CoreDump, [this](QcMachine &m)
    {
      for (std::int32_t entity = 0; entity < m.GetEntityCount(); entity++)
      {
        if (!m.IsEntityFree(entity)) { _host.PrintEntity(entity); }
      }
    });
    set(Number::Error, [this](QcMachine &m) { RaiseError(m); });
    set(Number::ObjError, [this](QcMachine &m) { RaiseObjectError(m); });

    // What the original did for whoever had the engine in a debugger: to
    // stop there, and to print every statement as it ran. The machine has
    // neither, and the game code that calls them goes on as it would.
    const auto nothing = [](QcMachine &) {};
    set(Number::Break, nothing);
    set(Number::TraceOn, nothing);
    set(Number::TraceOff, nothing);

    // the entities
    set(Number::Spawn, [this](QcMachine &m) { Spawn(m); });
    set(Number::Remove, [this](QcMachine &m) { RemoveEntity(m, m.GetParameterInteger(0)); });
    set(Number::Find, [this](QcMachine &m) { Find(m); });
    set(Number::NextEnt, [this](QcMachine &m) { NextEntity(m); });

    // The files the game code will use. Each returns the name it was given,
    // which game code hands on to `setmodel` or `sound` in the same line.
    const auto precache = [](QcPrecacheList *list)
    {
      return [list](QcMachine &m)
      {
        if (list != nullptr) { list->Add(m.GetParameterString(0)); }
        m.SetReturnInteger(m.GetParameterInteger(0));
      };
    };
    set(Number::PrecacheModel, precache(&_models));
    set(Number::PrecacheSound, precache(&_sounds));
    // the second of each is the same: it named what only the whole game has
    set(Number::PrecacheModel2, precache(&_models));
    set(Number::PrecacheSound2, precache(&_sounds));
    // a file that is neither was named for the tool that packed the game
    set(Number::PrecacheFile, precache(nullptr));
    set(Number::PrecacheFile2, precache(nullptr));

    // the console variables
    set(Number::CVar, [this](QcMachine &m) { m.SetReturnFloat(_variables.GetFloat(m.GetParameterString(0))); });
    set(Number::CVarSet, [this](QcMachine &m)
    {
      _variables.Set(m.GetParameterString(0), m.GetParameterString(1));
    });

    set(Number::LightStyle, [this](QcMachine &m) { SetLightStyle(m); });

    // what is the host's alone to do
    set(Number::StuffCmd, [this](QcMachine &m)
    {
      _host.ClientCommand(m.GetParameterInteger(0), m.GetParameterString(1));
    });
    set(Number::LocalCmd, [this](QcMachine &m) { _host.ServerCommand(m.GetParameterString(0)); });
    set(Number::ChangeLevel, [this](QcMachine &m) { _host.ChangeLevel(m.GetParameterString(0)); });
    set(Number::SetSpawnParms, [this](QcMachine &m) { _host.SetSpawnParameters(m.GetParameterInteger(0)); });

    constexpr std::pair<QcBuiltinNumber, QcMessageKind> writes[] = {
      {Number::WriteByte, QcMessageKind::Byte},
      {Number::WriteChar, QcMessageKind::Char},
      {Number::WriteShort, QcMessageKind::Short},
      {Number::WriteLong, QcMessageKind::Long},
      {Number::WriteCoord, QcMessageKind::Coord},
      {Number::WriteAngle, QcMessageKind::Angle},
      {Number::WriteString, QcMessageKind::String},
      {Number::WriteEntity, QcMessageKind::Entity},
    };
    for (const auto &[number, kind] : writes)
    {
      set(number, [this, kind](QcMachine &m) { Write(m, kind); });
    }
  }

  void QcCoreBuiltins::MakeVectors(QcMachine &machine) const
  {
    // The angles are pitch, yaw, and roll in degrees. Forward is where they
    // look, with a positive pitch looking down; right and up are the other
    // two axes of the same turn.
    const Vector angles = machine.GetParameterVector(0);
    const float sin_pitch = std::sin(angles[0] * degrees_to_radians);
    const float cos_pitch = std::cos(angles[0] * degrees_to_radians);
    const float sin_yaw = std::sin(angles[1] * degrees_to_radians);
    const float cos_yaw = std::cos(angles[1] * degrees_to_radians);
    const float sin_roll = std::sin(angles[2] * degrees_to_radians);
    const float cos_roll = std::cos(angles[2] * degrees_to_radians);

    machine.SetVector(_places.forward, {cos_pitch * cos_yaw, cos_pitch * sin_yaw, -sin_pitch});
    machine.SetVector(_places.right, {
      cos_roll * sin_yaw - sin_roll * sin_pitch * cos_yaw,
      -cos_roll * cos_yaw - sin_roll * sin_pitch * sin_yaw,
      -sin_roll * cos_pitch,
    });
    machine.SetVector(_places.up, {
      cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw,
      cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw,
      cos_roll * cos_pitch,
    });
  }

  void QcCoreBuiltins::Find(QcMachine &machine) const
  {
    // The next entity after one whose field of a string has a text. The
    // world, which the search never gives, stands for none.
    const std::int32_t field = machine.GetParameterInteger(1);
    const std::string_view wanted = machine.GetParameterString(2);
    machine.SetReturnInteger(0);
    if (field < 0) { return; }

    for (std::int32_t entity = std::max(machine.GetParameterInteger(0), 0) + 1;
      entity < machine.GetEntityCount(); entity++)
    {
      const std::span<QcCell> fields = machine.GetEntity(entity);
      if (machine.IsEntityFree(entity) || static_cast<std::size_t>(field) >= fields.size()) { continue; }

      if (machine.GetString(fields[static_cast<std::size_t>(field)].AsInteger()) == wanted)
      {
        machine.SetReturnInteger(entity);
        return;
      }
    }
  }

  void QcCoreBuiltins::NextEntity(QcMachine &machine) const
  {
    // the next entity after one that is not free, or the world at the end
    std::int32_t entity = std::max(machine.GetParameterInteger(0), 0) + 1;
    while (entity < machine.GetEntityCount() && machine.IsEntityFree(entity)) { entity++; }
    machine.SetReturnInteger(entity < machine.GetEntityCount() ? entity : 0);
  }

  void QcCoreBuiltins::Spawn(QcMachine &machine)
  {
    const std::optional<std::int32_t> entity = machine.CreateEntity();
    if (!entity) { return machine.Stop("spawn: there is no room for another entity"); }

    machine.SetReturnInteger(*entity);
    _host.EntityMade(*entity);
  }

  bool QcCoreBuiltins::RemoveEntity(QcMachine &machine, const std::int32_t entity)
  {
    if (machine.IsEntityFree(entity) || !machine.FreeEntity(entity)) { return false; }

    // The machine leaves a free entity as it was, since game code goes on
    // with one it removed. The original takes it out of the world with
    // these fields, and has it think no more.
    const QcCell zero;
    for (const std::int32_t field : {
           _places.model, _places.take_damage, _places.model_index, _places.color_map, _places.skin, _places.frame,
           _places.solid,
         })
    {
      SetCells(machine, entity, field, {zero});
    }
    SetCells(machine, entity, _places.origin, {zero, zero, zero});
    SetCells(machine, entity, _places.angles, {zero, zero, zero});
    SetCells(machine, entity, _places.next_think, {QcCell::OfFloat(-1.0f)});

    _host.EntityRemoved(entity);
    return true;
  }

  void QcCoreBuiltins::RaiseError(QcMachine &machine)
  {
    const std::string text = JoinStrings(machine, 0);
    _host.Error(machine.GetInteger(_places.self), text);
    machine.Stop(std::format("{}{}", error_start, text));
  }

  void QcCoreBuiltins::RaiseObjectError(QcMachine &machine)
  {
    // An error of one entity ended the run in the original as well. The
    // engines that came after only take the entity away and go on, and the
    // game code of today is written for that: levels of LibreQuake raise
    // this error while they start.
    const std::int32_t self = machine.GetInteger(_places.self);
    _host.ObjectError(self, JoinStrings(machine, 0));
    RemoveEntity(machine, self);
  }

  void QcCoreBuiltins::SetLightStyle(QcMachine &machine)
  {
    // A style there is not is left out. The original wrote outside its
    // table for one.
    const float style = machine.GetParameterFloat(0);
    if (!(style >= 0.0f && style < static_cast<float>(light_style_count))) { return; }

    const auto number = static_cast<std::int32_t>(style);
    std::string &kept = _light_styles[static_cast<std::size_t>(number)];
    kept = machine.GetParameterString(1);
    _host.LightStyleSet(number, kept);
  }

  void QcCoreBuiltins::Write(QcMachine &machine, const QcMessageKind kind)
  {
    const float number = machine.GetParameterFloat(0);
    if (number != std::trunc(number) || number < 0.0f || number > 3.0f)
    {
      return machine.Stop(std::format("A message is written to destination {}, and there are only 0 to 3", number));
    }
    const auto destination = static_cast<QcMessageDestination>(static_cast<std::int32_t>(number));

    QcMessageValue value{.kind = kind};
    switch (kind)
    {
      case QcMessageKind::String:
      {
        value.text = machine.GetParameterString(1);
        break;
      }
      case QcMessageKind::Entity:
      {
        value.entity = machine.GetParameterInteger(1);
        break;
      }
      default:
      {
        value.number = machine.GetParameterFloat(1);
        break;
      }
    }

    // the game code names the one player in a global before it writes
    const std::int32_t client =
      destination == QcMessageDestination::One ? machine.GetInteger(_places.message_entity) : 0;
    _host.WriteMessage(destination, client, value);
  }

  QcConsoleVariables &QcCoreBuiltins::GetVariables()
  {
    return _variables;
  }

  const QcConsoleVariables &QcCoreBuiltins::GetVariables() const
  {
    return _variables;
  }

  QcPrecacheList &QcCoreBuiltins::GetModels()
  {
    return _models;
  }

  const QcPrecacheList &QcCoreBuiltins::GetModels() const
  {
    return _models;
  }

  QcPrecacheList &QcCoreBuiltins::GetSounds()
  {
    return _sounds;
  }

  const QcPrecacheList &QcCoreBuiltins::GetSounds() const
  {
    return _sounds;
  }

  std::string_view QcCoreBuiltins::GetLightStyle(const std::int32_t style) const
  {
    if (style < 0 || style >= light_style_count) { return {}; }

    return _light_styles[static_cast<std::size_t>(style)];
  }

  bool QcCoreBuiltins::RestoreLightStyle(const std::int32_t style, const std::string_view text)
  {
    if (style < 0 || style >= light_style_count) { return false; }

    std::string &kept = _light_styles[static_cast<std::size_t>(style)];
    kept = text;
    _host.LightStyleSet(style, kept);
    return true;
  }

  void QcCoreBuiltins::SeedRandom(const std::uint32_t seed)
  {
    _random.seed(seed);
  }

  float QcCoreBuiltins::NextRandom()
  {
    // 15 bits, as many as the original took, and half a step in from 0
    return (static_cast<float>(_random() >> 17) + 0.5f) / 32768.0f;
  }
} // quake
