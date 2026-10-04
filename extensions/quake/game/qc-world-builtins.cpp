#include "qc-world-builtins.hpp"

#include <algorithm>
#include <utility>

#include "level-trace-kind.hpp"
#include "level-vector.hpp"
#include "qc-builtin-number.hpp"
#include "qc-flag.hpp"
#include "qc-solid.hpp"

namespace quake
{
  QcWorldBuiltins::QcWorldBuiltins(LevelCollision &collision, LevelStepping &stepping)
    : _collision(collision),
      _stepping(stepping),
      _fields(collision.GetMachine().GetProgs()),
      _globals(collision.GetMachine().GetProgs())
  {
  }

  void QcWorldBuiltins::SetClientCount(const std::int32_t count)
  {
    _client_count = std::max(count, 0);
  }

  std::int32_t QcWorldBuiltins::GetClientCount() const
  {
    return _client_count;
  }

  void QcWorldBuiltins::Register(QcMachine &machine)
  {
    const auto set = [&machine](const QcBuiltinNumber number, QcMachine::Builtin builtin)
    {
      machine.SetBuiltin(static_cast<std::int32_t>(number), std::move(builtin));
    };
    using Number = QcBuiltinNumber;

    set(Number::TraceLine, [this](QcMachine &m) { TraceLine(m); });
    set(Number::PointContents, [this](QcMachine &m)
    {
      m.SetReturnFloat(static_cast<float>(_collision.GetPointContents(m.GetParameterVector(0))));
    });
    set(Number::DropToFloor, [this](QcMachine &m) { DropToFloor(m); });
    set(Number::FindRadius, [this](QcMachine &m) { FindRadius(m); });
    set(Number::CheckClient, [this](QcMachine &m) { CheckClient(m); });
    set(Number::CheckBottom, [this](QcMachine &m)
    {
      m.SetReturnFloat(_stepping.CheckBottom(m.GetParameterInteger(0)) ? 1.0f : 0.0f);
    });
    set(Number::WalkMove, [this](QcMachine &m) { WalkMove(m); });
    set(Number::MoveToGoal, [this](QcMachine &m) { MoveToGoal(m); });
    set(Number::ChangeYaw, [this](QcMachine &m) { _stepping.ChangeYaw(_globals.self.Get(m)); });
    set(Number::Aim, [this](QcMachine &m) { m.SetReturnVector(_globals.v_forward.Get(m)); });
  }

  void QcWorldBuiltins::SetTraceGlobals(QcMachine &machine, const LevelTraceResult &trace) const
  {
    _globals.trace_allsolid.Set(machine, trace.all_solid ? 1.0f : 0.0f);
    _globals.trace_startsolid.Set(machine, trace.start_solid ? 1.0f : 0.0f);
    _globals.trace_fraction.Set(machine, trace.fraction);
    _globals.trace_inwater.Set(machine, trace.in_water ? 1.0f : 0.0f);
    _globals.trace_inopen.Set(machine, trace.in_open ? 1.0f : 0.0f);
    _globals.trace_endpos.Set(machine, trace.end_position);
    _globals.trace_plane_normal.Set(machine, trace.plane_normal);
    _globals.trace_plane_dist.Set(machine, trace.plane_distance);
    _globals.trace_ent.Set(machine, trace.HasEntity() ? trace.entity : 0);
  }

  void QcWorldBuiltins::TraceLine(QcMachine &machine) const
  {
    // any other number than the two is a move that everything stops
    const float number = machine.GetParameterFloat(2);
    LevelTraceKind kind = LevelTraceKind::Normal;
    if (number == static_cast<float>(LevelTraceKind::NoMonsters)) { kind = LevelTraceKind::NoMonsters; }
    if (number == static_cast<float>(LevelTraceKind::Missile)) { kind = LevelTraceKind::Missile; }

    SetTraceGlobals(machine, _collision.Trace(
      machine.GetParameterVector(0), {}, {}, machine.GetParameterVector(1), kind, machine.GetParameterInteger(3)));
  }

  void QcWorldBuiltins::DropToFloor(QcMachine &machine) const
  {
    const std::int32_t self = _globals.self.Get(machine);
    const LevelVector origin = _fields.origin.Get(machine, self);
    const LevelVector end = {origin[0], origin[1], origin[2] - drop_distance};

    const LevelTraceResult trace = _collision.Trace(
      origin, _fields.mins.Get(machine, self), _fields.maxs.Get(machine, self), end, LevelTraceKind::Normal, self);
    if (trace.fraction == 1.0f || trace.all_solid)
    {
      machine.SetReturnFloat(0.0f);
      return;
    }

    _fields.origin.Set(machine, self, trace.end_position);
    _collision.Link(self);
    _fields.flags.Set(machine, self, WithFlag(_fields.flags.Get(machine, self), QcFlag::OnGround));
    _fields.groundentity.Set(machine, self, trace.entity);
    machine.SetReturnFloat(1.0f);
  }

  void QcWorldBuiltins::FindRadius(QcMachine &machine) const
  {
    const LevelVector place = machine.GetParameterVector(0);
    const float radius = machine.GetParameterFloat(1);

    // each one found names the one found before it
    std::int32_t chain = 0;
    for (std::int32_t entity = 1; entity < machine.GetEntityCount(); entity++)
    {
      if (machine.IsEntityFree(entity)) { continue; }
      if (_fields.solid.Get(machine, entity) == static_cast<float>(QcSolid::Not)) { continue; }

      // by the middle of its box, not by its origin
      const LevelVector middle = Sum(
        _fields.origin.Get(machine, entity),
        Scaled(Sum(_fields.mins.Get(machine, entity), _fields.maxs.Get(machine, entity)), 0.5f));
      if (Length(Difference(place, middle)) > radius) { continue; }

      _fields.chain.Set(machine, entity, chain);
      chain = entity;
    }
    machine.SetReturnInteger(chain);
  }

  void QcWorldBuiltins::WalkMove(QcMachine &machine) const
  {
    // read before the step: a trigger touched on the way runs game code,
    // which takes the places of the parameters
    const std::int32_t self = _globals.self.Get(machine);
    const float yaw = machine.GetParameterFloat(0);
    const float distance = machine.GetParameterFloat(1);

    const bool stepped = _stepping.WalkMove(self, yaw, distance);
    machine.SetReturnFloat(stepped ? 1.0f : 0.0f);
  }

  void QcWorldBuiltins::MoveToGoal(QcMachine &machine) const
  {
    const std::int32_t self = _globals.self.Get(machine);
    const float distance = machine.GetParameterFloat(0);

    _stepping.MoveToGoal(self, distance);
  }

  void QcWorldBuiltins::CheckClient(QcMachine &machine) const
  {
    machine.SetReturnInteger(0);
    if (_client_count <= 0) { return; }

    // whose turn it is moves on every tenth of a second
    const auto turn = static_cast<std::int64_t>(std::max(_globals.time.Get(machine), 0.0f) * 10.0f);
    for (std::int32_t i = 0; i < _client_count; i++)
    {
      const auto client = static_cast<std::int32_t>((turn + i) % _client_count) + 1;
      if (machine.IsEntityFree(client)) { continue; }
      if (_fields.health.Get(machine, client) <= 0.0f) { continue; }
      if (HasFlag(_fields.flags.Get(machine, client), QcFlag::NoTarget)) { continue; }

      machine.SetReturnInteger(client);
      return;
    }
  }
} // quake
