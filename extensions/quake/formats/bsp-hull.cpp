#include "bsp-hull.hpp"

#include <array>
#include <utility>

namespace quake
{
  // Helpers of BspHull: the forks of a level read as forks of a hull, the
  // side of a plane a place lies on, and the walk of a move down the tree.
  namespace
  {
    constexpr auto empty = static_cast<std::int32_t>(BspContents::Empty);
    constexpr auto solid = static_cast<std::int32_t>(BspContents::Solid);
    constexpr auto water = static_cast<std::int32_t>(BspContents::Water);
    constexpr auto sky = static_cast<std::int32_t>(BspContents::Sky);

    /// Water that flows to one of six sides, which is water to the game.
    constexpr std::int32_t first_current = -9;
    constexpr std::int32_t last_current = -14;

    /// The boxes the hulls of a level were grown by.
    constexpr std::array<BspVector, BspHull::hull_count> hull_mins = {{
      {0.0f, 0.0f, 0.0f},
      {-16.0f, -16.0f, -24.0f},
      {-32.0f, -32.0f, -24.0f},
    }};
    constexpr std::array<BspVector, BspHull::hull_count> hull_maxs = {{
      {0.0f, 0.0f, 0.0f},
      {16.0f, 16.0f, 32.0f},
      {32.0f, 32.0f, 64.0f},
    }};

    bool refuse(std::string &error, std::string reason)
    {
      error = std::move(reason);
      return false;
    }

    std::string names_outside(
      const std::string &who, const std::string &what, const std::int64_t number, const std::size_t count)
    {
      return who + " names " + what + " " + std::to_string(number) + ", and there are " + std::to_string(count);
    }

    /// Takes what fills a place as the file has it, with water that flows
    /// made water. False when the game does not know the number.
    bool read_contents(const std::int32_t value, const std::string &who, std::int32_t &contents, std::string &error)
    {
      if (value <= first_current && value >= last_current)
      {
        contents = water;
        return true;
      }
      if (value > empty || value < sky)
      {
        return refuse(error, who + " is filled with " + std::to_string(value) + ", which the game does not know");
      }
      contents = value;
      return true;
    }

    /// What fills a leaf of the tree a level is drawn by, named the way a
    /// node names it: -1 is leaf 0.
    bool read_leaf(
      const BspFile &file, const std::int32_t child, const std::string &who, std::int32_t &contents,
      std::string &error)
    {
      const std::int64_t leaf = -(static_cast<std::int64_t>(child) + 1);
      if (static_cast<std::size_t>(leaf) >= file.leaves.size())
      {
        return refuse(error, names_outside(who, "leaf", leaf, file.leaves.size()));
      }
      return read_contents(
        file.leaves[static_cast<std::size_t>(leaf)].contents, "leaf " + std::to_string(leaf), contents, error);
    }

    /// Reads a node or a clip node of the level as a fork of a hull. A child
    /// that is a fork is still the number the level has for it; a child that
    /// is a leaf is what fills the leaf.
    bool read_fork(
      const BspFile &file, const bool of_nodes, const std::size_t index, BspHullNode &fork, std::string &error)
    {
      const std::string who = (of_nodes ? "node " : "clip node ") + std::to_string(index);
      const std::size_t count = of_nodes ? file.nodes.size() : file.clip_nodes.size();
      const std::int32_t plane = of_nodes ? file.nodes[index].plane : file.clip_nodes[index].plane;
      const std::array<std::int32_t, 2> &children =
        of_nodes ? file.nodes[index].children : file.clip_nodes[index].children;

      if (plane < 0 || static_cast<std::size_t>(plane) >= file.planes.size())
      {
        return refuse(error, names_outside(who, "plane", plane, file.planes.size()));
      }
      fork.normal = file.planes[static_cast<std::size_t>(plane)].normal;
      fork.distance = file.planes[static_cast<std::size_t>(plane)].distance;

      for (std::size_t side = 0; side < 2; side++)
      {
        // a child that is not negative is a fork, whatever form the file
        // of the level had: `BspFile` made it so when it read the file
        const std::int32_t child = children[side];
        if (child >= 0)
        {
          if (static_cast<std::size_t>(child) >= count)
          {
            return refuse(error, names_outside(who, of_nodes ? "node" : "clip node", child, count));
          }
          fork.children[side] = child;
        }
        else if (of_nodes)
        {
          if (!read_leaf(file, child, who, fork.children[side], error)) { return false; }
        }
        else if (!read_contents(child, "the space at " + who, fork.children[side], error))
        {
          return false;
        }
      }
      return true;
    }

    /// How far a place is in front of the plane of a fork, negative when it
    /// is behind. It is counted with more digits than the places have and
    /// then cut down to theirs, so that a slanted plane does not put a place
    /// on its wrong side by rounding.
    float distance_in_front(const BspHullNode &node, const BspVector &point)
    {
      return static_cast<float>(
        static_cast<double>(node.normal.x) * static_cast<double>(point.x) +
        static_cast<double>(node.normal.y) * static_cast<double>(point.y) +
        static_cast<double>(node.normal.z) * static_cast<double>(point.z) -
        static_cast<double>(node.distance));
    }

    /// What fills a place, looked for from a fork or a child of one down.
    std::int32_t contents_at(const std::span<const BspHullNode> nodes, std::int32_t at, const BspVector &point)
    {
      while (at >= 0)
      {
        const BspHullNode &node = nodes[static_cast<std::size_t>(at)];
        at = node.children[distance_in_front(node, point) < 0.0f ? 1 : 0];
      }
      return at;
    }

    /// The place a part of the way from one place to another.
    BspVector between(const BspVector &start, const BspVector &end, const float part)
    {
      return {
        start.x + part * (end.x - start.x),
        start.y + part * (end.y - start.y),
        start.z + part * (end.z - start.z),
      };
    }

    /// Walks a piece of the move down the tree, from the fork or child `at`.
    /// The piece goes from `start` to `end`, which are the fractions
    /// `start_fraction` and `end_fraction` of the whole move. False when the
    /// move was stopped in this piece, and the result says where.
    ///
    /// It calls itself for the half of a piece that lies on the side of a
    /// plane the piece starts on, and goes on with the other half in its own
    /// loop, so it is never deeper in calls than the tree is in forks.
    bool walk(
      const std::span<const BspHullNode> nodes,
      const std::int32_t head,
      std::int32_t at,
      float start_fraction,
      const float end_fraction,
      BspVector start,
      const BspVector &end,
      BspTraceResult &result)
    {
      for (;;)
      {
        // the end of the tree: the whole piece is in one thing
        if (at < 0)
        {
          if (at == solid)
          {
            result.start_solid = true;
            return true;
          }
          result.all_solid = false;
          if (at == empty) { result.in_open = true; }
          else { result.in_water = true; }
          return true;
        }

        const BspHullNode &node = nodes[static_cast<std::size_t>(at)];
        const float start_distance = distance_in_front(node, start);
        const float end_distance = distance_in_front(node, end);

        // the piece lies on one side whole
        if (start_distance >= 0.0f && end_distance >= 0.0f)
        {
          at = node.children[0];
          continue;
        }
        if (start_distance < 0.0f && end_distance < 0.0f)
        {
          at = node.children[1];
          continue;
        }

        // it crosses the plane, and is cut a little before it, on the side
        // it starts on
        const std::size_t side = start_distance < 0.0f ? 1 : 0;
        float part = side == 1
          ? (start_distance + BspHull::distance_epsilon) / (start_distance - end_distance)
          : (start_distance - BspHull::distance_epsilon) / (start_distance - end_distance);
        if (part < 0.0f) { part = 0.0f; }
        if (part > 1.0f) { part = 1.0f; }

        float middle_fraction = start_fraction + (end_fraction - start_fraction) * part;
        BspVector middle = between(start, end, part);

        // the half on the side it starts on
        if (!walk(nodes, head, node.children[side], start_fraction, middle_fraction, start, middle, result))
        {
          return false;
        }

        // the other half, when the move can go on there
        const std::int32_t far_child = node.children[side ^ 1];
        if (contents_at(nodes, far_child, middle) != solid)
        {
          at = far_child;
          start_fraction = middle_fraction;
          start = middle;
          continue;
        }

        // behind the plane it is solid. A move that never left what is
        // solid is not stopped by it: it was never out
        if (result.all_solid) { return false; }

        // the plane stops the move, and looks to where the move came from
        if (side == 0)
        {
          result.plane_normal = node.normal;
          result.plane_distance = node.distance;
        }
        else
        {
          result.plane_normal = {-node.normal.x, -node.normal.y, -node.normal.z};
          result.plane_distance = -node.distance;
        }

        // the place before the plane may count as solid all the same, at a
        // corner or by rounding: then the move ends a tenth of the piece
        // earlier, until it is out or the piece is used up. A part that is
        // no number, which a place that is none makes, ends it too: asked
        // the other way around it would never end
        while (contents_at(nodes, head, middle) == solid)
        {
          part -= 0.1f;
          if (!(part >= 0.0f)) { break; }
          middle_fraction = start_fraction + (end_fraction - start_fraction) * part;
          middle = between(start, end, part);
        }

        result.fraction = middle_fraction;
        result.end_position = middle;
        return false;
      }
    }
  }

  bool BspHull::Build(const BspFile &file, const std::size_t model, const std::size_t hull, std::string &error)
  {
    if (model >= file.models.size())
    {
      return refuse(error, "model " + std::to_string(model) + " was asked for, and there are " +
        std::to_string(file.models.size()));
    }
    if (hull >= hull_count)
    {
      return refuse(error, "hull " + std::to_string(hull) + " was asked for, and there are " +
        std::to_string(hull_count));
    }

    // hull 0 is the tree the level is drawn by, the others are clip nodes
    const bool of_nodes = hull == 0;
    const std::size_t count = of_nodes ? file.nodes.size() : file.clip_nodes.size();
    const std::string who = "hull " + std::to_string(hull) + " of model " + std::to_string(model);
    const std::int32_t head = file.models[model].head_nodes[hull];

    BspHull made;
    made._mins = hull_mins[hull];
    made._maxs = hull_maxs[hull];

    if (head < 0)
    {
      // a tree without a fork: one thing fills everything
      if (of_nodes)
      {
        if (!read_leaf(file, head, who, made._head, error)) { return false; }
      }
      else if (!read_contents(head, who, made._head, error))
      {
        return false;
      }
      *this = std::move(made);
      return true;
    }

    if (static_cast<std::size_t>(head) >= count)
    {
      return refuse(error, names_outside(who, of_nodes ? "node" : "clip node", head, count));
    }

    // The forks are taken level by level of the tree. `order` says which
    // fork of the level each fork of the hull is, and `depth` how many
    // forks lie on the way to it, itself counted.
    std::vector<std::size_t> order{static_cast<std::size_t>(head)};
    std::vector<std::size_t> depth{1};
    std::vector<bool> reached(count, false);
    reached[static_cast<std::size_t>(head)] = true;
    made._head = 0;

    for (std::size_t i = 0; i < order.size(); i++)
    {
      BspHullNode fork;
      if (!read_fork(file, of_nodes, order[i], fork, error)) { return false; }

      for (std::int32_t &child : fork.children)
      {
        if (child < 0) { continue; }
        const auto source = static_cast<std::size_t>(child);

        // a tree comes to every fork one way. One that is come to twice
        // may be a loop, which a move would never leave
        if (reached[source])
        {
          return refuse(error, who + " reaches " + (of_nodes ? "node " : "clip node ") +
            std::to_string(source) + " twice");
        }
        if (depth[i] >= deepest_tree)
        {
          return refuse(error, who + " is more than " + std::to_string(deepest_tree) + " forks deep");
        }
        reached[source] = true;
        child = static_cast<std::int32_t>(order.size());
        order.push_back(source);
        depth.push_back(depth[i] + 1);
      }
      made._nodes.push_back(fork);
    }

    *this = std::move(made);
    return true;
  }

  void BspHull::BuildBox(const BspVector &mins, const BspVector &maxs)
  {
    // Six forks, one after another, as the original has them: the far side
    // of an axis, then its near side. What is outside a plane is empty, and
    // what is inside all six is solid.
    const std::array<float, 6> distances = {maxs.x, mins.x, maxs.y, mins.y, maxs.z, mins.z};
    _nodes.assign(distances.size(), {});
    for (std::size_t i = 0; i < distances.size(); i++)
    {
      BspHullNode &node = _nodes[i];
      const std::size_t axis = i / 2;
      node.normal = {axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f};
      node.distance = distances[i];

      // in front of a far side and behind a near side is outside
      const std::size_t outside = i % 2;
      node.children[outside] = empty;
      node.children[outside ^ 1] = i + 1 < distances.size() ? static_cast<std::int32_t>(i + 1) : solid;
    }
    _head = 0;
    _mins = {};
    _maxs = {};
  }

  BspContents BspHull::GetPointContents(const BspVector &point) const
  {
    return static_cast<BspContents>(contents_at(_nodes, _head, point));
  }

  BspTraceResult BspHull::TraceLine(const BspVector &start, const BspVector &end) const
  {
    BspTraceResult result;

    // solid until a piece of the move is found that is not
    result.all_solid = true;
    result.end_position = end;
    walk(_nodes, _head, _head, 0.0f, 1.0f, start, end, result);
    return result;
  }
} // quake
