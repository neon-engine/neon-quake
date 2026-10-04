#include "level-running.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

#include "formats/progs-function.hpp"
#include "qc-move-type.hpp"

namespace quake
{
  LevelRunning::LevelRunning(QcMachine &machine, LevelMover *mover)
    : _machine(machine), _globals(machine.GetProgs()), _fields(machine.GetProgs()), _mover(mover)
  {
  }

  void LevelRunning::SetMover(LevelMover *mover)
  {
    _mover = mover;
  }

  double LevelRunning::GetTime() const
  {
    return _time;
  }

  void LevelRunning::SetTime(const double time)
  {
    _time = time;
  }

  bool LevelRunning::Run(
    const std::int32_t function, const std::int32_t self, const std::int32_t other, const float time)
  {
    _globals.time.Set(_machine, time);
    _globals.self.Set(_machine, self);
    _globals.other.Set(_machine, other);
    if (_machine.Call(function)) { return true; }

    _failure_count++;
    if (_failures.size() < max_failures_kept)
    {
      const Progs &progs = _machine.GetProgs();
      const bool is_function = function > 0 && static_cast<std::size_t>(function) < progs.functions.size();
      _failures.push_back({
        .entity = self,
        .classname = std::string(_fields.classname.GetText(_machine, self)),
        .function = is_function
                      ? std::string(progs.GetString(progs.functions[static_cast<std::size_t>(function)].name))
                      : std::string(),
        .error = _machine.GetError(),
      });
    }
    return false;
  }

  bool LevelRunning::RunNamed(
    const QcGlobals::Function &place, const std::string_view name, const std::int32_t self)
  {
    // A compiler may leave the global of a function out and keep the
    // function, which is then found by its name.
    std::int32_t function = place.Get(_machine);
    if (!place.IsFound()) { function = _machine.GetProgs().FindFunction(name).value_or(0); }
    if (function == 0) { return false; }

    return Run(function, self, 0, static_cast<float>(_time));
  }

  bool LevelRunning::RunFunction(const std::int32_t function, const std::int32_t self, const std::int32_t other)
  {
    if (function == 0) { return true; }

    return Run(function, self, other, static_cast<float>(_time));
  }

  void LevelRunning::Push(const std::int32_t entity, const float dt)
  {
    const std::array<float, 3> velocity = _fields.velocity.Get(_machine, entity);
    const std::array<float, 3> turning = _fields.avelocity.Get(_machine, entity);
    const float clock = _fields.ltime.Get(_machine, entity);

    // what stands still only has its clock run
    const auto is_zero = [](const std::array<float, 3> &vector)
    {
      return vector[0] == 0.0f && vector[1] == 0.0f && vector[2] == 0.0f;
    };
    if (is_zero(velocity) && is_zero(turning))
    {
      _fields.ltime.Set(_machine, entity, clock + dt);
      return;
    }

    std::array<float, 3> origin = _fields.origin.Get(_machine, entity);
    std::array<float, 3> angles = _fields.angles.Get(_machine, entity);
    for (std::size_t i = 0; i < origin.size(); i++)
    {
      origin[i] += velocity[i] * dt;
      angles[i] += turning[i] * dt;
    }

    // blocked: it stays, and its clock with it
    if (_mover != nullptr && !_mover->MovePusher(entity, origin, angles, dt)) { return; }

    _fields.origin.Set(_machine, entity, origin);
    _fields.angles.Set(_machine, entity, angles);
    _fields.ltime.Set(_machine, entity, clock + dt);
  }

  void LevelRunning::AdvancePusher(const std::int32_t entity, const float dt)
  {
    const float clock_before = _fields.ltime.Get(_machine, entity);
    const float think_time = _fields.nextthink.Get(_machine, entity);

    // It moves no further than to the time it thinks at: a door thinks to
    // stop, at the moment it has reached where it goes.
    float move_time = dt;
    if (think_time < clock_before + dt) { move_time = std::max(think_time - clock_before, 0.0f); }
    if (move_time > 0.0f) { Push(entity, move_time); }

    if (think_time <= clock_before || think_time > _fields.ltime.Get(_machine, entity)) { return; }

    // forgotten before it thinks, so that it thinks again only when it asks
    _fields.nextthink.Set(_machine, entity, 0.0f);
    RunFunction(_fields.think.Get(_machine, entity), entity, 0);
  }

  bool LevelRunning::AdvanceThinker(const std::int32_t entity, const float dt)
  {
    const float think_time = _fields.nextthink.Get(_machine, entity);
    const auto now = static_cast<float>(_time);
    if (think_time <= 0.0f || think_time > now + dt) { return true; }

    // The game code sees the time it asked for, not the start of the frame,
    // so that what it does every tenth of a second does not drift. A time
    // that is past already is now.
    _fields.nextthink.Set(_machine, entity, 0.0f);
    const std::int32_t think = _fields.think.Get(_machine, entity);
    if (think != 0) { Run(think, entity, 0, std::max(think_time, now)); }
    return !_machine.IsEntityFree(entity);
  }

  void LevelRunning::Advance(const float dt)
  {
    _globals.frametime.Set(_machine, dt);
    RunNamed(_globals.StartFrame, "StartFrame", 0);

    // the count is asked every time round: a think may make entities
    for (std::int32_t entity = 0; entity < _machine.GetEntityCount(); entity++)
    {
      if (_machine.IsEntityFree(entity)) { continue; }

      if (_fields.movetype.Get(_machine, entity) == static_cast<float>(QcMoveType::Push))
      {
        AdvancePusher(entity, dt);
        continue;
      }

      if (AdvanceThinker(entity, dt) && _mover != nullptr) { _mover->MoveEntity(entity, dt); }
    }

    // the game code asks for everything to touch its triggers anew for a
    // number of frames, which are counted down here
    const float retouch = _globals.force_retouch.Get(_machine);
    if (retouch > 0.0f) { _globals.force_retouch.Set(_machine, retouch - 1.0f); }

    _time += dt;
    _globals.time.Set(_machine, static_cast<float>(_time));
  }

  bool LevelRunning::ConnectClient(const std::int32_t entity, const std::string_view name)
  {
    if (_machine.IsEntityFree(entity)) { return false; }

    if (!name.empty()) { _fields.netname.SetText(_machine, entity, name); }

    // SetNewParms leaves the numbers in the globals `parm1` to `parm16`,
    // where PutClientInServer reads them.
    return RunNamed(_globals.SetNewParms, "SetNewParms", entity) &&
           RunNamed(_globals.ClientConnect, "ClientConnect", entity) &&
           RunNamed(_globals.PutClientInServer, "PutClientInServer", entity);
  }

  bool LevelRunning::RunClientThink(const std::int32_t entity, const ClientThink moment)
  {
    if (_machine.IsEntityFree(entity)) { return false; }

    return moment == ClientThink::Before
             ? RunNamed(_globals.PlayerPreThink, "PlayerPreThink", entity)
             : RunNamed(_globals.PlayerPostThink, "PlayerPostThink", entity);
  }

  const std::vector<LevelFailure> &LevelRunning::GetFailures() const
  {
    return _failures;
  }

  std::size_t LevelRunning::GetFailureCount() const
  {
    return _failure_count;
  }

  void LevelRunning::ClearFailures()
  {
    _failures.clear();
    _failure_count = 0;
  }
} // quake
