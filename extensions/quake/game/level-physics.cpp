#include "level-physics.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "formats/bsp-contents.hpp"
#include "level-box.hpp"
#include "level-trace-kind.hpp"
#include "qc-flag.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"

namespace quake
{
  // Helpers of LevelPhysics: a velocity that slides along what it hit.
  namespace
  {
    /// How steep what an entity comes down on may be to count as ground:
    /// the up part of its normal.
    constexpr float ground_normal = 0.7f;

    /// How many times a move slides along something new in one frame, and
    /// how many planes it keeps clear of at once.
    constexpr int max_bumps = 4;
    constexpr std::size_t max_planes = 5;

    /// A velocity without the part that goes into a plane. `bounce` is 1
    /// to slide along it, and more to be thrown back from it. What is
    /// left along an axis and is next to nothing is made nothing.
    LevelVector ClipVelocity(const LevelVector &velocity, const LevelVector &normal, const float bounce)
    {
      const float into = Dot(velocity, normal) * bounce;
      LevelVector clipped = Difference(velocity, Scaled(normal, into));
      for (float &part : clipped)
      {
        if (part > -0.1f && part < 0.1f) { part = 0.0f; }
      }
      return clipped;
    }

    bool Is(const float value, const QcMoveType type)
    {
      return value == static_cast<float>(type);
    }

    bool Is(const float value, const QcSolid solid)
    {
      return value == static_cast<float>(solid);
    }
  }

  LevelPhysics::LevelPhysics(LevelCollision &collision, LevelTouching &touching)
    : _collision(collision),
      _touching(touching),
      _machine(collision.GetMachine()),
      _fields(_machine.GetProgs()),
      _gravity_scale(_machine.GetProgs(), "gravity")
  {
  }

  void LevelPhysics::SetGravity(const float gravity)
  {
    _gravity = gravity;
  }

  float LevelPhysics::GetGravity() const
  {
    return _gravity;
  }

  void LevelPhysics::SetMaxVelocity(const float max_velocity)
  {
    _max_velocity = max_velocity;
  }

  float LevelPhysics::GetMaxVelocity() const
  {
    return _max_velocity;
  }

  void LevelPhysics::SetMovesClients(const bool moves_clients)
  {
    _moves_clients = moves_clients;
  }

  bool LevelPhysics::GetMovesClients() const
  {
    return _moves_clients;
  }

  bool LevelPhysics::HasFlag(const std::int32_t entity, const QcFlag flag) const
  {
    return quake::HasFlag(_fields.flags.Get(_machine, entity), flag);
  }

  void LevelPhysics::SetFlag(const std::int32_t entity, const QcFlag flag, const bool set) const
  {
    const float flags = _fields.flags.Get(_machine, entity);
    _fields.flags.Set(_machine, entity, set ? WithFlag(flags, flag) : WithoutFlag(flags, flag));
  }

  void LevelPhysics::CheckVelocity(const std::int32_t entity) const
  {
    LevelVector velocity = _fields.velocity.Get(_machine, entity);
    LevelVector origin = _fields.origin.Get(_machine, entity);
    for (std::size_t i = 0; i < velocity.size(); i++)
    {
      // what game code divided by zero
      if (std::isnan(velocity[i])) { velocity[i] = 0.0f; }
      if (std::isnan(origin[i])) { origin[i] = 0.0f; }

      if (velocity[i] > _max_velocity) { velocity[i] = _max_velocity; }
      if (velocity[i] < -_max_velocity) { velocity[i] = -_max_velocity; }
    }
    _fields.velocity.Set(_machine, entity, velocity);
    _fields.origin.Set(_machine, entity, origin);
  }

  void LevelPhysics::AddGravity(const std::int32_t entity, const float dt) const
  {
    const float own = _gravity_scale.Get(_machine, entity);
    const float scale = own != 0.0f ? own : 1.0f;

    LevelVector velocity = _fields.velocity.Get(_machine, entity);
    velocity[2] -= scale * _gravity * dt;
    _fields.velocity.Set(_machine, entity, velocity);
  }

  LevelTraceResult LevelPhysics::PushEntity(const std::int32_t entity, const LevelVector &push) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);

    // A missile hits a monster in a larger box, and what stops nothing
    // itself is stopped by the level alone.
    LevelTraceKind kind = LevelTraceKind::Normal;
    const float solid = _fields.solid.Get(_machine, entity);
    if (Is(_fields.movetype.Get(_machine, entity), QcMoveType::FlyMissile)) { kind = LevelTraceKind::Missile; }
    else if (Is(solid, QcSolid::Trigger) || Is(solid, QcSolid::Not)) { kind = LevelTraceKind::NoMonsters; }

    const LevelTraceResult trace = _collision.Trace(
      origin, _fields.mins.Get(_machine, entity), _fields.maxs.Get(_machine, entity), Sum(origin, push), kind,
      entity);

    _fields.origin.Set(_machine, entity, trace.end_position);
    _touching.Link(entity, true);
    if (trace.HasEntity() && !_machine.IsEntityFree(entity)) { _touching.Impact(entity, trace.entity); }
    return trace;
  }

  void LevelPhysics::FlyMove(const std::int32_t entity, const float dt) const
  {
    const LevelVector first_velocity = _fields.velocity.Get(_machine, entity);
    LevelVector slide_velocity = first_velocity;
    std::array<LevelVector, max_planes> planes{};
    std::size_t plane_count = 0;
    float time_left = dt;

    for (int bump = 0; bump < max_bumps; bump++)
    {
      LevelVector velocity = _fields.velocity.Get(_machine, entity);
      if (IsZero(velocity)) { break; }

      const LevelVector origin = _fields.origin.Get(_machine, entity);
      const LevelTraceResult trace = _collision.Trace(
        origin, _fields.mins.Get(_machine, entity), _fields.maxs.Get(_machine, entity),
        Sum(origin, Scaled(velocity, time_left)), LevelTraceKind::Normal, entity);

      if (trace.all_solid)
      {
        // stuck in what is solid
        _fields.velocity.Set(_machine, entity, {});
        return;
      }
      if (trace.fraction > 0.0f)
      {
        // it got somewhere: the planes behind it are forgotten
        _fields.origin.Set(_machine, entity, trace.end_position);
        slide_velocity = velocity;
        plane_count = 0;
      }
      if (trace.fraction == 1.0f || !trace.HasEntity()) { break; }

      // it came down on a floor, which holds it when it is of the level
      if (trace.plane_normal[2] > ground_normal &&
          (trace.entity == 0 || Is(_fields.solid.Get(_machine, trace.entity), QcSolid::Bsp)))
      {
        SetFlag(entity, QcFlag::OnGround, true);
        _fields.groundentity.Set(_machine, entity, trace.entity);
      }

      _touching.Impact(entity, trace.entity);
      if (_machine.IsEntityFree(entity)) { break; }

      time_left -= time_left * trace.fraction;
      if (plane_count >= max_planes)
      {
        _fields.velocity.Set(_machine, entity, {});
        return;
      }
      planes[plane_count++] = trace.plane_normal;

      // a velocity along one of the planes that goes into none of the
      // others
      LevelVector along{};
      std::size_t chosen = 0;
      for (; chosen < plane_count; chosen++)
      {
        along = ClipVelocity(slide_velocity, planes[chosen], 1.0f);
        std::size_t other = 0;
        for (; other < plane_count; other++)
        {
          if (other != chosen && Dot(along, planes[other]) < 0.0f) { break; }
        }
        if (other == plane_count) { break; }
      }

      // the touch may have given it a velocity of its own
      velocity = _fields.velocity.Get(_machine, entity);
      if (chosen != plane_count) { velocity = along; }
      else
      {
        // none: between two planes it goes along the crease, and among
        // more it stops
        if (plane_count != 2)
        {
          _fields.velocity.Set(_machine, entity, {});
          return;
        }
        const LevelVector crease = Cross(planes[0], planes[1]);
        velocity = Scaled(crease, Dot(crease, velocity));
      }

      // turned back against where it was going, it stops, so that it does
      // not shake in a corner
      if (Dot(velocity, first_velocity) <= 0.0f) { velocity = {}; }
      _fields.velocity.Set(_machine, entity, velocity);
      if (IsZero(velocity)) { return; }
    }
  }

  void LevelPhysics::CheckWaterTransition(const std::int32_t entity) const
  {
    const auto contents = static_cast<float>(_collision.GetPointContents(_fields.origin.Get(_machine, entity)));
    const auto empty = static_cast<float>(BspContents::Empty);

    // the first time it is only noted
    if (_fields.watertype.Get(_machine, entity) == 0.0f)
    {
      _fields.watertype.Set(_machine, entity, contents);
      _fields.waterlevel.Set(_machine, entity, 1.0f);
      return;
    }

    // The original plays a splash when it goes in or comes out. Sounds are
    // a host's own, which tells by the two fields.
    if (contents <= static_cast<float>(BspContents::Water))
    {
      _fields.watertype.Set(_machine, entity, contents);
      _fields.waterlevel.Set(_machine, entity, 1.0f);
    }
    else
    {
      // out of it, the original leaves what fills the place as the level
      // of the water, and so does this
      _fields.watertype.Set(_machine, entity, empty);
      _fields.waterlevel.Set(_machine, entity, contents);
    }
  }

  void LevelPhysics::MoveTossed(const std::int32_t entity, const float dt) const
  {
    // what lies on the ground lies
    if (HasFlag(entity, QcFlag::OnGround)) { return; }

    CheckVelocity(entity);
    const float move_type = _fields.movetype.Get(_machine, entity);
    if (!Is(move_type, QcMoveType::Fly) && !Is(move_type, QcMoveType::FlyMissile)) { AddGravity(entity, dt); }

    _fields.angles.Set(
      _machine, entity,
      Sum(_fields.angles.Get(_machine, entity), Scaled(_fields.avelocity.Get(_machine, entity), dt)));

    const LevelTraceResult trace = PushEntity(entity, Scaled(_fields.velocity.Get(_machine, entity), dt));
    if (trace.fraction == 1.0f || _machine.IsEntityFree(entity)) { return; }

    // thrown back from what it hit, or sliding along it
    const bool bounces = Is(move_type, QcMoveType::Bounce);
    const LevelVector velocity =
      ClipVelocity(_fields.velocity.Get(_machine, entity), trace.plane_normal, bounces ? 1.5f : 1.0f);
    _fields.velocity.Set(_machine, entity, velocity);

    // on a floor it stays, unless it still bounces high
    if (trace.plane_normal[2] > ground_normal && (velocity[2] < 60.0f || !bounces))
    {
      SetFlag(entity, QcFlag::OnGround, true);
      _fields.groundentity.Set(_machine, entity, trace.entity);
      _fields.velocity.Set(_machine, entity, {});
      _fields.avelocity.Set(_machine, entity, {});
    }

    CheckWaterTransition(entity);
  }

  void LevelPhysics::MoveStepper(const std::int32_t entity, const float dt) const
  {
    // it falls when nothing holds it
    if (!HasFlag(entity, QcFlag::OnGround) && !HasFlag(entity, QcFlag::Fly) && !HasFlag(entity, QcFlag::Swim))
    {
      AddGravity(entity, dt);
      CheckVelocity(entity);
      FlyMove(entity, dt);
      if (_machine.IsEntityFree(entity)) { return; }

      _touching.Link(entity, true);
      if (_machine.IsEntityFree(entity)) { return; }
    }

    CheckWaterTransition(entity);
  }

  void LevelPhysics::MoveNoClip(const std::int32_t entity, const float dt) const
  {
    _fields.angles.Set(
      _machine, entity,
      Sum(_fields.angles.Get(_machine, entity), Scaled(_fields.avelocity.Get(_machine, entity), dt)));
    _fields.origin.Set(
      _machine, entity,
      Sum(_fields.origin.Get(_machine, entity), Scaled(_fields.velocity.Get(_machine, entity), dt)));
    _touching.Link(entity, false);
  }

  void LevelPhysics::MoveEntity(const std::int32_t entity, const float dt)
  {
    if (entity == 0 || _machine.IsEntityFree(entity)) { return; }

    const float move_type = _fields.movetype.Get(_machine, entity);
    if (Is(move_type, QcMoveType::Toss) || Is(move_type, QcMoveType::Bounce) || Is(move_type, QcMoveType::Fly) ||
        Is(move_type, QcMoveType::FlyMissile))
    {
      MoveTossed(entity, dt);
    }
    else if (Is(move_type, QcMoveType::Step)) { MoveStepper(entity, dt); }
    else if (Is(move_type, QcMoveType::NoClip)) { MoveNoClip(entity, dt); }
  }

  bool LevelPhysics::MovePusher(
    const std::int32_t entity,
    const std::array<float, 3> &origin,
    const std::array<float, 3> &angles,
    const float dt)
  {
    const LevelVector old_origin = _fields.origin.Get(_machine, entity);
    const LevelVector move = Difference(origin, old_origin);
    if (IsZero(move)) { return true; }

    // where the pusher is when it has moved
    _fields.origin.Set(_machine, entity, origin);
    _touching.Link(entity, false);
    const LevelBox reach = _collision.GetBox(entity);
    const float pusher_solid = _fields.solid.Get(_machine, entity);

    // who was moved, and from where, to put them back
    std::vector<std::pair<std::int32_t, LevelVector>> moved;

    for (std::int32_t check = 1; check < _machine.GetEntityCount(); check++)
    {
      if (check == entity || _machine.IsEntityFree(check)) { continue; }

      const float move_type = _fields.movetype.Get(_machine, check);
      if (Is(move_type, QcMoveType::Push) || Is(move_type, QcMoveType::None) || Is(move_type, QcMoveType::NoClip))
      {
        continue;
      }
      if (!_moves_clients && HasFlag(check, QcFlag::Client)) { continue; }

      // what stands on the pusher moves for certain, and anything else
      // only when the pusher is now where it is
      const bool rides = HasFlag(check, QcFlag::OnGround) && _fields.groundentity.Get(_machine, check) == entity;
      if (!rides)
      {
        const LevelBox box = _collision.GetBox(check);
        bool apart = false;
        for (std::size_t i = 0; i < box.mins.size(); i++)
        {
          if (box.mins[i] >= reach.maxs[i] || box.maxs[i] <= reach.mins[i]) { apart = true; }
        }
        if (apart || !_collision.TestPosition(check)) { continue; }
      }

      // a player keeps standing, anything else finds its ground anew
      if (!Is(move_type, QcMoveType::Walk)) { SetFlag(check, QcFlag::OnGround, false); }

      const LevelVector from = _fields.origin.Get(_machine, check);
      moved.emplace_back(check, from);

      // moved along, while the pusher itself is no obstacle
      _fields.solid.Set(_machine, entity, static_cast<float>(QcSolid::Not));
      PushEntity(check, move);
      _fields.solid.Set(_machine, entity, pusher_solid);

      // a touch on the way may have removed it
      if (_machine.IsEntityFree(check) || !_collision.TestPosition(check)) { continue; }

      // It is still inside something: it had nowhere to go. What has no
      // width is left where it is.
      const LevelVector mins = _fields.mins.Get(_machine, check);
      if (mins[0] == _fields.maxs.Get(_machine, check)[0]) { continue; }

      // what stops nothing, a corpse, is squashed flat and left
      const float solid = _fields.solid.Get(_machine, check);
      if (Is(solid, QcSolid::Not) || Is(solid, QcSolid::Trigger))
      {
        const LevelVector flat = {0.0f, 0.0f, mins[2]};
        _fields.mins.Set(_machine, check, flat);
        _fields.maxs.Set(_machine, check, flat);
        continue;
      }

      // blocked: the obstacle and the pusher go back, the pusher is told,
      // and then everything that was moved before goes back
      _fields.origin.Set(_machine, check, from);
      _touching.Link(check, true);
      _fields.origin.Set(_machine, entity, old_origin);
      _touching.Link(entity, false);

      _touching.Block(entity, check);

      for (const auto &[rider, place] : moved)
      {
        if (_machine.IsEntityFree(rider)) { continue; }

        _fields.origin.Set(_machine, rider, place);
        _touching.Link(rider, false);
      }
      return false;
    }
    return true;
  }
} // quake
