#include "level-touching.hpp"

#include "level-box.hpp"
#include "qc-solid.hpp"

namespace quake
{
  LevelTouching::LevelTouching(LevelCollision &collision, LevelCaller &caller)
    : _collision(collision),
      _caller(caller),
      _machine(collision.GetMachine()),
      _fields(_machine.GetProgs()),
      _globals(_machine.GetProgs())
  {
  }

  void LevelTouching::Call(const std::int32_t function, const std::int32_t self, const std::int32_t other) const
  {
    if (function == 0) { return; }

    const std::int32_t self_before = _globals.self.Get(_machine);
    const std::int32_t other_before = _globals.other.Get(_machine);
    _caller.RunFunction(function, self, other);
    _globals.self.Set(_machine, self_before);
    _globals.other.Set(_machine, other_before);
  }

  void LevelTouching::Impact(const std::int32_t first, const std::int32_t second) const
  {
    const auto tell = [this](const std::int32_t self, const std::int32_t other)
    {
      if (_fields.solid.Get(_machine, self) == static_cast<float>(QcSolid::Not)) { return; }

      Call(_fields.touch.Get(_machine, self), self, other);
    };
    tell(first, second);
    tell(second, first);
  }

  void LevelTouching::TouchTriggers(const std::int32_t entity) const
  {
    if (entity == 0) { return; }

    // the count is asked every time round: a touch may make entities
    for (std::int32_t trigger = 1; trigger < _machine.GetEntityCount(); trigger++)
    {
      // a touch may have removed the one that moved
      if (_machine.IsEntityFree(entity)) { return; }
      if (trigger == entity || _machine.IsEntityFree(trigger)) { continue; }
      if (_fields.solid.Get(_machine, trigger) != static_cast<float>(QcSolid::Trigger)) { continue; }

      const std::int32_t touch = _fields.touch.Get(_machine, trigger);
      if (touch == 0) { continue; }

      // by the boxes as they are now: a touch before may have moved it
      if (!_collision.GetBox(entity).Overlaps(_collision.GetBox(trigger))) { continue; }

      Call(touch, trigger, entity);
    }
  }

  void LevelTouching::Link(const std::int32_t entity, const bool touch_triggers) const
  {
    _collision.Link(entity);
    if (touch_triggers && !_machine.IsEntityFree(entity)) { TouchTriggers(entity); }
  }

  void LevelTouching::Block(const std::int32_t pusher, const std::int32_t obstacle) const
  {
    Call(_fields.blocked.Get(_machine, pusher), pusher, obstacle);
  }
} // quake
