#include "player-movement.hpp"

#include <cmath>

#include "level-axes.hpp"
#include "level-trace-kind.hpp"
#include "level-trace-result.hpp"
#include "qc-flag.hpp"
#include "qc-move-type.hpp"

namespace quake
{
  // Helpers of PlayerMovement: the numbers the original has in its code,
  // and a direction of a vector.
  namespace
  {
    /// How far ahead of a player the ground is looked for, and how far
    /// under the player's feet.
    constexpr float edge_ahead = 16.0f;
    constexpr float edge_depth = 34.0f;

    /// How fast a player sinks who asks for nothing, and how much slower
    /// than on land a player swims.
    constexpr float sink_speed = 60.0f;
    constexpr float swim_scale = 0.7f;

    /// The lean to the side is shown four times as large as it is counted.
    constexpr float roll_scale = 4.0f;

    /// The model of a player nods by a third of where the player looks.
    constexpr float pitch_scale = 3.0f;

    bool Is(const float value, const QcMoveType type)
    {
      return value == static_cast<float>(type);
    }

    /// Makes a vector one unit long and gives how long it was. One of no
    /// length is left as it is.
    float Normalize(LevelVector &vector)
    {
      const float length = Length(vector);
      if (length != 0.0f) { vector = Scaled(vector, 1.0f / length); }
      return length;
    }
  }

  PlayerMovement::PlayerMovement(LevelCollision &collision)
    : _collision(collision), _machine(collision.GetMachine()), _fields(_machine.GetProgs())
  {
  }

  void PlayerMovement::SetSettings(const PlayerMovementSettings &settings)
  {
    _settings = settings;
  }

  const PlayerMovementSettings &PlayerMovement::GetSettings() const
  {
    return _settings;
  }

  void PlayerMovement::DropPunchAngle(const std::int32_t entity, const float dt) const
  {
    LevelVector punch = _fields.punchangle.Get(_machine, entity);
    float length = Normalize(punch);
    length -= _settings.punch_recovery * dt;
    if (length < 0.0f) { length = 0.0f; }
    _fields.punchangle.Set(_machine, entity, Scaled(punch, length));
  }

  float PlayerMovement::GetRoll(const LevelVector &angles, const LevelVector &velocity) const
  {
    float side = Dot(velocity, LevelAxes::Of(angles).right);
    const float sign = side < 0.0f ? -1.0f : 1.0f;
    side = std::fabs(side);

    side = side < _settings.roll_speed ? side * _settings.roll_angle / _settings.roll_speed : _settings.roll_angle;
    return side * sign;
  }

  void PlayerMovement::ApplyFriction(const std::int32_t entity, const float dt) const
  {
    const LevelVector velocity = _fields.velocity.Get(_machine, entity);
    const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]);
    if (speed == 0.0f) { return; }

    // a line down from under the feet a little ahead: when it meets
    // nothing, the player is about to go over an edge
    const LevelVector origin = _fields.origin.Get(_machine, entity);
    const LevelVector start = {
      origin[0] + velocity[0] / speed * edge_ahead,
      origin[1] + velocity[1] / speed * edge_ahead,
      origin[2] + _fields.mins.Get(_machine, entity)[2],
    };
    const LevelVector stop = {start[0], start[1], start[2] - edge_depth};
    const LevelTraceResult trace = _collision.Trace(start, {}, {}, stop, LevelTraceKind::NoMonsters, entity);
    const float friction =
      trace.fraction == 1.0f ? _settings.friction * _settings.edge_friction : _settings.friction;

    // what is slow is slowed down as if it were faster, so that it stops
    const float control = speed < _settings.stop_speed ? _settings.stop_speed : speed;
    float new_speed = speed - dt * control * friction;
    if (new_speed < 0.0f) { new_speed = 0.0f; }
    _fields.velocity.Set(_machine, entity, Scaled(velocity, new_speed / speed));
  }

  void PlayerMovement::Accelerate(
    const std::int32_t entity, const float wish_speed, const LevelVector &wish_direction, const float dt) const
  {
    const LevelVector velocity = _fields.velocity.Get(_machine, entity);
    const float add_speed = wish_speed - Dot(velocity, wish_direction);
    if (add_speed <= 0.0f) { return; }

    float gain = _settings.accelerate * dt * wish_speed;
    if (gain > add_speed) { gain = add_speed; }
    _fields.velocity.Set(_machine, entity, Sum(velocity, Scaled(wish_direction, gain)));
  }

  void PlayerMovement::AirAccelerate(
    const std::int32_t entity, const float wish_speed, const LevelVector &wish_velocity, const float dt) const
  {
    LevelVector direction = wish_velocity;
    float capped_speed = Normalize(direction);
    if (capped_speed > _settings.air_speed) { capped_speed = _settings.air_speed; }

    const LevelVector velocity = _fields.velocity.Get(_machine, entity);
    const float add_speed = capped_speed - Dot(velocity, direction);
    if (add_speed <= 0.0f) { return; }

    // by the whole speed asked for, not the capped one
    float gain = _settings.accelerate * wish_speed * dt;
    if (gain > add_speed) { gain = add_speed; }
    _fields.velocity.Set(_machine, entity, Sum(velocity, Scaled(direction, gain)));
  }

  void PlayerMovement::MoveOnLand(
    const std::int32_t entity, const PlayerCommand &command, const float time, const float dt) const
  {
    // by the angles of the model, which nod less than the player looks
    const LevelAxes axes = LevelAxes::Of(_fields.angles.Get(_machine, entity));

    // a player who just came out of a teleporter does not back into it
    float forward_move = command.forward_move;
    if (time < _fields.teleport_time.Get(_machine, entity) && forward_move < 0.0f) { forward_move = 0.0f; }

    const float move_type = _fields.movetype.Get(_machine, entity);
    LevelVector wish_velocity = Sum(Scaled(axes.forward, forward_move), Scaled(axes.right, command.side_move));
    wish_velocity[2] = Is(move_type, QcMoveType::Walk) ? 0.0f : command.up_move;

    LevelVector wish_direction = wish_velocity;
    float wish_speed = Normalize(wish_direction);
    if (wish_speed > _settings.max_speed)
    {
      wish_velocity = Scaled(wish_velocity, _settings.max_speed / wish_speed);
      wish_speed = _settings.max_speed;
    }

    if (Is(move_type, QcMoveType::NoClip))
    {
      _fields.velocity.Set(_machine, entity, wish_velocity);
    }
    else if (HasFlag(_fields.flags.Get(_machine, entity), QcFlag::OnGround))
    {
      ApplyFriction(entity, dt);
      Accelerate(entity, wish_speed, wish_direction, dt);
    }
    else
    {
      AirAccelerate(entity, wish_speed, wish_velocity, dt);
    }
  }

  void PlayerMovement::MoveInWater(const std::int32_t entity, const PlayerCommand &command, const float dt) const
  {
    // by where the player looks: looking up and going forward is going up
    const LevelAxes axes = LevelAxes::Of(_fields.v_angle.Get(_machine, entity));
    LevelVector wish_velocity =
      Sum(Scaled(axes.forward, command.forward_move), Scaled(axes.right, command.side_move));

    if (command.forward_move == 0.0f && command.side_move == 0.0f && command.up_move == 0.0f)
    {
      wish_velocity[2] -= sink_speed;
    }
    else { wish_velocity[2] += command.up_move; }

    float wish_speed = Length(wish_velocity);
    if (wish_speed > _settings.max_speed)
    {
      wish_velocity = Scaled(wish_velocity, _settings.max_speed / wish_speed);
      wish_speed = _settings.max_speed;
    }
    wish_speed *= swim_scale;

    // the water slows the player down, along every axis
    LevelVector velocity = _fields.velocity.Get(_machine, entity);
    const float speed = Length(velocity);
    float new_speed = 0.0f;
    if (speed != 0.0f)
    {
      new_speed = speed - dt * speed * _settings.friction;
      if (new_speed < 0.0f) { new_speed = 0.0f; }
      velocity = Scaled(velocity, new_speed / speed);
      _fields.velocity.Set(_machine, entity, velocity);
    }

    if (wish_speed == 0.0f) { return; }

    const float add_speed = wish_speed - new_speed;
    if (add_speed <= 0.0f) { return; }

    Normalize(wish_velocity);
    float gain = _settings.accelerate * wish_speed * dt;
    if (gain > add_speed) { gain = add_speed; }
    _fields.velocity.Set(_machine, entity, Sum(velocity, Scaled(wish_velocity, gain)));
  }

  void PlayerMovement::JumpOutOfWater(const std::int32_t entity, const float time) const
  {
    if (time > _fields.teleport_time.Get(_machine, entity) || _fields.waterlevel.Get(_machine, entity) == 0.0f)
    {
      _fields.flags.Set(_machine, entity, WithoutFlag(_fields.flags.Get(_machine, entity), QcFlag::WaterJump));
      _fields.teleport_time.Set(_machine, entity, 0.0f);
    }

    // towards the edge, as the game code noted it
    const LevelVector towards = _fields.movedir.Get(_machine, entity);
    LevelVector velocity = _fields.velocity.Get(_machine, entity);
    velocity[0] = towards[0];
    velocity[1] = towards[1];
    _fields.velocity.Set(_machine, entity, velocity);
  }

  void PlayerMovement::Steer(
    const std::int32_t entity, const PlayerCommand &command, const float time, const float dt) const
  {
    if (entity == 0 || _machine.IsEntityFree(entity)) { return; }

    _fields.v_angle.Set(_machine, entity, command.view_angles);

    const float move_type = _fields.movetype.Get(_machine, entity);
    if (Is(move_type, QcMoveType::None)) { return; }

    DropPunchAngle(entity, dt);

    // the dead are not steered
    if (_fields.health.Get(_machine, entity) <= 0.0f) { return; }

    // The model shows a third of the pitch, and leans with the speed to
    // the side. A kick of the view counts as looking.
    LevelVector angles = _fields.angles.Get(_machine, entity);
    const LevelVector view = Sum(command.view_angles, _fields.punchangle.Get(_machine, entity));
    angles[2] = GetRoll(angles, _fields.velocity.Get(_machine, entity)) * roll_scale;
    if (_fields.fixangle.Get(_machine, entity) == 0.0f)
    {
      angles[0] = -view[0] / pitch_scale;
      angles[1] = view[1];
    }
    _fields.angles.Set(_machine, entity, angles);

    if (HasFlag(_fields.flags.Get(_machine, entity), QcFlag::WaterJump))
    {
      JumpOutOfWater(entity, time);
      return;
    }

    if (_fields.waterlevel.Get(_machine, entity) >= 2.0f && !Is(move_type, QcMoveType::NoClip))
    {
      MoveInWater(entity, command, dt);
      return;
    }

    MoveOnLand(entity, command, time, dt);
  }
} // quake
