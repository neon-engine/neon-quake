#ifndef QUAKE_BSP_COLLISION_HPP
#define QUAKE_BSP_COLLISION_HPP

#include <array>
#include <cstddef>
#include <string>

#include "bsp-contents.hpp"
#include "bsp-file.hpp"
#include "bsp-hull.hpp"
#include "bsp-trace-result.hpp"
#include "bsp-vector.hpp"

namespace quake
{
  /// What things collide with of a model of a level: its three hulls, and
  /// the moves of a box through them.
  ///
  /// This is what the game code asks with `traceline`, `pointcontents`,
  /// `droptofloor`, `walkmove`, and `checkbottom`, and what stops a player
  /// at a wall of a brush that is only there to be walked into.
  ///
  /// It is made from a `BspFile` and keeps nothing of it. Everything is in
  /// the units and axes of the game, with Z pointing up.
  ///
  /// A model stands at the origin of its entity and is never turned: the
  /// original game turns a model of a level in one of its mission packs
  /// only, and that is not done here.
  class BspCollision
  {
    std::array<BspHull, BspHull::hull_count> _hulls;

  public:
    /// Makes the hulls of a model of a level. Model 0 is the world. Returns
    /// false, says why in `error`, and stays as it was when there is no such
    /// model or when one of its hulls is refused, see `BspHull::Build`.
    bool Build(const BspFile &file, std::size_t model, std::string &error);

    [[nodiscard]] const BspHull &GetHull(std::size_t hull) const;

    /// The number of the hull the game moves a box of this size through. It
    /// looks at the width along X alone: under 3 units it is a point, hull
    /// 0, up to 32 units it is the player, hull 1, and above it is hull 2.
    ///
    /// The hull is used as it is, whatever the box: a box smaller than the
    /// player is stopped where the player is, counted from its lowest
    /// corner.
    [[nodiscard]] static std::size_t ChooseHull(const BspVector &mins, const BspVector &maxs);

    /// What fills a place, by hull 0. The place is in the space of the
    /// model: for the world that is the place itself.
    [[nodiscard]] BspContents GetPointContents(const BspVector &point) const;

    /// Moves a box from `start` to `end` and says how far it came.
    ///
    /// `origin` is where the model stands, the origin of its entity: zero
    /// for the world, and where a door or a lift is at the time. `mins` and
    /// `maxs` are the corners of the box around the place that is moved,
    /// and both zero for a point. `start`, `end`, and the result are in the
    /// world.
    ///
    /// The hull is the one of ChooseHull(), and the move is counted from
    /// the lowest corner of the box: the point that is moved through the
    /// hull is shifted by the lowest corner of the hull less `mins`, as in
    /// the original game.
    ///
    /// The end is the place the trace of the hull found, shifted back, and
    /// exactly `end` when nothing stopped the move.
    [[nodiscard]] BspTraceResult TraceBox(
      const BspVector &origin,
      const BspVector &start,
      const BspVector &mins,
      const BspVector &maxs,
      const BspVector &end) const;
  };
} // quake

#endif //QUAKE_BSP_COLLISION_HPP
