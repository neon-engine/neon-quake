#ifndef QUAKE_LEVEL_MOVER_HPP
#define QUAKE_LEVEL_MOVER_HPP

#include <array>
#include <cstdint>

namespace quake
{
  /// What a host that knows the shape of the level does when `LevelRunning`
  /// lets time pass: it says whether a door may move, and moves what falls,
  /// flies, and walks.
  ///
  /// `LevelRunning` knows no walls. Without a mover a door moves through
  /// whatever is in its way, and nothing else moves at all. Both functions
  /// are called while a frame runs, and may read and write the entities,
  /// and call the game code through LevelRunning::RunFunction().
  class LevelMover
  {
  public:
    virtual ~LevelMover() = default;

    /// What pushes, a door or a lift, is about to move from where its
    /// fields say it is to a new place and new angles, in `dt` seconds of
    /// its own clock.
    ///
    /// True lets it: `LevelRunning` then writes the place and the angles,
    /// and moves the entity's clock on. Before returning true a host moves
    /// what rides on the entity and what it pushes away. False says that
    /// something it cannot push is in the way: the entity stays, and so
    /// does its clock, so that it thinks as much later. The host has then
    /// called its function `blocked`.
    virtual bool MovePusher(
      std::int32_t entity,
      const std::array<float, 3> &origin,
      const std::array<float, 3> &angles,
      float dt)
    {
      return true;
    }

    /// Moves an entity that is not one that pushes for `dt` seconds, by its
    /// `movetype`: gravity, its velocity, what it runs into and touches. It
    /// is called for every such entity in every frame, after the entity has
    /// thought, unless the thought removed it.
    virtual void MoveEntity(std::int32_t entity, float dt)
    {
    }
  };
} // quake

#endif //QUAKE_LEVEL_MOVER_HPP
