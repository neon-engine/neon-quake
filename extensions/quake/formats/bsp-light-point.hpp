#ifndef QUAKE_BSP_LIGHT_POINT_HPP
#define QUAKE_BSP_LIGHT_POINT_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "bsp-file.hpp"
#include "bsp-light-face.hpp"
#include "bsp-light-filter.hpp"
#include "bsp-light-node.hpp"
#include "bsp-light-sample.hpp"
#include "bsp-vector.hpp"

namespace quake
{
  /// How bright a level is at a place, for lighting what is not the level:
  /// a monster, an item, the weapon in the hands of the player.
  ///
  /// The game does not light such a thing by the lights of the level. It
  /// gives it the light of the floor under it: it goes straight down from
  /// the place through the tree the world is drawn by, and reads the
  /// lightmap of the first face it meets, at the spot it meets it.
  ///
  /// It is made from a `BspFile` and keeps nothing of it: the forks, the
  /// faces, and the samples are copied. One that was made is whole: every
  /// fork, face, and sample it names is there, no fork is reached twice,
  /// and the tree is no deeper than `deepest_tree`. So asking never reads
  /// what is not there and always ends, whatever the file was and whatever
  /// the place is.
  ///
  /// Only the world, model 0, is looked at. A thing that stands on a lift or
  /// a door takes the light of the floor of the world under that, as in the
  /// original game. The lights that come and go while the game runs, as the
  /// flash of a shot, are not counted in either: they are no part of a level.
  ///
  /// Everything is in the units and axes of the game, with Z pointing up.
  class BspLightPoint
  {
    std::vector<BspLightNode> _nodes;

    /// Where the tree starts: a fork, or `BspLightNode::leaf` when the world
    /// is a single leaf.
    std::int32_t _head = BspLightNode::leaf;

    /// One for each face of the level, by its number there. A face that no
    /// fork of the world names is left as one that is never met.
    std::vector<BspLightFace> _faces;

    /// The samples of all lightmaps: one byte for each when the light is
    /// white, three when it is coloured.
    std::vector<std::uint8_t> _samples;
    std::size_t _bytes_for_each_sample = 1;

    /// Whether the level has any light. One that has none is fully bright
    /// everywhere.
    bool _has_light = false;

  public:
    /// How far down a face is looked for, as in the ports of today. The
    /// original looked 2048 units down, which the large levels made since
    /// are too high for.
    static constexpr float deepest_look = 8192.0f;

    /// How many forks deep a tree may be. Looking down goes as deep in calls
    /// of itself, so this keeps it from running out of stack. The levels of
    /// the game are far shallower.
    static constexpr std::size_t deepest_tree = 1024;

    /// How far under a face without a lightmap a face with one is still
    /// taken in its place.
    static constexpr float dark_face_slack = 8.0f;

    /// How many styles of light the game has. A face names a style by a
    /// number below this.
    static constexpr std::size_t style_count = 64;

    /// What `Sample` gives for each colour where the level has no light at
    /// all, and the most a single light gives.
    static constexpr float fully_bright = 255.0f;

    /// Takes what is needed from a level. `coloured` is the light of the
    /// level in colours, as a file `.lit` carries it after its header: three
    /// bytes, red, green, and blue, for each byte of the lighting of the
    /// level, in the same order. Without it the light is white, as the level
    /// itself has it.
    ///
    /// Returns false, says why in `error`, and stays as it was when the
    /// level has no world, when `coloured` is not three times the lighting,
    /// when a fork of the world names a plane, a fork, or a face that is not
    /// there or is reached twice, when the tree is too deep, or when a face
    /// on a fork names what is not there, has fewer than three corners, or
    /// has a lightmap that does not lie inside the lighting.
    bool Build(const BspFile &file, std::string &error, std::span<const std::uint8_t> coloured = {});

    /// Whether the level has any light. Where it has none, every place is
    /// `fully_bright` and no face is ever found.
    [[nodiscard]] bool HasLight() const
    {
      return _has_light;
    }

    /// The light under a place: of the first face that lies straight below
    /// it, no further than `deepest_look`.
    ///
    /// `styles` says how bright each style of light is just now, by its
    /// number: 1 is as bright as the level was lit, 0 is off, 2 is twice as
    /// bright. A style it does not have counts as 1, so that without any
    /// every light is on. The light is the sum, over the up to four styles
    /// of the face, of the sample of that style times how bright it is.
    ///
    /// The sky and the liquids are looked through, since they carry no
    /// light. A wall or floor without a lightmap is not: the tools that
    /// light a level leave the lightmap out where no light reaches, so such
    /// a face is found and is dark, as in the original game, and the light
    /// of a room under it does not shine through. Only a face with light
    /// that lies no more than `dark_face_slack` under it is taken in its
    /// place, as vkQuake does it: the game takes the whole rectangle of a
    /// lightmap for the face, so a dark face next to the place is met at
    /// times where the floor that is seen has light.
    ///
    /// The filter is `Nearest` unless asked otherwise, which is what the
    /// original game does.
    [[nodiscard]] BspLightSample Sample(
      const BspVector &point,
      std::span<const float> styles = {},
      BspLightFilter filter = BspLightFilter::Nearest) const;
  };
} // quake

#endif //QUAKE_BSP_LIGHT_POINT_HPP
