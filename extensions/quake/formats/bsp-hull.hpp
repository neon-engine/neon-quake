#ifndef QUAKE_BSP_HULL_HPP
#define QUAKE_BSP_HULL_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "bsp-contents.hpp"
#include "bsp-file.hpp"
#include "bsp-hull-node.hpp"
#include "bsp-trace-result.hpp"
#include "bsp-vector.hpp"

namespace quake
{
  /// One of the trees of a model of a level that things collide with: for
  /// every place it says what fills it.
  ///
  /// The game never moves a box through a level. It moves a point through a
  /// level whose walls were grown by the size of the box, when the level was
  /// compiled, and a level carries three such trees, the hulls:
  ///
  /// - hull 0, for a point. It is the tree the level is drawn by, whose
  ///   leaves say what fills them.
  /// - hull 1, for a box the size of the player: from -16 -16 -24 to
  ///   16 16 32 around its origin.
  /// - hull 2, for a box the size of a large monster: from -32 -32 -24 to
  ///   32 32 64.
  ///
  /// Hull 0 is made of nodes and leaves and the others of clip nodes. Here
  /// all three are the same: forks whose children are a fork or what fills
  /// the space, as the game makes them the same when it loads a level.
  ///
  /// A hull is made from a `BspFile` and keeps nothing of it. A hull that
  /// was made is whole: every child is there, no fork is reached twice, and
  /// the tree is no deeper than `deepest_tree`. So a trace never reads what
  /// is not there and always ends, whatever the file was.
  ///
  /// Everything is in the units and axes of the game, with Z pointing up,
  /// and in the space of the model: where the model stands in the world is
  /// for `BspCollision` to count in.
  class BspHull
  {
    std::vector<BspHullNode> _nodes;

    /// Where the tree starts: a node, or what fills everything when it is
    /// negative.
    std::int32_t _head = static_cast<std::int32_t>(BspContents::Empty);

    BspVector _mins;
    BspVector _maxs;

  public:
    /// How many hulls of a model the game uses. A level has room for a
    /// fourth, which it never fills.
    static constexpr std::size_t hull_count = 3;

    /// How far in front of a plane a move is stopped, so that it does not
    /// end on the plane itself and count as behind it the next time. It is
    /// the number of the original game, which the game code was tuned on.
    static constexpr float distance_epsilon = 0.03125f;

    /// How many forks deep a tree may be. A trace goes as deep in calls of
    /// itself, so this keeps it from running out of stack. The levels of
    /// the game are far shallower.
    static constexpr std::size_t deepest_tree = 1024;

    /// Makes a hull of a model of a level. Model 0 is the world. Returns
    /// false, says why in `error`, and stays as it was when there is no such
    /// model or hull, when a fork names a plane, a fork, or a leaf that is
    /// not there, when something fills a place that the game does not know,
    /// when a fork is reached twice, which a tree that loops does, or when
    /// the tree is too deep.
    bool Build(const BspFile &file, std::size_t model, std::size_t hull, std::string &error);

    /// Makes the hull a box with sides along the axes: solid inside, empty
    /// around it. The original game collides with an entity that is no part
    /// of the level this way, as six planes, so that a move is stopped by
    /// the box of an entity exactly as it is by a wall. The box the level
    /// was grown by is zero: whoever asks grows the box itself.
    void BuildBox(const BspVector &mins, const BspVector &maxs);

    /// The forks, in an order of their own: a child is a place in here.
    [[nodiscard]] std::span<const BspHullNode> GetNodes() const
    {
      return _nodes;
    }

    /// Where the tree starts: a node, or a value of `BspContents` when the
    /// whole hull is filled with one thing.
    [[nodiscard]] std::int32_t GetHead() const
    {
      return _head;
    }

    /// The box the level was grown by for this hull, around the origin of
    /// what moves. Both are zero for hull 0.
    [[nodiscard]] const BspVector &GetMins() const
    {
      return _mins;
    }

    [[nodiscard]] const BspVector &GetMaxs() const
    {
      return _maxs;
    }

    /// What fills a place. A place that lies on a plane counts as in front
    /// of it.
    [[nodiscard]] BspContents GetPointContents(const BspVector &point) const;

    /// Moves a point from `start` to `end` and says how far it came, the way
    /// the original game does it: the move is stopped `distance_epsilon` in
    /// front of the first plane behind which the hull is solid, and when
    /// that place counts as solid all the same, it is moved back a tenth of
    /// the way at a time.
    ///
    /// A move that starts in what is solid goes on until it is out of it and
    /// is stopped by the next thing that is solid; `start_solid` says so.
    [[nodiscard]] BspTraceResult TraceLine(const BspVector &start, const BspVector &end) const;
  };
} // quake

#endif //QUAKE_BSP_HULL_HPP
