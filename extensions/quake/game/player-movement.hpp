#ifndef QUAKE_PLAYER_MOVEMENT_HPP
#define QUAKE_PLAYER_MOVEMENT_HPP

#include <cstdint>

#include "formats/qc-machine.hpp"
#include "level-collision.hpp"
#include "level-vector.hpp"
#include "player-command.hpp"
#include "player-movement-settings.hpp"
#include "qc-fields.hpp"

namespace quake
{
  /// Steers a player by what the player asks for, as the server of the
  /// original does for each of its clients in every frame, before anything
  /// moves: it turns a `PlayerCommand` into the `velocity` of the player's
  /// entity. It moves nothing. The mover of the level then moves the
  /// player by that velocity, see LevelPhysics::SetWalksClients().
  ///
  /// How a player is steered is where the player is:
  ///
  /// - On the ground the player is slowed down first, more so at an edge,
  ///   and then gets faster towards where the player wants to go, up to
  ///   the speed asked for.
  /// - In the air nothing slows the player down, and the player gains
  ///   only a little along the direction asked for. Turning while jumping
  ///   is how a player of the original gets faster than running.
  /// - In water up to the waist or higher the player swims where the
  ///   player looks, slower than on land, and sinks when nothing is asked.
  /// - With the `movetype` `NoClip` the player goes exactly as asked.
  ///
  /// A player whose `health` is gone is not steered, and one whose
  /// `movetype` is `None` is left alone altogether.
  ///
  /// Jumping is the game code's: it reads `button2` and sets the velocity
  /// upwards, in `PlayerPreThink`. So is leaving the water over an edge,
  /// which it starts with the flag `WaterJump`, and which is carried on
  /// here.
  ///
  /// The original also levels the view on a slope for a player who does
  /// not look around with the mouse, by the field `idealpitch`. That is
  /// left out.
  class PlayerMovement final
  {
    LevelCollision &_collision;
    QcMachine &_machine;
    QcFields _fields;
    PlayerMovementSettings _settings;

    /// Brings a view that was kicked back, by so many degrees a second.
    void DropPunchAngle(std::int32_t entity, float dt) const;

    /// How far a player leans to the side for moving sideways, in degrees.
    [[nodiscard]] float GetRoll(const LevelVector &angles, const LevelVector &velocity) const;

    /// Slows a player on the ground down, twice as much when the ground
    /// ends a little ahead of where the player goes.
    void ApplyFriction(std::int32_t entity, float dt) const;

    /// Makes a player faster along a direction, up to a speed.
    void Accelerate(std::int32_t entity, float wish_speed, const LevelVector &wish_direction, float dt) const;

    /// The same for a player in the air, who gains along the direction no
    /// more than `air_speed`, while how fast that goes is still by the
    /// speed asked for.
    void AirAccelerate(std::int32_t entity, float wish_speed, const LevelVector &wish_velocity, float dt) const;

    /// A player on the ground, in the air, or flying through walls.
    void MoveOnLand(std::int32_t entity, const PlayerCommand &command, float time, float dt) const;

    /// A player who swims.
    void MoveInWater(std::int32_t entity, const PlayerCommand &command, float dt) const;

    /// A player the game code throws out of the water over an edge: the
    /// player keeps going towards the edge until out of the water, or
    /// until the time for it is over.
    void JumpOutOfWater(std::int32_t entity, float time) const;

  public:
    /// The collision is asked for the ground ahead of a player, and has to
    /// outlive this.
    explicit PlayerMovement(LevelCollision &collision);

    void SetSettings(const PlayerMovementSettings &settings);

    [[nodiscard]] const PlayerMovementSettings &GetSettings() const;

    /// Steers a player for a step of `dt` seconds at the time of the
    /// level, `time`. A host calls it once for each player in every step,
    /// before LevelRunning::Advance().
    ///
    /// It writes where the player looks into `v_angle`, and from that the
    /// `angles` the player's model is shown with: a third of the pitch,
    /// the yaw, and the lean to the side. The game code may have turned the
    /// player, by a teleporter, and says so in `fixangle`: the `angles`
    /// are then left as the game code set them. A host that sees
    /// `fixangle` after a step looks where `angles` say, and sets the
    /// field back to zero.
    void Steer(std::int32_t entity, const PlayerCommand &command, float time, float dt) const;
  };
} // quake

#endif //QUAKE_PLAYER_MOVEMENT_HPP
