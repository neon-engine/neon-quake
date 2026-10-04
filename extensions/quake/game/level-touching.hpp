#ifndef QUAKE_LEVEL_TOUCHING_HPP
#define QUAKE_LEVEL_TOUCHING_HPP

#include <cstdint>

#include "formats/qc-machine.hpp"
#include "level-caller.hpp"
#include "level-collision.hpp"
#include "qc-fields.hpp"
#include "qc-globals.hpp"

namespace quake
{
  /// Tells the game code that entities met, as the engine of the original
  /// does when it has moved one: the function `touch` of an entity is
  /// called with the entity as `self` and whom it met as `other`.
  ///
  /// The globals `self` and `other` are put back as they were after every
  /// call, since entities meet in the middle of a function of the game
  /// code, when a monster takes a step. A function may remove an entity or
  /// move one, so whoever asks looks at the entities anew afterwards.
  class LevelTouching final
  {
    LevelCollision &_collision;
    LevelCaller &_caller;
    QcMachine &_machine;
    QcFields _fields;
    QcGlobals _globals;

    /// Calls a function for two entities and puts `self` and `other` back.
    void Call(std::int32_t function, std::int32_t self, std::int32_t other) const;

  public:
    /// The collision is asked for the boxes of the entities, and the
    /// caller runs the functions. Both have to outlive this.
    LevelTouching(LevelCollision &collision, LevelCaller &caller);

    /// Two entities ran into each other. Each that has a `touch` and whose
    /// `solid` is not `Not` is told, the first one first.
    void Impact(std::int32_t first, std::int32_t second) const;

    /// An entity has moved: every entity whose `solid` is `Trigger`, that
    /// has a `touch`, and whose box overlaps the box of the one that moved
    /// is told that it was touched, in the order of their numbers.
    void TouchTriggers(std::int32_t entity) const;

    /// Writes the box of an entity that has moved into its fields, see
    /// LevelCollision::Link(), and has it touch the triggers it is in when
    /// asked to.
    void Link(std::int32_t entity, bool touch_triggers) const;

    /// Tells an entity that pushes that another is in its way, with its
    /// function `blocked`.
    void Block(std::int32_t pusher, std::int32_t obstacle) const;
  };
} // quake

#endif //QUAKE_LEVEL_TOUCHING_HPP
