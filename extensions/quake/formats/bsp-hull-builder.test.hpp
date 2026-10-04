#ifndef QUAKE_BSP_HULL_BUILDER_TEST_HPP
#define QUAKE_BSP_HULL_BUILDER_TEST_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "bsp-contents.hpp"
#include "bsp-file.hpp"

namespace quake
{
  /// Puts the trees of a level together in memory, for the tests of what
  /// things collide with: a test names planes and what lies in front of
  /// them and behind them, and gets a level that has those trees and
  /// nothing else.
  ///
  /// It starts with leaf 0, which is solid as in every level, and a leaf 1
  /// that is empty. The level is public, so that a test can also make one
  /// that is wrong.
  class BspHullBuilder
  {
  public:
    static constexpr auto empty = static_cast<std::int32_t>(BspContents::Empty);
    static constexpr auto solid = static_cast<std::int32_t>(BspContents::Solid);
    static constexpr auto water = static_cast<std::int32_t>(BspContents::Water);

    /// What a node names the two leaves the builder starts with by.
    static constexpr std::int32_t solid_leaf = -1;
    static constexpr std::int32_t empty_leaf = -2;

    BspFile file;

    BspHullBuilder()
    {
      file.leaves.resize(2);
      file.leaves[0].contents = solid;
      file.leaves[1].contents = empty;
    }

    /// Adds a plane and gives its number.
    std::int32_t Plane(const BspVector &normal, const float distance)
    {
      BspPlane plane;
      plane.normal = normal;
      plane.distance = distance;
      file.planes.push_back(plane);
      return static_cast<std::int32_t>(file.planes.size() - 1);
    }

    /// Adds a clip node with a plane of its own and gives its number, which
    /// is what another clip node or a model names it by.
    std::int32_t ClipNode(
      const BspVector &normal, const float distance, const std::int32_t front, const std::int32_t back)
    {
      BspClipNode node;
      node.plane = Plane(normal, distance);
      node.children = {front, back};
      file.clip_nodes.push_back(node);
      return static_cast<std::int32_t>(file.clip_nodes.size() - 1);
    }

    /// Adds the six clip nodes of a box with sides along the axes, and
    /// gives the one the tree of the box starts at. A place within the box
    /// comes to `inside`, a clip node or what fills the space, and every
    /// other to `outside`, which is what fills the space: a clip node there
    /// would be reached six ways, and a tree has one way to each.
    std::int32_t Box(
      const BspVector &mins, const BspVector &maxs, const std::int32_t inside, const std::int32_t outside)
    {
      // from the last plane to the first, since a clip node names the next
      std::int32_t next = inside;
      next = ClipNode({0.0f, 0.0f, 1.0f}, maxs.z, outside, next);
      next = ClipNode({0.0f, 0.0f, 1.0f}, mins.z, next, outside);
      next = ClipNode({0.0f, 1.0f, 0.0f}, maxs.y, outside, next);
      next = ClipNode({0.0f, 1.0f, 0.0f}, mins.y, next, outside);
      next = ClipNode({1.0f, 0.0f, 0.0f}, maxs.x, outside, next);
      next = ClipNode({1.0f, 0.0f, 0.0f}, mins.x, next, outside);
      return next;
    }

    /// Adds a leaf filled with this and gives what a node names it by.
    std::int32_t Leaf(const std::int32_t contents)
    {
      BspLeaf leaf;
      leaf.contents = contents;
      file.leaves.push_back(leaf);
      return -static_cast<std::int32_t>(file.leaves.size());
    }

    /// Adds a node of the tree a level is drawn by, with a plane of its
    /// own, and gives its number. A child is a node, or a leaf as Leaf()
    /// names it.
    std::int32_t Node(
      const BspVector &normal, const float distance, const std::int32_t front, const std::int32_t back)
    {
      BspNode node;
      node.plane = Plane(normal, distance);
      node.children = {front, back};
      file.nodes.push_back(node);
      return static_cast<std::int32_t>(file.nodes.size() - 1);
    }

    /// Adds a model whose three hulls start at these, and gives its number.
    /// The first is a node or a leaf, the others a clip node or what fills
    /// the space.
    std::size_t Model(const std::int32_t point, const std::int32_t player, const std::int32_t large)
    {
      BspModel model;
      model.head_nodes = {point, player, large, 0};
      file.models.push_back(model);
      return file.models.size() - 1;
    }
  };
} // quake

#endif //QUAKE_BSP_HULL_BUILDER_TEST_HPP
