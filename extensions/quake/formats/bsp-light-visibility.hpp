#ifndef QUAKE_BSP_LIGHT_VISIBILITY_HPP
#define QUAKE_BSP_LIGHT_VISIBILITY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "bsp-file.hpp"
#include "bsp-light-node.hpp"
#include "bsp-moment-light.hpp"
#include "bsp-vector.hpp"

namespace quake
{
  /// Which faces of a level a light of a moment reaches: those of the rooms
  /// it can see into.
  ///
  /// The light of an explosion or a shot is a point light that the shaders
  /// add to every face within its reach, by distance alone. A wall stops
  /// no such light, so a room is lit by a fireball on the other side of
  /// its wall. The original marks the faces a light reaches by walking the
  /// tree of the level, and never reaches those of a room apart. This does
  /// the same by what the level knows of which leaves see which, its
  /// visibility: a light stands in a leaf, and reaches the faces of the
  /// leaves seen from there, no further than its radius.
  ///
  /// Each light has a bit, and each face gets a mask of the lights that
  /// reach it, which the shaders read, see `light_of_the_moment` in
  /// quake.glsl. It is made from a `BspFile` and keeps nothing of it:
  /// the forks of the world are copied, as are its leaves and their faces
  /// and the visibility. Only the world, model 0, is looked at: the faces
  /// of a door or a lift lie in no leaf and are left to whoever asks.
  ///
  /// Everything is in the units and axes of the game, with Z pointing up.
  class BspLightVisibility
  {
    /// A leaf as it is kept: which faces are its, and where its list of
    /// the leaves seen from it starts in the visibility, -1 for all.
    struct Leaf
    {
      std::int32_t visibility_offset = -1;
      std::array<float, 3> mins{};
      std::array<float, 3> maxs{};
      std::uint32_t first_face = 0;
      std::uint32_t face_count = 0;
    };

    std::vector<BspLightNode> _forks;

    /// Where the tree starts: a fork, or the leaf it is when the world is
    /// one leaf, as a negative number as the file has it.
    std::int32_t _head = BspLightNode::leaf;

    std::vector<Leaf> _leaves;
    std::vector<std::uint32_t> _leaf_faces;
    std::vector<std::uint8_t> _visibility;
    std::size_t _face_count = 0;

    // room for the leaves a light sees, kept so that asking allots nothing
    mutable std::vector<std::uint8_t> _seen;

  public:
    /// How many lights there are bits for in the mask of a face.
    static constexpr std::uint32_t most_lights = 32;

    /// Takes what is needed from a level. Returns false, says why in
    /// `error`, and stays as it was when the level has no world, or when a
    /// fork of the world names a plane, a fork, or a leaf that is not
    /// there, or when a leaf names a face that is not there.
    bool Build(const BspFile &file, std::string &error);

    /// How many faces the level has: how long the masks are.
    [[nodiscard]] std::size_t GetFaceCount() const
    {
      return _face_count;
    }

    /// The leaf a place lies in. Leaf 0 is everything solid.
    [[nodiscard]] std::size_t FindLeaf(const BspVector &place) const;

    /// The masks of the faces for these lights: one for each face of the
    /// level, by its number there, with the bit of each light that reaches
    /// it set. A light in the solid reaches nothing. In a level without
    /// visibility every light reaches every face of every leaf within its
    /// reach, as a face that lies in no leaf is reached by none here.
    void Mark(std::span<const BspMomentLight> lights, std::vector<std::uint32_t> &masks) const;
  };
} // quake

#endif //QUAKE_BSP_LIGHT_VISIBILITY_HPP
