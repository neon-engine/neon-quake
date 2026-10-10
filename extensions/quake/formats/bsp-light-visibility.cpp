#include "bsp-light-visibility.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace quake
{
  // Helpers of BspLightVisibility, for this file alone.
  namespace
  {
    /// How far a place is from a box, 0 inside of it.
    float distance_to_box(const BspVector &place, const std::array<float, 3> &mins, const std::array<float, 3> &maxs)
    {
      const float at[3] = {place.x, place.y, place.z};
      float sum = 0.0f;
      for (int axis = 0; axis < 3; axis++)
      {
        const float outside = at[axis] < mins[axis] ? mins[axis] - at[axis] : at[axis] > maxs[axis] ? at[axis] - maxs[axis] : 0.0f;
        sum += outside * outside;
      }
      return std::sqrt(sum);
    }
  }

  bool BspLightVisibility::Build(const BspFile &file, std::string &error)
  {
    if (file.models.empty())
    {
      error = "the level has no world";
      return false;
    }

    std::vector<BspLightNode> forks;
    forks.reserve(file.nodes.size());
    for (std::size_t i = 0; i < file.nodes.size(); i++)
    {
      const BspNode &node = file.nodes[i];
      if (node.plane < 0 || static_cast<std::size_t>(node.plane) >= file.planes.size())
      {
        error = "fork " + std::to_string(i) + " lies on plane " + std::to_string(node.plane) + ", which is not there";
        return false;
      }
      for (const std::int32_t child : node.children)
      {
        if (child >= 0 && static_cast<std::size_t>(child) >= file.nodes.size())
        {
          error = "fork " + std::to_string(i) + " names fork " + std::to_string(child) + ", which is not there";
          return false;
        }
        if (child < 0 && static_cast<std::size_t>(-(child + 1)) >= file.leaves.size())
        {
          error = "fork " + std::to_string(i) + " names leaf " + std::to_string(-(child + 1)) + ", which is not there";
          return false;
        }
      }

      const BspPlane &plane = file.planes[static_cast<std::size_t>(node.plane)];
      BspLightNode fork;
      fork.normal = plane.normal;
      fork.distance = plane.distance;
      fork.children = node.children;
      forks.push_back(fork);
    }

    const std::int32_t head = file.models[0].head_nodes[0];
    if (head >= 0 && static_cast<std::size_t>(head) >= file.nodes.size())
    {
      error = "the world starts at fork " + std::to_string(head) + ", which is not there";
      return false;
    }
    if (head < 0 && static_cast<std::size_t>(-(head + 1)) >= file.leaves.size())
    {
      error = "the world starts at leaf " + std::to_string(-(head + 1)) + ", which is not there";
      return false;
    }

    std::vector<Leaf> leaves;
    leaves.reserve(file.leaves.size());
    for (std::size_t i = 0; i < file.leaves.size(); i++)
    {
      const BspLeaf &from = file.leaves[i];
      if (static_cast<std::size_t>(from.first_leaf_face) + from.leaf_face_count > file.leaf_faces.size())
      {
        error = "leaf " + std::to_string(i) + " names faces of leaves that are not there";
        return false;
      }
      for (std::uint32_t j = 0; j < from.leaf_face_count; j++)
      {
        if (file.leaf_faces[from.first_leaf_face + j] >= file.faces.size())
        {
          error = "leaf " + std::to_string(i) + " names face " + std::to_string(file.leaf_faces[from.first_leaf_face + j]) +
                  ", which is not there";
          return false;
        }
      }

      Leaf leaf;
      leaf.visibility_offset = from.visibility_offset;
      leaf.mins = from.mins;
      leaf.maxs = from.maxs;
      leaf.first_face = from.first_leaf_face;
      leaf.face_count = from.leaf_face_count;
      leaves.push_back(leaf);
    }

    _forks = std::move(forks);
    _head = head;
    _leaves = std::move(leaves);
    _leaf_faces = file.leaf_faces;
    _visibility = file.visibility;
    _face_count = file.faces.size();
    return true;
  }

  std::size_t BspLightVisibility::FindLeaf(const BspVector &place) const
  {
    // Every fork was checked to name what is there. A tree that goes round
    // would never end, so no more steps are taken than there are forks.
    std::int32_t at = _head;
    for (std::size_t steps = 0; at >= 0 && steps <= _forks.size(); steps++)
    {
      const BspLightNode &fork = _forks[static_cast<std::size_t>(at)];
      const float side = fork.normal.x * place.x + fork.normal.y * place.y + fork.normal.z * place.z - fork.distance;
      at = fork.children[side >= 0.0f ? 0 : 1];
    }
    if (at >= 0) { return 0; }
    return static_cast<std::size_t>(-(at + 1));
  }

  void BspLightVisibility::Mark(const std::span<const BspMomentLight> lights, std::vector<std::uint32_t> &masks) const
  {
    masks.assign(_face_count, 0);
    if (_leaves.size() < 2) { return; }

    // the leaves but leaf 0, one bit each, as the visibility counts them
    const std::size_t counted = _leaves.size() - 1;
    const std::size_t row = (counted + 7) / 8;

    for (const BspMomentLight &light : lights)
    {
      if (light.bit >= most_lights) { continue; }
      const std::uint32_t bit = 1u << light.bit;

      const std::size_t leaf = FindLeaf(light.place);
      if (leaf == 0) { continue; }

      // the leaves seen from there: all of them when the level does not say
      const std::int32_t offset = _leaves[leaf].visibility_offset;
      const bool sees_all = _visibility.empty() || offset < 0;
      _seen.assign(row, sees_all ? 0xFF : 0x00);
      if (!sees_all)
      {
        // Packed as the game packs it: a byte that is not 0 is eight bits
        // as they are, a 0 and a count is that many bytes of 0. Where the
        // visibility ends short, the rest is not seen.
        std::size_t in = static_cast<std::size_t>(offset);
        std::size_t out = 0;
        while (out < row && in < _visibility.size())
        {
          const std::uint8_t byte = _visibility[in++];
          if (byte != 0)
          {
            _seen[out++] = byte;
            continue;
          }
          if (in >= _visibility.size()) { break; }
          out += _visibility[in++];
        }
      }

      // the own leaf is always reached
      _seen[(leaf - 1) / 8] |= static_cast<std::uint8_t>(1u << ((leaf - 1) % 8));

      for (std::size_t other = 1; other < _leaves.size(); other++)
      {
        if ((_seen[(other - 1) / 8] & (1u << ((other - 1) % 8))) == 0) { continue; }

        const Leaf &room = _leaves[other];
        if (distance_to_box(light.place, room.mins, room.maxs) > light.radius) { continue; }

        for (std::uint32_t i = 0; i < room.face_count; i++) { masks[_leaf_faces[room.first_face + i]] |= bit; }
      }
    }
  }
} // quake
