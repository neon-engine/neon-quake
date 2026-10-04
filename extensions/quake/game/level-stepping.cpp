#include "level-stepping.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <utility>

#include "formats/bsp-contents.hpp"
#include "level-box.hpp"
#include "level-trace-kind.hpp"
#include "level-trace-result.hpp"
#include "qc-flag.hpp"

namespace quake
{
  // The angles of walking: a yaw as the original keeps one, and the
  // direction it points in.
  namespace
  {
    /// What stands for no direction where one is chosen.
    constexpr float no_direction = -1.0f;

    /// An angle brought into 0 to 360, in the steps of a 65536th of a turn
    /// the original keeps angles in.
    float WrapAngle(const float angle)
    {
      constexpr float steps = 65536.0f;
      return (360.0f / steps) * static_cast<float>(static_cast<std::int32_t>(angle * (steps / 360.0f)) & 65535);
    }

    /// A step of a length on the ground in the direction of a yaw.
    LevelVector StepAlong(const float yaw, const float distance)
    {
      const float radians = yaw * std::numbers::pi_v<float> * 2.0f / 360.0f;
      return {std::cos(radians) * distance, std::sin(radians) * distance, 0.0f};
    }
  }

  LevelStepping::LevelStepping(LevelCollision &collision, LevelTouching &touching, Random random)
    : _collision(collision),
      _touching(touching),
      _machine(collision.GetMachine()),
      _fields(_machine.GetProgs()),
      _random(std::move(random))
  {
  }

  bool LevelStepping::HasAnyFlag(const std::int32_t entity, const std::int32_t flags) const
  {
    return (static_cast<std::int32_t>(_fields.flags.Get(_machine, entity)) & flags) != 0;
  }

  bool LevelStepping::CanStep(const std::int32_t entity) const
  {
    return HasAnyFlag(
      entity,
      static_cast<std::int32_t>(QcFlag::OnGround) | static_cast<std::int32_t>(QcFlag::Fly) |
      static_cast<std::int32_t>(QcFlag::Swim));
  }

  bool LevelStepping::CheckBottom(const std::int32_t entity) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);
    const LevelVector mins = Sum(origin, _fields.mins.Get(_machine, entity));
    const LevelVector maxs = Sum(origin, _fields.maxs.Get(_machine, entity));
    const std::array<std::array<float, 2>, 4> corners = {{
      {mins[0], mins[1]}, {mins[0], maxs[1]}, {maxs[0], mins[1]}, {maxs[0], maxs[1]},
    }};

    // the quick answer: a unit under every corner the world is solid
    bool all_solid = true;
    for (const std::array<float, 2> &corner : corners)
    {
      if (_collision.GetPointContents({corner[0], corner[1], mins[2] - 1.0f}) != BspContents::Solid)
      {
        all_solid = false;
        break;
      }
    }
    if (all_solid) { return true; }

    // The slow one: a line down from the middle, then one from each
    // corner, each two steps deep. No corner may hang more than a step
    // above what is under the middle.
    const float top = mins[2];
    const float bottom = mins[2] - 2.0f * step_height;
    const auto look_down = [&](const float x, const float y)
    {
      return _collision.Trace({x, y, top}, {}, {}, {x, y, bottom}, LevelTraceKind::NoMonsters, entity);
    };

    const LevelTraceResult middle = look_down((mins[0] + maxs[0]) * 0.5f, (mins[1] + maxs[1]) * 0.5f);
    if (middle.fraction == 1.0f) { return false; }

    const float middle_height = middle.end_position[2];
    for (const std::array<float, 2> &corner : corners)
    {
      const LevelTraceResult trace = look_down(corner[0], corner[1]);
      if (trace.fraction == 1.0f || middle_height - trace.end_position[2] > step_height) { return false; }
    }
    return true;
  }

  bool LevelStepping::MoveThroughTheOpen(const std::int32_t entity, const LevelVector &move, const bool relink) const
  {
    const LevelVector mins = _fields.mins.Get(_machine, entity);
    const LevelVector maxs = _fields.maxs.Get(_machine, entity);
    const std::int32_t enemy = _fields.enemy.Get(_machine, entity);

    // once with a drift to the height of the enemy, and once level
    for (int attempt = 0; attempt < 2; attempt++)
    {
      const LevelVector origin = _fields.origin.Get(_machine, entity);
      LevelVector target = Sum(origin, move);
      if (attempt == 0 && enemy != 0)
      {
        const float above = origin[2] - _fields.origin.Get(_machine, enemy)[2];
        if (above > 40.0f) { target[2] -= 8.0f; }
        if (above < 30.0f) { target[2] += 8.0f; }
      }

      const LevelTraceResult trace = _collision.Trace(origin, mins, maxs, target, LevelTraceKind::Normal, entity);
      if (trace.fraction == 1.0f)
      {
        // what swims does not leave the water
        if (HasAnyFlag(entity, static_cast<std::int32_t>(QcFlag::Swim)) &&
            _collision.GetPointContents(trace.end_position) == BspContents::Empty)
        {
          return false;
        }

        _fields.origin.Set(_machine, entity, trace.end_position);
        if (relink) { _touching.Link(entity, true); }
        return true;
      }
      if (enemy == 0) { break; }
    }
    return false;
  }

  bool LevelStepping::MoveStep(const std::int32_t entity, const LevelVector &move, const bool relink) const
  {
    if (HasAnyFlag(entity, static_cast<std::int32_t>(QcFlag::Swim) | static_cast<std::int32_t>(QcFlag::Fly)))
    {
      return MoveThroughTheOpen(entity, move, relink);
    }

    const LevelVector old_origin = _fields.origin.Get(_machine, entity);
    const LevelVector mins = _fields.mins.Get(_machine, entity);
    const LevelVector maxs = _fields.maxs.Get(_machine, entity);
    const auto has_partial_ground = [&]
    {
      return HasFlag(_fields.flags.Get(_machine, entity), QcFlag::PartialGround);
    };

    // From a step above where it wants to be, down to a step below it:
    // where that ends is where it stands after the step.
    LevelVector start = Sum(old_origin, move);
    start[2] += step_height;
    const LevelVector end = {start[0], start[1], start[2] - 2.0f * step_height};

    LevelTraceResult trace = _collision.Trace(start, mins, maxs, end, LevelTraceKind::Normal, entity);
    if (trace.all_solid) { return false; }
    if (trace.start_solid)
    {
      // no room a step up: under a low ceiling it goes level
      start[2] -= step_height;
      trace = _collision.Trace(start, mins, maxs, end, LevelTraceKind::Normal, entity);
      if (trace.all_solid || trace.start_solid) { return false; }
    }

    if (trace.fraction == 1.0f)
    {
      // Nothing under it within a step: it would walk off an edge. One
      // that had the ground pulled from under it goes all the same, and
      // falls.
      if (!has_partial_ground()) { return false; }

      _fields.origin.Set(_machine, entity, Sum(old_origin, move));
      if (relink) { _touching.Link(entity, true); }
      _fields.flags.Set(_machine, entity, WithoutFlag(_fields.flags.Get(_machine, entity), QcFlag::OnGround));
      return true;
    }

    // it stands there now, when no corner of it hangs over an edge
    _fields.origin.Set(_machine, entity, trace.end_position);
    if (!CheckBottom(entity))
    {
      if (has_partial_ground())
      {
        // it had no whole floor before either, and is walking off it
        if (relink) { _touching.Link(entity, true); }
        return true;
      }
      _fields.origin.Set(_machine, entity, old_origin);
      return false;
    }

    _fields.flags.Set(_machine, entity, WithoutFlag(_fields.flags.Get(_machine, entity), QcFlag::PartialGround));
    _fields.groundentity.Set(_machine, entity, trace.entity);
    if (relink) { _touching.Link(entity, true); }
    return true;
  }

  bool LevelStepping::WalkMove(const std::int32_t entity, const float yaw, const float distance) const
  {
    if (!CanStep(entity)) { return false; }

    return MoveStep(entity, StepAlong(yaw, distance), true);
  }

  void LevelStepping::ChangeYaw(const std::int32_t entity) const
  {
    LevelVector angles = _fields.angles.Get(_machine, entity);
    const float current = WrapAngle(angles[1]);
    const float ideal = _fields.ideal_yaw.Get(_machine, entity);
    const float speed = _fields.yaw_speed.Get(_machine, entity);
    if (current == ideal) { return; }

    // the shorter way round, and no faster than it turns
    float turn = ideal - current;
    if (ideal > current)
    {
      if (turn >= 180.0f) { turn -= 360.0f; }
    }
    else if (turn <= -180.0f) { turn += 360.0f; }
    if (turn > 0.0f)
    {
      if (turn > speed) { turn = speed; }
    }
    else if (turn < -speed) { turn = -speed; }

    angles[1] = WrapAngle(current + turn);
    _fields.angles.Set(_machine, entity, angles);
  }

  bool LevelStepping::StepDirection(const std::int32_t entity, const float yaw, const float distance) const
  {
    _fields.ideal_yaw.Set(_machine, entity, yaw);
    ChangeYaw(entity);

    const LevelVector old_origin = _fields.origin.Get(_machine, entity);
    const bool stepped = MoveStep(entity, StepAlong(yaw, distance), false);
    if (stepped)
    {
      // not turned far enough yet: the step is not taken
      const float off = _fields.angles.Get(_machine, entity)[1] - _fields.ideal_yaw.Get(_machine, entity);
      if (off > 45.0f && off < 315.0f) { _fields.origin.Set(_machine, entity, old_origin); }
    }
    _touching.Link(entity, true);
    return stepped;
  }

  void LevelStepping::ChooseDirection(const std::int32_t entity, const std::int32_t goal, const float distance) const
  {
    const auto by_chance = [this](const float chance) { return _random && _random() < chance; };

    // the way it went, to the nearest of the eight below it
    const float old_direction =
      WrapAngle(static_cast<float>(static_cast<std::int32_t>(_fields.ideal_yaw.Get(_machine, entity) / 45.0f) * 45));
    const float turn_around = WrapAngle(old_direction - 180.0f);

    const LevelVector delta =
      Difference(_fields.origin.Get(_machine, goal), _fields.origin.Get(_machine, entity));
    float along_x = no_direction;
    if (delta[0] > 10.0f) { along_x = 0.0f; }
    else if (delta[0] < -10.0f) { along_x = 180.0f; }
    float along_y = no_direction;
    if (delta[1] < -10.0f) { along_y = 270.0f; }
    else if (delta[1] > 10.0f) { along_y = 90.0f; }

    // straight at the goal, between the two axes
    if (along_x != no_direction && along_y != no_direction)
    {
      // The last of the four is 215 in the original, where 225 was
      // meant. The game was tuned with it, so it stays.
      float diagonal;
      if (along_x == 0.0f) { diagonal = along_y == 90.0f ? 45.0f : 315.0f; }
      else { diagonal = along_y == 90.0f ? 135.0f : 215.0f; }
      if (diagonal != turn_around && StepDirection(entity, diagonal, distance)) { return; }
    }

    // along one axis, the longer one first, or the other by chance
    if (by_chance(0.5f) || std::fabs(delta[1]) > std::fabs(delta[0])) { std::swap(along_x, along_y); }
    if (along_x != no_direction && along_x != turn_around && StepDirection(entity, along_x, distance)) { return; }
    if (along_y != no_direction && along_y != turn_around && StepDirection(entity, along_y, distance)) { return; }

    // there is no straight way: the way it went before
    if (old_direction != no_direction && StepDirection(entity, old_direction, distance)) { return; }

    // any of the eight, counted up or down by chance
    if (by_chance(0.5f))
    {
      for (float direction = 0.0f; direction <= 315.0f; direction += 45.0f)
      {
        if (direction != turn_around && StepDirection(entity, direction, distance)) { return; }
      }
    }
    else
    {
      for (float direction = 315.0f; direction >= 0.0f; direction -= 45.0f)
      {
        if (direction != turn_around && StepDirection(entity, direction, distance)) { return; }
      }
    }

    // back where it came from
    if (turn_around != no_direction && StepDirection(entity, turn_around, distance)) { return; }

    // It cannot move. When it does not even stand on a whole floor, a
    // bridge may have gone from under it: it is let walk off what is left.
    _fields.ideal_yaw.Set(_machine, entity, old_direction);
    if (!CheckBottom(entity))
    {
      _fields.flags.Set(_machine, entity, WithFlag(_fields.flags.Get(_machine, entity), QcFlag::PartialGround));
    }
  }

  bool LevelStepping::IsCloseEnough(const std::int32_t entity, const std::int32_t goal, const float distance) const
  {
    const LevelBox own = _collision.GetBox(entity);
    const LevelBox other = _collision.GetBox(goal);
    for (std::size_t i = 0; i < own.mins.size(); i++)
    {
      if (other.mins[i] > own.maxs[i] + distance) { return false; }
      if (other.maxs[i] < own.mins[i] - distance) { return false; }
    }
    return true;
  }

  void LevelStepping::MoveToGoal(const std::int32_t entity, const float distance) const
  {
    if (!CanStep(entity)) { return; }

    const std::int32_t goal = _fields.goalentity.Get(_machine, entity);
    if (_fields.enemy.Get(_machine, entity) != 0 && IsCloseEnough(entity, goal, distance)) { return; }

    // on the way it faces, and another way when that is blocked, or by a
    // chance of one in four, so that it does not walk a line for ever
    const bool bumps_around = _random && _random() < 0.25f;
    if (bumps_around || !StepDirection(entity, _fields.ideal_yaw.Get(_machine, entity), distance))
    {
      ChooseDirection(entity, goal, distance);
    }
  }
} // quake
