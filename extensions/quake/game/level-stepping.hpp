#ifndef QUAKE_LEVEL_STEPPING_HPP
#define QUAKE_LEVEL_STEPPING_HPP

#include <cstdint>
#include <functional>

#include "formats/qc-machine.hpp"
#include "level-collision.hpp"
#include "level-touching.hpp"
#include "level-vector.hpp"
#include "qc-fields.hpp"

namespace quake
{
  /// How a monster walks, as the engine of the original moves it for the
  /// builtins `walkmove` and `movetogoal`: a step at a time, each a move
  /// along the ground that climbs what is no higher than `step_height` and
  /// refuses to leave the floor.
  ///
  /// A monster that walks keeps to what it stands on: a step that would
  /// leave it over an edge is not taken. One that flies or swims, with the
  /// flag `Fly` or `Swim`, moves through the air or the water, and drifts
  /// to the height of its enemy.
  ///
  /// Everything is in the units and axes of the game, and an angle is the
  /// yaw in degrees, counted from the X axis towards the Y axis.
  class LevelStepping final
  {
  public:
    /// A number from 0 to 1 for what a monster does by chance, as the
    /// builtin `random` gives one.
    using Random = std::function<float()>;

  private:
    LevelCollision &_collision;
    LevelTouching &_touching;
    QcMachine &_machine;
    QcFields _fields;
    Random _random;

    [[nodiscard]] bool HasAnyFlag(std::int32_t entity, std::int32_t flags) const;

    /// Whether an entity may take steps at all: it stands on the ground,
    /// flies, or swims.
    [[nodiscard]] bool CanStep(std::int32_t entity) const;

    /// A step of a monster that flies or swims.
    bool MoveThroughTheOpen(std::int32_t entity, const LevelVector &move, bool relink) const;

  public:
    /// How high a step is that a monster walks up or down.
    static constexpr float step_height = 18.0f;

    /// The collision and the touching have to outlive this. `random` is
    /// asked whenever a monster chooses by chance, and may be empty, which
    /// is a monster that always chooses the same.
    LevelStepping(LevelCollision &collision, LevelTouching &touching, Random random);

    /// Whether an entity has a floor under all of it: under each corner of
    /// its box there is something solid within a step's height of what is
    /// under its middle. `checkbottom` asks it, and a step that would end
    /// without it is not taken.
    [[nodiscard]] bool CheckBottom(std::int32_t entity) const;

    /// Moves an entity by a step along the ground, or through the open for
    /// one that flies or swims. False, with the entity where it was, when
    /// the step runs into something, climbs higher than `step_height`, or
    /// leaves the floor. `relink` writes the box of the entity anew and
    /// has it touch the triggers it came into.
    bool MoveStep(std::int32_t entity, const LevelVector &move, bool relink) const;

    /// `walkmove`: a step of a length in a direction, for an entity that
    /// stands on the ground, flies, or swims. False for any other, and
    /// when the step is not taken.
    bool WalkMove(std::int32_t entity, float yaw, float distance) const;

    /// `ChangeYaw`: turns an entity towards its `ideal_yaw`, by no more
    /// than its `yaw_speed`, the shorter way round.
    void ChangeYaw(std::int32_t entity) const;

    /// Turns an entity to a direction and has it take a step that way.
    /// The step is taken back when the entity has not yet turned far
    /// enough: it turns first and walks then. That is counted as the
    /// original counts it, the yaw less the direction being between 45
    /// and 315, which holds a turn to the right back and lets one to the
    /// left walk at once. False when the step runs into something.
    bool StepDirection(std::int32_t entity, float yaw, float distance) const;

    /// Finds a direction for an entity to walk in towards another, when
    /// the way it went is blocked: straight at the other, then along one
    /// axis, then the way it went before, then any of the eight, and last
    /// back where it came from. The direction that works is its
    /// `ideal_yaw` afterwards.
    void ChooseDirection(std::int32_t entity, std::int32_t goal, float distance) const;

    /// Whether the boxes of two entities are within a distance of each
    /// other along every axis.
    [[nodiscard]] bool IsCloseEnough(std::int32_t entity, std::int32_t goal, float distance) const;

    /// `movetogoal`: a step towards the `goalentity` of an entity that
    /// stands on the ground, flies, or swims. It does nothing for one that
    /// has an enemy and is close enough to its goal. Otherwise it walks on
    /// the way it faces, and looks for another way when that is blocked,
    /// and now and then by chance.
    void MoveToGoal(std::int32_t entity, float distance) const;
  };
} // quake

#endif //QUAKE_LEVEL_STEPPING_HPP
