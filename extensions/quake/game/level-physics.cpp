#include "level-physics.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "formats/bsp-contents.hpp"
#include "level-axes.hpp"
#include "level-box.hpp"
#include "level-trace-kind.hpp"
#include "qc-flag.hpp"
#include "qc-move-type.hpp"
#include "qc-solid.hpp"

namespace quake
{
  // Helpers of LevelPhysics: a velocity that slides along what it hit, and
  // the numbers of the original for what a player runs into.
  namespace
  {
    /// How steep what an entity comes down on may be to count as ground:
    /// the up part of its normal.
    constexpr float ground_normal = 0.7f;

    /// How many times a move slides along something new in one frame, and
    /// how many planes it keeps clear of at once.
    constexpr int max_bumps = 4;
    constexpr std::size_t max_planes = 5;

    /// What a move ran into, as bits: a floor, something upright, and
    /// with the third that it got stuck.
    constexpr int blocked_by_floor = 1;
    constexpr int blocked_by_wall = 2;
    constexpr int blocked_stuck = 4;

    /// How far up a player in what is solid is tried to be put.
    constexpr int unstick_height = 18;

    /// How far to a side a player who climbs a step and gets nowhere is
    /// put to try again, how long the move tried from there is, and how
    /// far it has to get.
    constexpr float unstick_nudge = 2.0f;
    constexpr float unstick_time = 0.1f;
    constexpr float unstick_progress = 4.0f;

    /// Less than this along both axes of the ground is getting nowhere.
    constexpr float no_progress = 0.03125f;

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

  void LevelPhysics::SetWalksClients(const bool walks_clients)
  {
    _walks_clients = walks_clients;
  }

  bool LevelPhysics::GetWalksClients() const
  {
    return _walks_clients;
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

  int LevelPhysics::FlyMove(const std::int32_t entity, const float dt, LevelTraceResult *wall) const
  {
    int blocked = 0;
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
        return blocked_by_floor | blocked_by_wall;
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
      if (trace.plane_normal[2] > ground_normal)
      {
        blocked |= blocked_by_floor;
        if (trace.entity == 0 || Is(_fields.solid.Get(_machine, trace.entity), QcSolid::Bsp))
        {
          SetFlag(entity, QcFlag::OnGround, true);
          _fields.groundentity.Set(_machine, entity, trace.entity);
        }
      }
      if (trace.plane_normal[2] == 0.0f)
      {
        blocked |= blocked_by_wall;
        if (wall != nullptr) { *wall = trace; }
      }

      _touching.Impact(entity, trace.entity);
      if (_machine.IsEntityFree(entity)) { break; }

      time_left -= time_left * trace.fraction;
      if (plane_count >= max_planes)
      {
        _fields.velocity.Set(_machine, entity, {});
        return blocked_by_floor | blocked_by_wall;
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
          return blocked_by_floor | blocked_by_wall | blocked_stuck;
        }
        const LevelVector crease = Cross(planes[0], planes[1]);
        velocity = Scaled(crease, Dot(crease, velocity));
      }

      // turned back against where it was going, it stops, so that it does
      // not shake in a corner
      if (Dot(velocity, first_velocity) <= 0.0f) { velocity = {}; }
      _fields.velocity.Set(_machine, entity, velocity);
      if (IsZero(velocity)) { return blocked; }
    }
    return blocked;
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

  bool LevelPhysics::CheckWater(const std::int32_t entity) const
  {
    const LevelVector origin = _fields.origin.Get(_machine, entity);
    const LevelVector mins = _fields.mins.Get(_machine, entity);
    const LevelVector maxs = _fields.maxs.Get(_machine, entity);

    // Water, slime, and lava, and as in the original the sky, which has a
    // number below theirs. Water that flows is water to the collision.
    const auto is_liquid = [this, &origin](const float height, float &contents)
    {
      contents = static_cast<float>(_collision.GetPointContents({origin[0], origin[1], height}));
      return contents <= static_cast<float>(BspContents::Water);
    };

    float level = 0.0f;
    auto type = static_cast<float>(BspContents::Empty);
    float contents = 0.0f;
    if (is_liquid(origin[2] + mins[2] + 1.0f, contents))
    {
      // the feet are in it: then the waist, then the eyes
      type = contents;
      level = 1.0f;
      if (is_liquid(origin[2] + (mins[2] + maxs[2]) * 0.5f, contents))
      {
        level = 2.0f;
        if (is_liquid(origin[2] + _fields.view_ofs.Get(_machine, entity)[2], contents)) { level = 3.0f; }
      }
    }
    _fields.waterlevel.Set(_machine, entity, level);
    _fields.watertype.Set(_machine, entity, type);
    return level > 1.0f;
  }

  void LevelPhysics::CheckStuck(const std::int32_t entity) const
  {
    if (!_collision.TestPosition(entity))
    {
      _fields.oldorigin.Set(_machine, entity, _fields.origin.Get(_machine, entity));
      return;
    }

    // where the player was free last
    const LevelVector stuck_at = _fields.origin.Get(_machine, entity);
    _fields.origin.Set(_machine, entity, _fields.oldorigin.Get(_machine, entity));
    if (!_collision.TestPosition(entity))
    {
      _touching.Link(entity, true);
      return;
    }

    // a unit to each side, and higher and higher
    for (int up = 0; up < unstick_height; up++)
    {
      for (int x = -1; x <= 1; x++)
      {
        for (int y = -1; y <= 1; y++)
        {
          _fields.origin.Set(
            _machine, entity,
            Sum(stuck_at, {static_cast<float>(x), static_cast<float>(y), static_cast<float>(up)}));
          if (!_collision.TestPosition(entity))
          {
            _touching.Link(entity, true);
            return;
          }
        }
      }
    }

    // stuck for good: the player stays
    _fields.origin.Set(_machine, entity, stuck_at);
  }

  void LevelPhysics::ApplyWallFriction(const std::int32_t entity, const LevelTraceResult &wall) const
  {
    // only for a player who looks at the wall more than along it
    const LevelVector forward = LevelAxes::Of(_fields.v_angle.Get(_machine, entity)).forward;
    const float facing = Dot(wall.plane_normal, forward) + 0.5f;
    if (facing >= 0.0f) { return; }

    // what goes along the wall is cut, the more the straighter the look
    LevelVector velocity = _fields.velocity.Get(_machine, entity);
    const LevelVector along =
      Difference(velocity, Scaled(wall.plane_normal, Dot(wall.plane_normal, velocity)));
    velocity[0] = along[0] * (1.0f + facing);
    velocity[1] = along[1] * (1.0f + facing);
    _fields.velocity.Set(_machine, entity, velocity);
  }

  int LevelPhysics::TryUnstick(const std::int32_t entity, const LevelVector &velocity_before) const
  {
    constexpr LevelVector nudges[] = {
      {unstick_nudge, 0.0f, 0.0f},
      {0.0f, unstick_nudge, 0.0f},
      {-unstick_nudge, 0.0f, 0.0f},
      {0.0f, -unstick_nudge, 0.0f},
      {unstick_nudge, unstick_nudge, 0.0f},
      {-unstick_nudge, unstick_nudge, 0.0f},
      {unstick_nudge, -unstick_nudge, 0.0f},
      {-unstick_nudge, -unstick_nudge, 0.0f},
    };

    const LevelVector from = _fields.origin.Get(_machine, entity);
    for (const LevelVector &nudge : nudges)
    {
      PushEntity(entity, nudge);
      if (_machine.IsEntityFree(entity)) { return 0; }

      // the move the player wanted, along the ground
      _fields.velocity.Set(_machine, entity, {velocity_before[0], velocity_before[1], 0.0f});
      LevelTraceResult wall;
      const int blocked = FlyMove(entity, unstick_time, &wall);
      if (_machine.IsEntityFree(entity)) { return 0; }

      const LevelVector origin = _fields.origin.Get(_machine, entity);
      if (std::fabs(from[1] - origin[1]) > unstick_progress || std::fabs(from[0] - origin[0]) > unstick_progress)
      {
        return blocked;
      }

      // back, and to the next side
      _fields.origin.Set(_machine, entity, from);
    }

    _fields.velocity.Set(_machine, entity, {});
    return blocked_by_floor | blocked_by_wall | blocked_stuck;
  }

  void LevelPhysics::WalkMove(const std::int32_t entity, const float dt) const
  {
    // whether the player stands is found anew by the move
    const bool stood = HasFlag(entity, QcFlag::OnGround);
    SetFlag(entity, QcFlag::OnGround, false);

    const LevelVector origin_before = _fields.origin.Get(_machine, entity);
    const LevelVector velocity_before = _fields.velocity.Get(_machine, entity);

    LevelTraceResult wall;
    int blocked = FlyMove(entity, dt, &wall);
    if (_machine.IsEntityFree(entity)) { return; }

    // nothing upright was in the way: no step to climb
    if ((blocked & blocked_by_wall) == 0) { return; }

    // no step is climbed in the air, only out of water
    if (!stood && _fields.waterlevel.Get(_machine, entity) == 0.0f) { return; }

    // a touch on the way may have made the player something else
    if (!Is(_fields.movetype.Get(_machine, entity), QcMoveType::Walk)) { return; }
    if (HasFlag(entity, QcFlag::WaterJump)) { return; }

    // where the move without a step ended, should the step be none
    const LevelVector origin_without_step = _fields.origin.Get(_machine, entity);
    const LevelVector velocity_without_step = _fields.velocity.Get(_machine, entity);

    // from the start again: up,
    _fields.origin.Set(_machine, entity, origin_before);
    PushEntity(entity, {0.0f, 0.0f, step_height});
    if (_machine.IsEntityFree(entity)) { return; }

    // along the ground,
    _fields.velocity.Set(_machine, entity, {velocity_before[0], velocity_before[1], 0.0f});
    blocked = FlyMove(entity, dt, &wall);
    if (_machine.IsEntityFree(entity)) { return; }

    // The hulls of a level are not exact, and a player a step higher may
    // be held by what is not there.
    if (blocked != 0)
    {
      const LevelVector origin = _fields.origin.Get(_machine, entity);
      if (std::fabs(origin_before[1] - origin[1]) < no_progress &&
          std::fabs(origin_before[0] - origin[0]) < no_progress)
      {
        blocked = TryUnstick(entity, velocity_before);
        if (_machine.IsEntityFree(entity)) { return; }
      }
    }

    if ((blocked & blocked_by_wall) != 0) { ApplyWallFriction(entity, wall); }

    // and down, by the step and by what the player fell in the frame
    const LevelTraceResult down = PushEntity(entity, {0.0f, 0.0f, -step_height + velocity_before[2] * dt});
    if (_machine.IsEntityFree(entity)) { return; }

    if (down.plane_normal[2] > ground_normal)
    {
      // The original asks here whether the player itself is a part of the
      // level, which no player is. So a player who climbed a step stands
      // on the ground again only by the next frame's move, as there.
      if (Is(_fields.solid.Get(_machine, entity), QcSolid::Bsp))
      {
        SetFlag(entity, QcFlag::OnGround, true);
        _fields.groundentity.Set(_machine, entity, down.entity);
      }
    }
    else
    {
      // No floor up there, as at a wall on a slope: the move without the
      // step counts, or the player would hop up what is too steep.
      _fields.origin.Set(_machine, entity, origin_without_step);
      _fields.velocity.Set(_machine, entity, velocity_without_step);
    }
  }

  void LevelPhysics::MoveClient(const std::int32_t entity, const float dt) const
  {
    CheckVelocity(entity);

    const float move_type = _fields.movetype.Get(_machine, entity);
    if (Is(move_type, QcMoveType::Walk))
    {
      // A player falls in every frame, standing or not: the move down is
      // what finds the floor again. Not one who swims, nor one who is
      // thrown out of the water.
      if (!CheckWater(entity) && !HasFlag(entity, QcFlag::WaterJump)) { AddGravity(entity, dt); }
      CheckStuck(entity);
      WalkMove(entity, dt);
    }
    else if (Is(move_type, QcMoveType::Toss) || Is(move_type, QcMoveType::Bounce)) { MoveTossed(entity, dt); }
    else if (Is(move_type, QcMoveType::Fly)) { FlyMove(entity, dt); }
    else if (Is(move_type, QcMoveType::NoClip))
    {
      _fields.origin.Set(
        _machine, entity,
        Sum(_fields.origin.Get(_machine, entity), Scaled(_fields.velocity.Get(_machine, entity), dt)));
    }
    if (_machine.IsEntityFree(entity)) { return; }

    _touching.Link(entity, true);
  }

  void LevelPhysics::MoveEntity(const std::int32_t entity, const float dt)
  {
    if (entity == 0 || _machine.IsEntityFree(entity)) { return; }

    if (_walks_clients && HasFlag(entity, QcFlag::Client))
    {
      MoveClient(entity, dt);
      return;
    }

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
      if (!_moves_clients && !_walks_clients && HasFlag(check, QcFlag::Client)) { continue; }

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
