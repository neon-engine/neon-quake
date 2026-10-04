#ifndef QUAKE_LEVEL_PHYSICS_HPP
#define QUAKE_LEVEL_PHYSICS_HPP

#include <array>
#include <cstdint>

#include "formats/qc-machine.hpp"
#include "level-collision.hpp"
#include "level-mover.hpp"
#include "level-touching.hpp"
#include "level-trace-result.hpp"
#include "level-vector.hpp"
#include "qc-field.hpp"
#include "qc-flag.hpp"
#include "qc-fields.hpp"

namespace quake
{
  /// The mover of a level that has its collision: it moves what falls,
  /// bounces, and flies, drops a monster that lost its floor, and has a
  /// door or a lift carry what stands on it and stop for what it cannot
  /// push away. It is what the engine of the original does with every
  /// entity in every frame, after the entity has thought.
  ///
  /// How an entity is moved is its `movetype`:
  ///
  /// - `Toss`, `Bounce`, `Fly`, `FlyMissile`: by its velocity, with gravity
  ///   for the first two, until it hits something. Both are then told with
  ///   their `touch`. What is tossed stays where it lands, and what
  ///   bounces goes on until it is slow.
  /// - `Step`, a monster: it falls when it neither stands on the ground
  ///   nor flies nor swims. Its steps are its own, see `LevelStepping`.
  /// - `NoClip`: by its velocity, through everything.
  /// - `None`, and every other: not at all.
  ///
  /// `Walk`, a player, is not moved here at the start. A host moves the
  /// player with the character of its engine, and writes where the player
  /// is into the fields of the entity.
  ///
  /// With SetWalksClients() a player is moved here as well, as the
  /// original moves one: by the velocity `PlayerMovement` gave the player,
  /// with gravity, sliding along walls, up and down steps no higher than
  /// `step_height`, through water. Each of them is noted for the game code:
  /// whether the player stands, on what, and how deep in water.
  ///
  /// An entity that was moved touches the triggers it came into.
  class LevelPhysics final : public LevelMover
  {
    LevelCollision &_collision;
    LevelTouching &_touching;
    QcMachine &_machine;
    QcFields _fields;

    /// By how much of the gravity an entity falls, for a program that has
    /// the field. Zero, and a program without it, is all of it.
    QcField<ProgsType::Float> _gravity_scale;

    float _gravity = 800.0f;
    float _max_velocity = 2000.0f;
    bool _moves_clients = false;
    bool _walks_clients = false;

    [[nodiscard]] bool HasFlag(std::int32_t entity, QcFlag flag) const;

    void SetFlag(std::int32_t entity, QcFlag flag, bool set) const;

    /// Keeps the velocity of an entity a number, and no faster than the
    /// fastest along any axis.
    void CheckVelocity(std::int32_t entity) const;

    void AddGravity(std::int32_t entity, float dt) const;

    /// Moves an entity by a vector as far as it gets, has it touch the
    /// triggers there, and tells it and what it ran into of each other.
    LevelTraceResult PushEntity(std::int32_t entity, const LevelVector &push) const;

    /// Moves an entity by its velocity for a time, sliding along what it
    /// runs into, up to four times. It stands on the ground afterwards
    /// when it came down on a part of the level.
    ///
    /// Gives what it ran into, as bits: 1 for a floor, 2 for what stands
    /// upright, a wall or the side of a step, and both with 4 when it got
    /// stuck. The last upright thing it ran into is written to `wall` when
    /// that is given.
    int FlyMove(std::int32_t entity, float dt, LevelTraceResult *wall = nullptr) const;

    /// Notes whether an entity is in water now, in `watertype` and
    /// `waterlevel`.
    void CheckWaterTransition(std::int32_t entity) const;

    void MoveTossed(std::int32_t entity, float dt) const;

    void MoveStepper(std::int32_t entity, float dt) const;

    void MoveNoClip(std::int32_t entity, float dt) const;

    /// Notes how deep a player is in water, in `waterlevel`: 1 with the
    /// feet in it, 2 up to the waist, 3 over the eyes, and what the water
    /// is in `watertype`. True when the player swims, from the waist on.
    bool CheckWater(std::int32_t entity) const;

    /// Gets a player out of what is solid: back to where the player last
    /// was free, `oldorigin`, or to a place a little off and up to 17
    /// units higher. A player who is free has the place noted for it.
    void CheckStuck(std::int32_t entity) const;

    /// Slows a player down who runs into a wall while looking at it.
    void ApplyWallFriction(std::int32_t entity, const LevelTraceResult &wall) const;

    /// For a player who went up for a step and then got nowhere: the move
    /// is tried again from two units off to each of eight sides. Gives
    /// what the move that got somewhere ran into, as FlyMove() does.
    int TryUnstick(std::int32_t entity, const LevelVector &velocity_before) const;

    /// Moves a player who walks: by the velocity, sliding, and when the
    /// side of a step is in the way and the player stood, the same again
    /// from a step higher, and down onto it.
    void WalkMove(std::int32_t entity, float dt) const;

    /// A frame of a player, by the `movetype`.
    void MoveClient(std::int32_t entity, float dt) const;

  public:
    /// How high a step is that a player walks up.
    static constexpr float step_height = 18.0f;

    /// The collision and the touching have to outlive this.
    LevelPhysics(LevelCollision &collision, LevelTouching &touching);

    /// How fast what falls gets faster, in units a second each second. 800
    /// at the start, the console variable `sv_gravity` of the original.
    void SetGravity(float gravity);

    [[nodiscard]] float GetGravity() const;

    /// The fastest anything moves along an axis. 2000 at the start, the
    /// console variable `sv_maxvelocity` of the original.
    void SetMaxVelocity(float max_velocity);

    [[nodiscard]] float GetMaxVelocity() const;

    /// Whether a door or a lift moves the players, the entities with the
    /// flag `Client`, and is blocked by them, as it is by everything else.
    /// Off at the start: a host that moves the player with the character
    /// of its engine has the engine carry the player and push them away,
    /// and a door would move them a second time.
    void SetMovesClients(bool moves_clients);

    [[nodiscard]] bool GetMovesClients() const;

    /// Whether the players, the entities with the flag `Client`, are moved
    /// here, by their velocity and their `movetype`, as the original moves
    /// them:
    ///
    /// - `Walk`: with gravity, sliding along walls and climbing steps.
    /// - `Toss` and `Bounce`, a player who died: as anything tossed.
    /// - `Fly`: sliding, without gravity. `NoClip`: through everything.
    /// - `None`: not at all.
    ///
    /// The player then touches the triggers the player is in, which is how
    /// a player picks things up and is taken away by a teleporter. A door
    /// or a lift moves such players and is blocked by them, whatever
    /// SetMovesClients() says.
    ///
    /// Off at the start, for a host that moves the player with the
    /// character of its engine. A host that turns it on steers the player
    /// with `PlayerMovement` before each frame, and only reads where the
    /// player is afterwards.
    void SetWalksClients(bool walks_clients);

    [[nodiscard]] bool GetWalksClients() const;

    /// Moves a door or a lift to a new place. Everything that stands on
    /// it, by `groundentity` and the flag `OnGround`, and everything its
    /// new place is inside of, is moved along by as much. When one of them
    /// then stands in what is solid, everything is put back, the
    /// `blocked` of the pusher is called with the obstacle as `other`, and
    /// false is returned. The angles are not looked at: a model of a
    /// level is never turned in what collides.
    bool MovePusher(
      std::int32_t entity,
      const std::array<float, 3> &origin,
      const std::array<float, 3> &angles,
      float dt) override;

    void MoveEntity(std::int32_t entity, float dt) override;
  };
} // quake

#endif //QUAKE_LEVEL_PHYSICS_HPP
